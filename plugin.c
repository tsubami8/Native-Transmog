// MHGU Recomp: layered appearance (cosmetic armor) with a native menu entry.
#include <stddef.h>
#include <stdint.h>

extern int forge_log(int level, const char *fmt, ...);
#define LOG(...) forge_log(1, __VA_ARGS__)
extern uint32_t forge_mem_getMainTextAddr(void);
extern int forge_fs_mountSaveData(void);
extern void forge_fs_unmountSaveData(void);
extern int forge_fs_fileExists(const char *path);
extern int forge_fs_openFile(const char *path, int mode, uint32_t *out);
extern int forge_fs_getFileSize(uint32_t handle, int64_t *out);
extern int forge_fs_readFile(uint32_t handle, int64_t off, void *buf, uint32_t size);
extern int forge_fs_closeFile(uint32_t handle);
extern int forge_input_isPressed(uint32_t index);
extern uint32_t sdk_create(const char *, int64_t) __asm__("_ZN2nn2fs10CreateFileEPKcx");
extern uint32_t sdk_write(uint32_t, int64_t, const void *, uint32_t, const uint32_t *)
    __asm__("_ZN2nn2fs9WriteFileENS0_10FileHandleExPKvjRKNS0_11WriteOptionE");
extern uint32_t sdk_commit(const char *) __asm__("_ZN2nn2fs14CommitSaveDataEPKc");


#define LD32(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define ST32(a, v) (*(volatile uint32_t *)(uintptr_t)(a) = (uint32_t)(v))
#define LD16(a) (*(volatile uint16_t *)(uintptr_t)(a))
#define ST16(a, v) (*(volatile uint16_t *)(uintptr_t)(a) = (uint16_t)(v))
#define LD8(a) (*(volatile uint8_t *)(uintptr_t)(a))
#define ST8(a, v) (*(volatile uint8_t *)(uintptr_t)(a) = (uint8_t)(v))

typedef struct { uint32_t words[6]; } ForgeHook;
extern ForgeHook forge_hook_create(void *target, void *detour, void **original);

#define G_EQUIP_BOX 0x1a56fe8u
#define G_PLAYER_MANAGER 0x1a85b58u
#define G_SESSION 0x1a8ca48u
#define BOX_RECORDS 0x1cu
#define REC_STRIDE 0x2cu
#define ENTRY_OFF 0x4u
#define APP_OFF 0x4u
#define PLAYER_EQUIP 0x84u
#define MAX_IDX 2000u

void *memcpy(void *d, const void *s, size_t n) {
    uint8_t *p = d;
    const uint8_t *q = s;
    while (n--) *p++ = *q++;
    return d;
}
void *memset(void *d, int v, size_t n) {
    uint8_t *p = d;
    while (n--) *p++ = (uint8_t)v;
    return d;
}
int memcmp(const void *a, const void *b, size_t n) {
    const uint8_t *p = a, *q = b;
    while (n--) { if (*p != *q) return *p - *q; p++; q++; }
    return 0;
}

static uint16_t cosmetic[5];
static uint8_t hunter_refresh;
static void mark_current(void);

static char g_our_label[] = "\xe5\xa4\x96\xe8\xa7\x82\xe8\xa3\x85\xe5\xa4\x87";  // 外观装备
static char g_our_desc[] = "\xe8\xae\xb0\xe5\xbd\x95\xe5\xbd\x93\xe5\x89\x8d\xe7\xa9\xbf\xe7\x9d\x80\xe7\x9a\x84"
                           "\xe5\xa4\x96\xe8\xa7\x82\xef\xbc\x8c\xe6\x8d\xa2\xe8\xa3\x85\xe5\x90\x8e\xe4\xbb\x8d\xe4\xbf\x9d\xe6\x8c\x81";
static const char register_appearance_desc[] = "\xe8\xaf\xb7\xe9\x80\x89\xe6\x8b\xa9\xe7\x99\xbb\xe8\xae\xb0\xe4\xbd\x8d\xe7\xbd\xae\xe3\x80\x82";
#define REGISTER_APPEARANCE_MESSAGE 0x7ff2u
#define KINSECT_APPEARANCE_MESSAGE 0x7ff3u
static const char kinsect_desc[] = "\xe8\xaf\xb7\xe9\x80\x89\xe6\x8b\xa9\xe8\xa6\x81\xe6\x98\xbe\xe7\xa4\xba\xe7\x9a\x84\xe7\x8c\x8e\xe8\x99\xab\xe5\xa4\x96\xe8\xa7\x82\xe3\x80\x82";

#define WEAPON_BOX_MESSAGE 0x7ff4u
static const char *const weapon_box_descriptions[] = {
    "\xe4\xbb\x8e\xe8\xa3\x85\xe5\xa4\x87\xe7\xae\xb1\xe9\x80\x89\xe6\x8b\xa9\xe4\xb8\x8e\xe5\xbd\x93\xe5\x89\x8d\xe6\xad\xa6\xe5\x99\xa8\xe5\x90\x8c\xe7\xa7\x8d\xe7\xb1\xbb\xe7\x9a\x84\xe6\xad\xa6\xe5\x99\xa8\xe4\xbd\x9c\xe4\xb8\xba\xe5\xa4\x96\xe8\xa7\x82\xe3\x80\x82",
    "\xe6\x81\xa2\xe5\xa4\x8d\xe5\xbd\x93\xe5\x89\x8d\xe6\xad\xa6\xe5\x99\xa8\xe7\x9a\x84\xe5\x8e\x9f\xe7\x94\x9f\xe5\xa4\x96\xe8\xa7\x82\xe3\x80\x82",
    "\xe4\xbb\x8e\xe5\x90\x8d\xe5\x8d\x95\xe9\x80\x89\xe6\x8b\xa9\xe8\xa6\x81\xe6\x98\xbe\xe7\xa4\xba\xe7\x9a\x84\xe7\x8c\x8e\xe8\x99\xab\xe5\xa4\x96\xe8\xa7\x82\xe3\x80\x82",
    "\xe6\x81\xa2\xe5\xa4\x8d\xe5\xbd\x93\xe5\x89\x8d\xe7\x8c\x8e\xe8\x99\xab\xe7\x9a\x84\xe5\x8e\x9f\xe7\x94\x9f\xe5\xa4\x96\xe8\xa7\x82\xe3\x80\x82",
    "\xe8\xa3\x85\xe5\xa4\x87\xe6\x93\x8d\xe8\x99\xab\xe6\xa3\x8d\xe5\x90\x8e\xe5\x8f\xaf\xe4\xbd\xbf\xe7\x94\xa8\xe6\xad\xa4\xe9\x80\x89\xe9\xa1\xb9\xe3\x80\x82",
};

static int key_match(const char *p, const char *key, int len) {
    for (int i = 0; i < len; i++) if (p[i] != key[i]) return 0;
    return 1;
}

static void parse(const char *buf, int sz) {
    static const char *keys[5] = {"head", "chest", "arms", "waist", "legs"};
    static const int lens[5] = {4, 5, 4, 5, 4};
    for (int k = 0; k < 5; k++) {
        for (int i = 0; i < sz; i++) {
            if (i && buf[i - 1] != '\n' && buf[i - 1] != '\r') continue;
            if (sz - i <= lens[k] || !key_match(buf + i, keys[k], lens[k])) continue;
            const char *q = buf + i + lens[k];
            while (*q == ' ' || *q == '\t') q++;
            if (*q != '=') continue;
            q++;
            while (*q == ' ' || *q == '\t') q++;
            uint32_t v = 0;
            int hex = 0;
            if (q[0] == '0' && (q[1] == 'x' || q[1] == 'X')) { q += 2; hex = 1; }
            for (;;) {
                int c = *q, d;
                if (c >= '0' && c <= '9') d = c - '0';
                else if (hex && c >= 'a' && c <= 'f') d = c - 'a' + 10;
                else if (hex && c >= 'A' && c <= 'F') d = c - 'A' + 10;
                else break;
                v = v * (hex ? 16u : 10u) + (uint32_t)d;
                if (v > UINT16_MAX) break;
                q++;
            }
            if (v <= UINT16_MAX) cosmetic[k] = (uint16_t)v;
            break;
        }
    }
}

static void read_config(void) {
    if (!forge_fs_mountSaveData()) return;
    uint32_t h = 0;
    int64_t sz = 0;
    char buf[512];
    if (forge_fs_openFile("savedata:/layered.ini", 1, &h)) {
        if (forge_fs_getFileSize(h, &sz) && sz > 0 && sz < (int64_t)sizeof buf &&
            forge_fs_readFile(h, 0, buf, (uint32_t)sz)) {
            buf[sz] = 0;
            parse(buf, (int)sz);
        }
        forge_fs_closeFile(h);
    }
    forge_fs_unmountSaveData();
}

static int snapshot_current(uint16_t ids[5]) {
    uint32_t player = LD32(G_PLAYER_MANAGER);
    uint32_t box = LD32(G_EQUIP_BOX);
    if (!player || !box) return 0;
    for (unsigned i = 0; i < 5; i++) {
        uint32_t index = LD16(player + PLAYER_EQUIP + 2 * (i + 1));
        uint16_t id = cosmetic[i];
        if (!id && index < MAX_IDX) {
            uint32_t record = box + BOX_RECORDS + index * REC_STRIDE + ENTRY_OFF;
            id = LD16(record + APP_OFF);
            if (!id) id = LD16(record + 2);
        }
        ids[i] = id;
    }
    return 1;
}

// ---------------------------------------------------------------- start menu row
#define LOBBY_VTABLE 0x0197e424u
#define QUEST_VTABLE 0x0197aeb8u
#define ALLOC_GET 0x9aa47cu
#define ALLOC_GOT 0x1a38b5cu
#define DESC_MAKE 0x74f9c0u
#define DESC_RESIZE 0x825138u
#define SET_TOTAL 0x74ee34u

static uint32_t g_our_index = 0xffffffffu;
static uint32_t (*g_init)(uint32_t);
static uint32_t (*g_fill)(uint32_t);
static uint32_t (*g_sel)(uint32_t, uint32_t);
static uint32_t (*g_update)(uint32_t);
static uint32_t (*g_msg)(uint32_t, uint32_t, uint32_t, uint32_t);

static int is_menu(uint32_t owner) {
    if (!owner) return 0;
    uint32_t vt = LD32(owner);
    return vt == LOBBY_VTABLE || vt == QUEST_VTABLE;
}

static uint32_t init_detour(uint32_t self) {
    uint32_t r = g_init(self);
    if (!is_menu(self)) return r;
    uint32_t count = LD32(self + 0xac), names = LD32(self + 0xa8);
    if (!names || count == 0 || count > 64) return r;
    if (g_our_index == 0xffffffffu || count <= g_our_index) g_our_index = count;
    uint32_t n = g_our_index + 1;
    if (count < n) {
        uint32_t desc = LD32(ALLOC_GOT);
        if (!desc) return r;
        uint32_t a = ((uint32_t (*)(uint32_t))ALLOC_GET)(desc);
        if (!a || !LD32(a)) return r;
        uint32_t avt = LD32(a);
        uint32_t (*alloc)(uint32_t, uint32_t, uint32_t) = (void *)(uintptr_t)LD32(avt + 0x1c);
        uint32_t fresh = alloc(a, n * 8, 0x10);
        if (!fresh) return r;
        memcpy((void *)(uintptr_t)fresh, (void *)(uintptr_t)names, count * 8);
        for (uint32_t i = count; i < n; i++) {
            *(volatile uint8_t *)(uintptr_t)(fresh + i * 8) = 0;
            ST32(fresh + i * 8 + 4, (uint32_t)(uintptr_t)g_our_label);
        }
        ST32(self + 0xa8, fresh);
        ST32(self + 0xac, n);
        if (count == 16) {
            void (*free_)(uint32_t, uint32_t) = (void *)(uintptr_t)LD32(avt + 0x34);
            free_(a, names);
        }
        LOG("[layered] menu names grown %u -> %u\n", (unsigned)count, (unsigned)n);
    }
    if (LD32(self + 0xc4) < n) {
        uint32_t tmpl[3] = {0, 0, 0};
        ((void (*)(uint32_t))DESC_MAKE)((uint32_t)(uintptr_t)tmpl);
        ((uint32_t (*)(uint32_t, uint32_t, uint32_t))DESC_RESIZE)(self + 0xc0, n, (uint32_t)(uintptr_t)tmpl);
    }
    if (LD32(self + 0xc4) >= n) {
        uint32_t list = self + 0xd8;
        if (LD16(list + 0x14) < n)
            ((uint32_t (*)(uint32_t, uint32_t, uint32_t))SET_TOTAL)(list, n, LD32(list + 0x1c));
    }
    return r;
}

static uint32_t fill_detour(uint32_t self) {
    uint32_t r = g_fill(self);
    if (!is_menu(self) || g_our_index == 0xffffffffu) return r;
    uint32_t names = LD32(self + 0xa8), descs = LD32(self + 0xc0);
    uint32_t count = LD32(self + 0xac), dcount = LD32(self + 0xc4);
    if (!names || !descs || count <= g_our_index || dcount <= g_our_index) return r;
    uint32_t e = names + g_our_index * 8;
    *(volatile uint8_t *)(uintptr_t)e = 1;
    ST32(e + 4, (uint32_t)(uintptr_t)g_our_label);
    uint32_t d = descs + g_our_index * 12;
    *(volatile uint8_t *)(uintptr_t)(d + 4) = 0;
    *(volatile uint16_t *)(uintptr_t)(d + 6) = 9;
    *(volatile uint16_t *)(uintptr_t)(d + 8) = 0x7ff1;
    *(volatile uint16_t *)(uintptr_t)(d + 10) = 0xffff;
    return r;
}

static uint32_t sel_detour(uint32_t self, uint32_t index) {
    if (g_our_index != 0xffffffffu && index == g_our_index && is_menu(self)) return 0;
    return g_sel(self, index);
}

static uint32_t update_detour(uint32_t owner) {
    if (is_menu(owner) && g_our_index != 0xffffffffu) {
        uint32_t list = owner + 0xd8;
        uint32_t page = LD16(list + 0x10);
        uint32_t rows = LD8(list + 0xe);
        uint32_t cur = LD8(list + 0xd);
        int hot = LD32(owner + 0x48) == 0 && page * rows + cur == g_our_index;
        static int prev_a, was_hot;
        int a = forge_input_isPressed(0);
        if (hot && !was_hot) LOG("[layered] cursor on our row (index %u)\n", (unsigned)g_our_index);
        if (hot && a && !prev_a) mark_current();
        prev_a = a;
        was_hot = hot;
    }
    return g_update(owner);
}

static uint32_t msg_detour(uint32_t mgr, uint32_t kind, uint32_t file, uint32_t id) {
    if (kind == 0 && file == 9 && id == 0x7ff1) return (uint32_t)(uintptr_t)g_our_desc;
    if (kind == 1 && file == 0x16 && id == REGISTER_APPEARANCE_MESSAGE)
        return (uint32_t)(uintptr_t)register_appearance_desc;
    if (kind == 1 && file == 0x16 && id == KINSECT_APPEARANCE_MESSAGE)
        return (uint32_t)(uintptr_t)kinsect_desc;
    if (kind == 1 && file == 0x16 && id >= WEAPON_BOX_MESSAGE &&
        id - WEAPON_BOX_MESSAGE < sizeof weapon_box_descriptions / sizeof *weapon_box_descriptions)
        return (uint32_t)(uintptr_t)weapon_box_descriptions[id - WEAPON_BOX_MESSAGE];
    return g_msg(mgr, kind, file, id);
}

void forge_onLoad(void *params) {
    uint16_t *v = params;
    v[0] = 1; v[1] = 5; v[2] = 1;
    static const char name[32] = "Layered Armor";
    char *dst = (char *)params + 6;
    for (unsigned i = 0; i < 32; i++) dst[i] = name[i];
}

#define INPUT_MANAGER 0x1a8c670u
#define INPUT_PRESSED 0x6bfcu
#define INPUT_REPEAT 0x6c34u
#define BUTTON_A 0x2000u
#define BUTTON_B 0x4000u
#define BUTTON_ZL 0x400u
#define BUTTON_ZR 0x800u
#define DPAD_LEFT 0x80u
#define DPAD_RIGHT 0x20u

enum { SND_CONFIRM = 1, SND_CANCEL = 2, SND_ERROR = 3, SND_DIRECT = 6 };
enum { SND_MOVE_PICK = 0x0f, SND_MOVE_DONE = 0x10, SND_REMOVE_PROMPT = 0x15 };
enum { SND_EQUIP = 0x65, SND_REGISTER = 0x89 };
static unsigned quiet_menu;
static void (*g_menu_sound)(uint32_t, uint32_t);

static void menu_sound(uint32_t bank, uint32_t effect) {
    if (!quiet_menu && g_menu_sound) g_menu_sound(bank, effect);
}

static void menu_sound_detour(uint32_t bank, uint32_t effect) {
    menu_sound(bank, effect);
}

static uint32_t input_address(void) {
    uint32_t manager = LD32(INPUT_MANAGER);
    return manager ? manager + INPUT_PRESSED : 0;
}

static uint32_t with_button(uint32_t (*update)(uint32_t), uint32_t ui, uint32_t input,
                            uint32_t button, int quiet) {
    uint32_t pressed = LD32(input);
    const uint32_t mask = BUTTON_A | BUTTON_B;
    ST32(input, (pressed & ~mask) | button);
    quiet_menu += quiet;
    uint32_t result = update(ui);
    quiet_menu -= quiet;
    ST32(input, (LD32(input) & ~mask) | (pressed & mask));
    return result;
}

static uint32_t update_paged_menu(uint32_t (*update)(uint32_t), uint32_t ui, uint32_t input) {
    uint32_t pressed = input ? LD32(input) : 0;
    uint32_t direction = 0;
    if (pressed & BUTTON_ZL) direction |= DPAD_LEFT;
    if (pressed & BUTTON_ZR) direction |= DPAD_RIGHT;
    if (!direction || (pressed & (BUTTON_A | BUTTON_B))) return update(ui);
    // Keep the native selector's D-pad navigation and accept the displayed page buttons.
    ST32(input, pressed | direction);
    uint32_t result = update(ui);
    ST32(input, (LD32(input) & ~direction) | (pressed & direction));
    return result;
}

static int load_file(const char *path, void *data, uint32_t size) {
    if (!forge_fs_mountSaveData()) return 0;
    uint32_t handle;
    int64_t length;
    int loaded = 0;
    if (forge_fs_fileExists(path) && forge_fs_openFile(path, 1, &handle)) {
        loaded = forge_fs_getFileSize(handle, &length) && length == size &&
                 forge_fs_readFile(handle, 0, data, size);
        forge_fs_closeFile(handle);
    }
    forge_fs_unmountSaveData();
    return loaded;
}

// A fixed-size file lets us update one record and preserve all other records.
static int save_record(const char *path, const void *data, uint32_t size,
                       uint32_t offset, uint32_t length) {
    if (!forge_fs_mountSaveData()) return 0;
    int exists = forge_fs_fileExists(path);
    uint32_t handle;
    int saved = 0;
    if (!exists) {
        if (sdk_create(path, size) != 0) goto done;
        offset = 0;
        length = size;
    }
    if (forge_fs_openFile(path, 2, &handle)) {
        int64_t current_size;
        const uint32_t flush = 1;
        saved = forge_fs_getFileSize(handle, &current_size) && current_size == size &&
                sdk_write(handle, offset, (const char *)data + offset, length, &flush) == 0;
        forge_fs_closeFile(handle);
    }
    if (saved) saved = sdk_commit("savedata") == 0;
done:
    forge_fs_unmountSaveData();
    return saved;
}

// The MOD owns these choices. Native equipment and fusion entries are read-only.
static const char hunter_path[] = "savedata:/layered_hunter.bin";
static const char hunter_restore[] = "\xe6\x81\xa2\xe5\xa4\x8d\xe5\x8e\x9f\xe7\x94\x9f\xe5\xa4\x96\xe8\xa7\x82";
static const uint32_t hunter_parts[] = {4, 0, 1, 2, 3};

static int set_current(const uint16_t ids[5]) {
    if (!save_record(hunter_path, ids, sizeof cosmetic, 0, sizeof cosmetic)) {
        LOG("[layered] hunter appearance save failed\n");
        menu_sound(0, SND_ERROR);
        return 0;
    }
    for (unsigned i = 0; i < 5; i++) {
        if (ids[i] != cosmetic[i]) hunter_refresh |= (uint8_t)(1u << (i + 1));
    }
    memcpy(cosmetic, ids, sizeof cosmetic);
    return 1;
}

static void mark_current(void) {
    uint16_t ids[5];
    if (snapshot_current(ids)) set_current(ids);
}

static int is_local_hunter(uint32_t actor) {
    if (LD8(actor + 0x13cc) & 8 || LD32(actor + 0x13c8) > 1) return 0;
    uint32_t manager = LD32(G_PLAYER_MANAGER);
    uint32_t session = LD32(G_SESSION);
    if (!manager || !session) return 0;
    uint32_t index = LD32(session + 0x30);
    return index < 4 && LD32(manager + 0x1c + 4 * index) == actor;
}

static uint32_t gear_catalog(void) {
    return LD32(0x1a56b18u);
}

static uint32_t equipment_model(uint32_t item) {
    uint32_t get_model = LD32(LD32(item) + 0x30);
    return ((uint32_t (*)(uint32_t))get_model)(item);
}

static uint32_t hunter_item(uint32_t type, uint16_t id) {
    uint32_t catalog = gear_catalog();
    return catalog ? ((uint32_t (*)(uint32_t, uint32_t, uint32_t))0x33c208u)(catalog, type, id) : 0;
}

#define INSECT_GLAIVE_TYPE 20u
#define WEAPON_PART 7u
#define SECOND_WEAPON_PART 8u
#define KINSECT_COUNT 31u
#define HUNTER_PROFILE_COUNT 3u
#define G_SAVE_MANAGER 0x1a8c76cu

typedef struct {
    uint32_t equipped_key;
    uint16_t weapon_id;
    uint16_t kinsect_id;
} WeaponAppearance;

static WeaponAppearance weapon_appearances[HUNTER_PROFILE_COUNT];
static const char weapon_appearance_path[] = "savedata:/layered_weapon.bin";
static int weapon_save_pending;
static unsigned weapon_save_delay;

static int is_weapon_type(unsigned type) {
    return type >= 7 && type <= 21 && type != 12;
}

static uint32_t equipped_weapon_key(void) {
    uint32_t player = LD32(G_PLAYER_MANAGER);
    uint32_t box = LD32(G_EQUIP_BOX);
    if (!player || !box) return 0;
    uint32_t index = LD16(player + PLAYER_EQUIP);
    if (index >= MAX_IDX) return 0;
    uint32_t record = box + BOX_RECORDS + index * REC_STRIDE + ENTRY_OFF;
    unsigned type = LD16(record) & 0x1f;
    uint16_t id = LD16(record + 2);
    // The slot distinguishes duplicate weapons. Level, dyes and the attached
    // kinsect are deliberately excluded so native kinsect exchanges keep the look.
    return is_weapon_type(type) && id ? ((uint32_t)id << 16) | (type << 11) | (index + 1) : 0;
}

static unsigned weapon_key_type(uint32_t key) {
    return (key >> 11) & 0x1f;
}

static WeaponAppearance *current_weapon_appearance(void) {
    uint32_t manager = LD32(G_SAVE_MANAGER);
    if (!manager) return 0;
    unsigned profile = LD32(manager + 64);
    return profile < HUNTER_PROFILE_COUNT ? &weapon_appearances[profile] : 0;
}

static int save_weapon_appearances(void) {
    if (save_record(weapon_appearance_path, weapon_appearances, sizeof weapon_appearances,
                    0, sizeof weapon_appearances)) {
        weapon_save_pending = 0;
        return 1;
    }
    LOG("[layered] weapon appearance save failed\n");
    return 0;
}

static WeaponAppearance *sync_weapon_appearance(uint32_t key) {
    WeaponAppearance *appearance = current_weapon_appearance();
    if (appearance && appearance->equipped_key && appearance->equipped_key != key) {
        *appearance = (WeaponAppearance){0};
        weapon_save_pending = 1;
        LOG("[layered] cleared weapon and kinsect appearances after equipment change\n");
    }
    return appearance;
}

static int set_weapon_appearance(uint32_t key, uint16_t id, int kinsect) {
    WeaponAppearance *appearance = sync_weapon_appearance(key);
    if (!appearance || !key) return 0;
    WeaponAppearance previous = *appearance;
    appearance->equipped_key = key;
    if (kinsect) appearance->kinsect_id = id;
    else appearance->weapon_id = id;
    if (!appearance->weapon_id && !appearance->kinsect_id) appearance->equipped_key = 0;
    if (!save_weapon_appearances()) {
        *appearance = previous;
        return 0;
    }
    hunter_refresh |= 1;
    return 1;
}

static uint32_t kinsect_data(unsigned id) {
    uint32_t catalog = gear_catalog();
    if (!catalog || !id || id > KINSECT_COUNT) return 0;
    return ((uint32_t (*)(uint32_t, uint32_t))0x33f518u)(catalog, id);
}

static const char *kinsect_name(unsigned id) {
    uint32_t entry = kinsect_data(id);
    uint32_t messages = LD32(0x1a56b64u);
    if (!entry || !messages) return "";
    const char *name = ((const char *(*)(uint32_t, uint32_t))0xd84114u)(messages, LD32(entry + 0x34));
    return name ? name : "";
}

static int weapon_has_second_part(unsigned type) {
    return type > 7 && type <= 21 && (0x3745u & (1u << (type - 8)));
}

static uint32_t weapon_model(uint32_t actor, uint32_t part, uint32_t stock) {
    uint32_t key = equipped_weapon_key();
    WeaponAppearance *appearance = current_weapon_appearance();
    if (!key || !appearance || appearance->equipped_key != key) return stock;
    unsigned type = weapon_key_type(key);
    uint32_t equip = LD32(actor + 0x13c4);
    if (!equip || LD8(equip + 0x4d4) != type - 7) return stock;
    if (part == SECOND_WEAPON_PART && type == INSECT_GLAIVE_TYPE) {
        uint32_t entry = kinsect_data(appearance->kinsect_id);
        return entry ? LD32(entry + 0xc) : stock;
    }
    uint32_t item = appearance->weapon_id ? hunter_item(type, appearance->weapon_id) : 0;
    return item ? equipment_model(item) : stock;
}

static int model_idle(uint32_t actor, uint32_t part) {
    uint32_t object = LD32(actor + 0x2384 + 4 * part);
    return object && LD32(object + 0x1378) <= 1;
}

// ---------------------------------------------------------------- Palico board
#define BOARD_VTABLE 0x197e024u
#define BOARD_PANEL_VTABLE 0x1971188u
#define BOARD_SEL 0x68u
#define BOARD_STATE 0x3cu
#define BOARD_READY 0x50u
#define BOARD_GRID_STATE 0x4cu
#define CAT_MANAGER 0x1a85468u
#define CAT_RECORDS 0x13938u
#define CAT_STRIDE 0x560u
#define CAT_COUNT 84u
#define CAT_BOX_RECORDS 0x157dcu
#define CAT_MODEL_NONE 0xffffffffu

static const char cat_path[] = "savedata:/layered_palicoes.bin";

typedef struct {
    uint32_t id[2];
    uint32_t model[3];
} CatAppearance;

static CatAppearance cat_looks[CAT_COUNT];
static uint8_t cat_refresh[CAT_COUNT];
static const uint32_t cat_parts[] = {7, 4, 0};
static uint32_t board_capture;
static int board_drive;
static int board_return;
static uint32_t (*g_board_update)(uint32_t);
static uint32_t (*g_board_grid)(uint32_t);
static uint32_t (*g_model_request)(uint32_t, uint32_t, uint32_t);
static uint32_t (*g_equip_update)(uint32_t);

static int cat_slot(uint32_t manager, uint32_t record) {
    uint32_t start = manager + CAT_RECORDS;
    if (record < start || record >= start + CAT_COUNT * CAT_STRIDE) return -1;
    uint32_t offset = record - start;
    return offset % CAT_STRIDE ? -1 : (int)(offset / CAT_STRIDE);
}

static int local_cat_slot(uint32_t actor) {
    if ((LD8(actor + 0x13cc) & 0x0a) != 0x0a) return -1;
    uint32_t manager = LD32(CAT_MANAGER);
    if (!manager) return -1;
    for (uint32_t i = 0; i < 4; i++) {
        if (LD32(manager + 0x1c + i * 4) != actor) continue;
        int slot = cat_slot(manager, LD32(actor + 0x13c4));
        if (slot < 0) return -1;
        uint32_t record = manager + CAT_RECORDS + (uint32_t)slot * CAT_STRIDE;
        const CatAppearance *look = &cat_looks[slot];
        if (!(look->id[0] | look->id[1]) || !LD8(record + 629) ||
            look->id[0] != LD32(record + 632) || look->id[1] != LD32(record + 636))
            return -1;
        return slot;
    }
    return -1;
}

static uint32_t model_request_detour(uint32_t actor, uint32_t part, uint32_t model) {
    int slot = local_cat_slot(actor);
    if (slot >= 0) {
        for (uint32_t i = 0; i < 3; i++) {
            if (part == cat_parts[i] && cat_looks[slot].model[i] <= UINT16_MAX) {
                model = cat_looks[slot].model[i];
                break;
            }
        }
    } else if (is_local_hunter(actor)) {
        if (part <= 4) {
            for (unsigned i = 0; i < 5; i++) {
                if (hunter_parts[i] == part && cosmetic[i]) {
                    uint32_t item = hunter_item(i + 1, cosmetic[i]);
                    if (item) model = equipment_model(item);
                    break;
                }
            }
        } else if ((part == WEAPON_PART || part == SECOND_WEAPON_PART) && model != UINT32_MAX) {
            model = weapon_model(actor, part, model);
        }
    }
    return g_model_request(actor, part, model);
}

static uint32_t equip_update_detour(uint32_t actor) {
    if (LD32(actor + 0x13c8) == 1) {
        uint32_t ready = 0;
        int slot = local_cat_slot(actor);
        if (slot >= 0 && cat_refresh[slot]) {
            for (unsigned i = 0; i < 3; i++) {
                uint32_t model = cat_looks[slot].model[i];
                if ((cat_refresh[slot] & (1u << i)) && model <= UINT16_MAX &&
                    model_idle(actor, cat_parts[i]) && g_model_request(actor, cat_parts[i], model)) {
                    cat_refresh[slot] &= (uint8_t)~(1u << i);
                }
            }
        } else if (is_local_hunter(actor)) {
            uint32_t key = equipped_weapon_key();
            sync_weapon_appearance(key);
            if ((hunter_refresh & 1) && key && model_idle(actor, WEAPON_PART) &&
                (!weapon_has_second_part(weapon_key_type(key)) || model_idle(actor, SECOND_WEAPON_PART)))
                ready |= 1;
            for (unsigned i = 0; i < 5; i++) {
                if ((hunter_refresh & (2u << i)) && model_idle(actor, hunter_parts[i]))
                    ready |= 2u << i;
            }
            hunter_refresh &= (uint8_t)~ready;
        }
        if (ready) ST32(actor + 0x2480, LD32(actor + 0x2480) | ready);
    }
    return g_equip_update(actor);
}

static uint32_t selected_index(uint32_t selector) {
    return LD16(selector + 16) * LD8(selector + 14) + LD8(selector + 13);
}

static int cat_capture(uint32_t ui) {
    uint32_t manager = LD32(CAT_MANAGER);
    uint32_t box = LD32(G_EQUIP_BOX);
    if (!manager || !box) return 0;
    uint32_t row = selected_index(ui + 0x11c);
    uint32_t part = selected_index(ui + 0x140);
    if (row >= CAT_COUNT || part >= 3) return 0;
    uint32_t order = ((uint32_t (*)(uint32_t, uint32_t))0x460440u)(manager, row);
    uint32_t cat = ((uint32_t (*)(uint32_t, uint32_t))0x4595e4u)(manager, order);
    int slot = cat_slot(manager, cat);
    if (slot < 0 || !LD8(cat + 629)) return 0;

    // The last grid row also contains empty cells and the remove-equipment action.
    uint32_t columns = LD8(ui + 0x19e);
    uint32_t rows = LD8(ui + 0x19f);
    uint32_t index = columns * (rows * LD16(ui + 0x1a0) + LD8(ui + 0x19d)) + LD8(ui + 0x19c);
    if (!((int (*)(uint32_t, uint32_t))0x344a48u)(box, index)) return 0;
    uint32_t record = box + CAT_BOX_RECORDS + index * REC_STRIDE;
    if ((LD16(record + 4) & 0x1fu) != 0x16 + part) return 0;

    uint32_t catalog = gear_catalog();
    if (!catalog) return 0;
    uint32_t lookup = part == 0 ? 0x33f03cu : 0x33f060u;
    uint32_t item = ((uint32_t (*)(uint32_t, uint32_t))lookup)(catalog, record);
    if (!item) return 0;
    uint32_t model = equipment_model(item);
    if (model > UINT16_MAX) return 0;
    CatAppearance next[CAT_COUNT];
    memcpy(next, cat_looks, sizeof next);
    CatAppearance *look = &next[slot];
    uint32_t lo = LD32(cat + 632), hi = LD32(cat + 636);
    if (!(lo | hi)) return 0;
    if (look->id[0] != lo || look->id[1] != hi) {
        look->id[0] = lo;
        look->id[1] = hi;
        for (unsigned i = 0; i < 3; i++) look->model[i] = CAT_MODEL_NONE;
    }
    look->model[part] = model;
    if (!save_record(cat_path, next, sizeof next,
                     (uint32_t)slot * sizeof *look, sizeof *look)) {
        LOG("[layered] Palico appearance save failed\n");
        menu_sound(0, SND_ERROR);
        return 1;
    }
    cat_looks[slot] = *look;
    cat_refresh[slot] |= (uint8_t)(1u << part);
    menu_sound(1, SND_EQUIP);
    LOG("[layered] Palico slot %d part %u model %u\n", slot, part, model);
    return 1;
}

static uint32_t board_grid_detour(uint32_t ui) {
    uint32_t input = input_address();
    if (ui == board_capture && LD32(ui + 0x48) == 6 &&
        LD32(ui + BOARD_GRID_STATE) == 5 && input &&
        (LD32(input) & (BUTTON_A | BUTTON_B)) == BUTTON_A) {
        if (!cat_capture(ui)) {
            menu_sound(0, SND_ERROR);
        }
        return 1;
    }
    return g_board_grid(ui);
}

// ---------------------------------------------------------------- item box row
#define BOX_SEL 300u
#define FN_BOX_UPDATE 0x913db8u
#define BOX_NODES 0x18328f0u
#define FN_VISIBLE 0x7c5500u
#define FN_LABEL 0x76ea08u
#define FN_PAINT 0x76e6fcu
#define FN_COLOR 0x750354u
#define FN_SEL_START 0x74edecu
#define FN_SEL_COUNT 0x74ee34u
#define PAGE_INPUT_DPAD 3u

static uint32_t g_box_ui;
static uint32_t g_box_panel;
static const char g_box_title[] = "\xe9\x81\x93\xe5\x85\xb7\xe7\xae\xb1";

static void box_set_visible(uint32_t panel, uint32_t rows) {
    if (LD32(panel + 0x64) == rows) return;
    void (*vis)(uint32_t, uint32_t) = (void (*)(uint32_t, uint32_t))FN_VISIBLE;
    uint32_t cap = LD32(panel + 0x5c);
    ST32(panel + 0x5c, 9);
    vis(panel, rows);
    ST32(panel + 0x5c, cap);
}

static void box_draw_row(uint32_t panel, uint32_t row, const char *label, int focused, int enabled) {
    uint32_t node = LD32(BOX_NODES + 4 * row);
    ((void (*)(uint32_t, uint32_t, uint32_t, uint32_t))FN_LABEL)(panel, node, 1, (uint32_t)(uintptr_t)label);
    uint32_t col = ((uint32_t (*)(uint32_t, uint32_t, uint32_t, uint32_t))FN_COLOR)(enabled, focused != 0, 0, 0);
    ((void (*)(uint32_t, uint32_t, uint32_t, uint32_t))FN_PAINT)(panel, node, 1, col);
}

static void set_menu_title(uint32_t panel, const char *title) {
    ((void (*)(uint32_t, uint32_t, uint32_t, const char *))FN_LABEL)(panel, 14, 3, title);
}

typedef struct {
    uint32_t selector[9];
    unsigned page;
    unsigned flash;
    int left;
} PageIndicator;
static PageIndicator box_indicator, board_indicator;

#define PAGE_COUNTER_VTABLE 0x1973efcu
#define PAGE_COUNTER_TYPE 0x1a3f23cu
#define PAUSE_HELP_LAYOUT 0xde57u
typedef void (*NodePosition)(uint32_t, uint32_t, float) __attribute__((pcs("aapcs-vfp")));

static uint32_t page_counter(uint32_t panel, uint32_t selector) {
    for (uint32_t child = LD32(panel + 0xc); child; child = LD32(child + 0x10)) {
        if (LD32(child) == PAGE_COUNTER_VTABLE && LD32(child + 0x7c) == selector) return child;
    }
    return 0;
}

static uint32_t attach_page_counter(uint32_t panel, uint32_t selector) {
    static uint32_t pause_layout;
    if (!pause_layout) pause_layout = ((uint32_t (*)(uint32_t, uint32_t))0x352960u)(0, PAUSE_HELP_LAYOUT);
    if (!pause_layout) return 0;
    uint32_t config[0x70 / 4] = {0};
    ((void (*)(void *))0x80097cu)(config);
    uint32_t gui = LD32(panel + 0x24);
    config[0x08 / 4] = gui;
    config[0x64 / 4] = 6;
    config[0x68 / 4] = LD32(panel + 0x28);
    uint32_t widget = ((uint32_t (*)(uint32_t, uint32_t, void *, uint32_t))0x76dd44u)
        (panel, LD32(PAGE_COUNTER_TYPE), config, 0);
    ((void (*)(void *))0x770710u)(config);
    if (!widget) return 0;
    ST32(widget + 0x7c, selector);

    const uint32_t nodes[] = {LD32(widget + 0x58), LD32(widget + 0x5c), LD32(widget + 0x60)};
    const float positions[] = {212.0f, 164.0f, 260.0f};
    for (unsigned i = 0; i < 3; i++) {
        uint32_t fixed = LD32(gui + 0x298);
        // Rebind the three instance nodes to the layout that owns the pause menu's counter.
        uint32_t node = nodes[i] < fixed ? LD32(LD32(gui + 0xf8) + 4 * nodes[i]) :
            LD32(LD32(gui + 0x294) + 4 * (nodes[i] - fixed));
        void (*bind_instance)(uint32_t, uint32_t) = (void (*)(uint32_t, uint32_t))0xc38828u;
        uint32_t instance = LD32(node + 0xd4);
        bind_instance(node, UINT32_MAX);
        ST32(node + 0x70, pause_layout);
        bind_instance(node, instance);
        if (!LD32(node + 0xd8) || !LD32(node + 0xdc)) {
            // Prevent the stock update from dereferencing an unloaded instance.
            ST8(widget + 0x18, 0);
            return 0;
        }
        ((void (*)(uint32_t))LD32(LD32(node) + 0x20))(node);
        ((void (*)(uint32_t, uint32_t))0xc3896cu)(node, 1);
        ((NodePosition)0x745a40u)(gui, nodes[i], positions[i]);
        ((NodePosition)0x745ac0u)(gui, nodes[i], 22.0f);
    }
    return widget;
}

static void draw_page_indicator(uint32_t panel, uint32_t sel, PageIndicator *indicator,
                                int root, int paged) {
    uint32_t display = (uint32_t)(uintptr_t)indicator->selector;
    uint32_t widget = page_counter(panel, display);
    if (root) {
        memcpy(indicator->selector, (const void *)(uintptr_t)sel, sizeof indicator->selector);
        indicator->page = LD16(sel + 16);
        if (!widget) widget = attach_page_counter(panel, display);
    }
    if (!widget || !LD8(widget + 0x18)) return;
    ST16(display + 16, indicator->page);
    ST16(display + 18, paged ? 2 : 1);
    ST32(display + 4, root ? 0 : 1);
    ST32(display + 8, indicator->flash > 4 ? (indicator->left ? 4 : 8) : 0);
    ((void (*)(uint32_t))0x800ec8u)(widget);
    // The shared GUI resets instance visibility when the next item box opens.
    // Keep the borrowed arrows outside the canvas until a paged box uses them.
    uint32_t gui = LD32(widget + 0x24);
    float y = paged ? 22.0f : -1024.0f;
    ((NodePosition)0x745ac0u)(gui, LD32(widget + 0x5c), y);
    ((NodePosition)0x745ac0u)(gui, LD32(widget + 0x60), y);
}

static uint32_t update_page_indicator(uint32_t (*update)(uint32_t), uint32_t ui, uint32_t input,
                                      uint32_t selector, PageIndicator *indicator) {
    unsigned page = LD16(selector + 16);
    uint32_t buttons = input ? LD32(input) | LD32(input + INPUT_REPEAT - INPUT_PRESSED) : 0;
    uint32_t result = update_paged_menu(update, ui, input);
    if (page != LD16(selector + 16)) {
        indicator->left = (buttons & (BUTTON_ZL | DPAD_LEFT)) != 0;
        indicator->flash = 10;
    }
    return result;
}

static void restore_root_selection(uint32_t selector, unsigned row) {
    ST16(selector + 16, 1);
    ST8(selector + 13, row);
}

static uint32_t board_update_detour(uint32_t ui) {
    uint32_t input = input_address();
    uint32_t state = LD32(ui + BOARD_STATE);
    uint32_t sel = ui + BOARD_SEL;
    uint32_t pressed = input ? LD32(input) : 0;
    int root_ready = state == 1 && LD32(ui + BOARD_READY) == 4 && LD8(sel + 14) == 8;
    if (root_ready && LD16(sel + 20) == 8) {
        ((void (*)(uint32_t, uint32_t, uint32_t))FN_SEL_COUNT)(sel, 9, PAGE_INPUT_DPAD);
        LOG("[layered] Palico board appearance entry enabled\n");
    }
    if (ui == board_capture) {
        if (root_ready && LD16(sel + 20) == 9) {
            restore_root_selection(sel, 0);
            board_indicator.flash = 0;
            board_capture = 0;
            board_drive = board_return = 0;
        } else if (board_return && state > 1 && state <= 5 && input) {
            return with_button(g_board_update, ui, input, BUTTON_B, 1);
        } else if (board_drive && state == 2 && LD32(ui + BOARD_READY) == 4 && input) {
            board_drive = 0;
            uint32_t result = with_button(g_board_update, ui, input, BUTTON_A, 0);
            if (LD32(ui + BOARD_STATE) != 4 || LD32(ui + 0x48) != 6) board_capture = 0;
            return result;
        } else if (state == 4 && (pressed & BUTTON_B)) {
            board_return = 1;
        } else if (state == 0 || state > 5) {
            board_capture = 0;
            board_drive = board_return = 0;
        }
    }
    if (root_ready) {
        if (LD16(sel + 20) == 9 && LD16(sel + 16) == 1 &&
            (pressed & (BUTTON_A | BUTTON_B)) == BUTTON_A) {
            ST16(sel + 16, 0);
            ST8(sel + 13, 4); // Change/Sell Equipment
            uint32_t result = with_button(g_board_update, ui, input, BUTTON_A, 1);
            if (LD32(ui + BOARD_STATE) == 2 && LD32(ui + 0x40) == 4) {
                board_capture = ui;
                board_drive = 1;
                board_return = 0;
            }
            return result;
        }
        if (LD16(sel + 20) == 9)
            return update_page_indicator(g_board_update, ui, input, sel, &board_indicator);
    }
    return g_board_update(ui);
}

static void draw_board_page(uint32_t panel) {
    if (LD32(panel) != BOARD_PANEL_VTABLE || LD32(panel + 0x98) != 3) return;
    uint32_t sel = LD32(panel + 0x54);
    if (!sel || LD8(sel + 14) != 8) return;
    uint32_t ui = sel - BOARD_SEL;
    if (LD32(ui) != BOARD_VTABLE) return;
    int root = LD32(ui + BOARD_STATE) == 1 && LD16(sel + 20) == 9;
    draw_page_indicator(panel, sel, &board_indicator, root, 1);
    if (!root && LD16(sel + 20) != 9) return;
    unsigned page = LD16(sel + 16);
    uint32_t visible = root && page == 1 ? 1 : 8;
    if (LD32(panel + 0x64) != visible)
        ((void (*)(uint32_t, uint32_t))FN_VISIBLE)(panel, visible);
    if (root) {
        if (page == 1) box_draw_row(panel, 0, g_our_label, 1, 1);
    }
}

#define PRESET_COUNT 40u
#define PRESETS_PER_PAGE 8u
#define PRESET_NAME_CAPACITY 42u
#define PRESET_SIGNATURE 0x3150414cu
#define PRESET_SELECTOR 0x2d0u
#define PRESET_PANEL 0x14u
#define NO_PRESET PRESET_COUNT

typedef enum { EQUIP_PRESET, REGISTER_PRESET, MOVE_PRESET, REMOVE_PRESET } PresetAction;

typedef struct {
    uint16_t ids[5];
    uint8_t used;
    char name[PRESET_NAME_CAPACITY];
} AppearancePreset;

typedef struct {
    uint32_t signature;
    AppearancePreset slots[PRESET_COUNT];
} PresetStore;

static PresetStore presets;
static PresetStore preset_draft;
static uint32_t preset_ui;
static int kinsect_picker;
static int preset_native_names;
static int preset_renaming;
static PresetAction preset_action;
static unsigned move_source = NO_PRESET;
static unsigned confirm_slot = NO_PRESET;
static const char preset_path[] = "savedata:/layered_presets.bin";
static const char preset_legacy_path[] = "savedata:/layered_sets.ini";
static const char preset_title[] = "\xe5\xb9\xbb\xe5\x8c\x96\xe4\xb8\xaa\xe4\xba\xba\xe7\xbb\x84\xe5\x90\x88";
static const char kinsect_title[] = "\xe7\x8c\x8e\xe8\x99\xab\xe5\xb9\xbb\xe5\x8c\x96";
static const char *const preset_actions[] = {
    "\xe8\xa3\x85\xe5\xa4\x87\xe5\xb9\xbb\xe5\x8c\x96\xe7\xbb\x84\xe5\x90\x88", "\xe7\x99\xbb\xe8\xae\xb0\xe5\xb9\xbb\xe5\x8c\x96\xe7\xbb\x84\xe5\x90\x88", "\xe7\xa7\xbb\xe5\x8a\xa8\xe5\xb9\xbb\xe5\x8c\x96\xe7\xbb\x84\xe5\x90\x88", "\xe8\xa7\xa3\xe9\x99\xa4\xe5\xb9\xbb\xe5\x8c\x96\xe7\xbb\x84\xe5\x90\x88"
};
static const char preset_prefix[] = "\xe7\xbb\x84\xe5\x90\x88\x20";
static char preset_default_names[PRESET_COUNT][12];

static uint32_t (*g_get_set_name)(uint32_t, uint32_t);
static void (*g_set_set_name)(uint32_t, uint32_t, uint32_t, const char *);
static uint32_t (*g_update_set_menu)(uint32_t);
static void (*g_preview_set)(uint32_t, uint32_t);
static void (*g_draw_preset_list)(uint32_t);

static int hex_digit(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return -1;
}

static void load_legacy_presets(void) {
    char data[8][30];
    if (!load_file(preset_legacy_path, data, sizeof data)) return;
    for (unsigned i = 0; i < 8; i++) {
        const char *row = data[i];
        if (!key_match(row, "set", 3) || row[3] != '1' + i || row[4] != '=' || row[29] != '\n') continue;
        AppearancePreset preset = {0};
        preset.used = 1;
        for (unsigned part = 0; part < 5; part++) {
            const char *digits = row + 5 + part * 5;
            for (unsigned j = 0; j < 4; j++) {
                int digit = hex_digit(digits[j]);
                if (digit < 0) { preset.used = 0; break; }
                preset.ids[part] = (uint16_t)((preset.ids[part] << 4) | digit);
            }
            if (part < 4 && digits[4] != ',') preset.used = 0;
        }
        if (preset.used) presets.slots[i] = preset;
    }
}

static void load_presets(void) {
    if (!load_file(preset_path, &presets, sizeof presets) || presets.signature != PRESET_SIGNATURE) {
        memset(&presets, 0, sizeof presets);
        presets.signature = PRESET_SIGNATURE;
        load_legacy_presets();
    }
    for (unsigned i = 0; i < PRESET_COUNT; i++) {
        presets.slots[i].name[PRESET_NAME_CAPACITY - 1] = 0;
        char *label = preset_default_names[i];
        memcpy(label, preset_prefix, sizeof preset_prefix - 1);
        label += sizeof preset_prefix - 1;
        unsigned number = i + 1;
        if (number >= 10) *label++ = (char)('0' + number / 10);
        *label++ = (char)('0' + number % 10);
        *label = 0;
    }
}

static int commit_presets(void) {
    if (!save_record(preset_path, &preset_draft, sizeof preset_draft, 0, sizeof preset_draft)) {
        menu_sound(0, SND_ERROR);
        LOG("[layered] appearance preset save failed\n");
        return 0;
    }
    presets = preset_draft;
    return 1;
}

static const char *preset_name(unsigned slot) {
    const AppearancePreset *preset = &presets.slots[slot];
    if (preset->name[0]) return preset->name;
    return preset->used ? preset_default_names[slot] : "";
}

static uint32_t get_set_name_detour(uint32_t box, uint32_t slot) {
    if (preset_native_names && box == LD32(G_EQUIP_BOX) && slot < PRESET_COUNT)
        return (uint32_t)(uintptr_t)(kinsect_picker ? kinsect_name(slot + 1) : preset_name(slot));
    return g_get_set_name(box, slot);
}

static void set_set_name_detour(uint32_t box, uint32_t kind, uint32_t slot, const char *name) {
    if (!preset_native_names || !preset_renaming) {
        g_set_set_name(box, kind, slot, name);
        return;
    }
    if (box != LD32(G_EQUIP_BOX) || kind != 0 || slot >= PRESET_COUNT) return;
    preset_draft = presets;
    char *dest = preset_draft.slots[slot].name;
    memset(dest, 0, PRESET_NAME_CAPACITY);
    for (unsigned i = 0; i + 1 < PRESET_NAME_CAPACITY && name[i]; i++) dest[i] = name[i];
    if (commit_presets()) LOG("[layered] named appearance preset %u\n", slot + 1);
}

static uint32_t box_widget(uint32_t ui, unsigned index) {
    uint32_t widgets = LD32(ui + 0x34);
    return widgets ? LD32(widgets + 4 * index) : 0;
}

static void preview_set_detour(uint32_t ui, uint32_t slot) {
    if (ui != preset_ui) g_preview_set(ui, slot);
}

static unsigned preset_page(uint32_t ui) {
    return LD16(ui + 0x2e8);
}

static void set_kinsect_last_row(uint32_t panel, int visible) {
    uint32_t gui = LD32(panel + 0x24);
    // menu_common_down: last button (11), last separator in Anm_line (13),
    // and Anm_Window_main (14). A 36-pixel row uses 1.8 window scale units.
    ((void (*)(uint32_t, uint32_t, uint32_t))0x74533cu)(gui, 11, visible);
    ((void (*)(uint32_t, uint32_t, uint32_t, uint32_t))0x76e530u)(panel, 13, 9, visible);
    ((NodePosition)0x7460ecu)(gui, 14, visible ? 16.8f : 15.0f);
}

static void draw_set_list_detour(uint32_t panel) {
    int replace_title = preset_ui && (LD8(panel + 0x54) & 1) &&
                        box_widget(preset_ui, PRESET_PANEL) == panel;
    g_draw_preset_list(panel);
    if (replace_title) {
        ((void (*)(uint32_t, uint32_t, uint32_t, const char *))FN_LABEL)
            (panel, 2, 3, kinsect_picker ? kinsect_title : preset_title);
        if (kinsect_picker)
            set_kinsect_last_row(panel, (preset_page(preset_ui) + 1) * PRESETS_PER_PAGE <= KINSECT_COUNT);
    }
}

static unsigned preset_index(uint32_t ui) {
    return preset_page(ui) * PRESETS_PER_PAGE + LD8(ui + 0x2e5);
}

static void set_preset_focus(uint32_t ui, int locked) {
    ((void (*)(uint32_t, uint32_t))0x74ecc0u)(ui + PRESET_SELECTOR, locked);
}

static void refresh_preset_rows(uint32_t ui) {
    unsigned page = preset_page(ui);
    if (page >= PRESET_COUNT / PRESETS_PER_PAGE) return;
    for (unsigned row = 0; row < PRESETS_PER_PAGE; row++) {
        unsigned slot = page * PRESETS_PER_PAGE + row;
        char *name = (char *)(uintptr_t)(ui + 0x59c + 0x2c * row);
        const char *source = kinsect_picker ? kinsect_name(slot + 1) : preset_name(slot);
        memset(name, 0, 0x2c);
        for (unsigned i = 0; i + 1 < PRESET_NAME_CAPACITY && source[i]; i++) name[i] = source[i];
        name[0x2a] = !kinsect_picker || slot < KINSECT_COUNT;
        name[0x2b] = !kinsect_picker && slot == move_source;
    }
    uint32_t panel = box_widget(ui, PRESET_PANEL);
    if (panel) ST32(panel + 0x54, LD32(panel + 0x54) | 1);
}

static void preset_message(uint32_t ui, unsigned message, int confirm) {
    if (!confirm && LD32(ui + 0x354) == 1 && LD32(ui + 0x358) == 0x16 &&
        LD32(ui + 0x35c) == message && !LD8(ui + 0x360)) return;
    ST32(ui + 0x354, 1);
    ST32(ui + 0x358, 0x16);
    ST32(ui + 0x35c, message);
    ((void (*)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t))0x85e5bcu)
        (confirm ? 0x14 : 4, 1, 0x16, message, confirm ? LD8(ui + 0x6c) : 0, 0);
    ST8(ui + 0x360, 0);
}

static void preset_guide(uint32_t ui) {
    static const unsigned descriptions[] = {0x95, REGISTER_APPEARANCE_MESSAGE, 0x9c, 0x9f};
    unsigned message = move_source == NO_PRESET ? descriptions[preset_action] : 0x9d;
    preset_message(ui, message, 0);
    // The stock guide for a slot with no equipment preview still offers Change Name.
    ((void (*)(uint32_t, uint32_t, uint32_t, uint32_t))0x918a0cu)(ui, 0x28, 1, 1);
}

static void kinsect_guide(uint32_t ui) {
    preset_message(ui, KINSECT_APPEARANCE_MESSAGE, 0);
    ((void (*)(uint32_t, uint32_t, uint32_t, uint32_t))0x918a0cu)(ui, 0x29, 1, 1);
}

static void apply_preset_slot(uint32_t ui, unsigned slot) {
    const AppearancePreset *chosen = &presets.slots[slot];
    if (preset_action == REGISTER_PRESET) {
        preset_draft = presets;
        AppearancePreset *dest = &preset_draft.slots[slot];
        if (!snapshot_current(dest->ids)) { menu_sound(0, SND_ERROR); return; }
        dest->used = 1;
        if (!commit_presets()) return;
        menu_sound(1, SND_REGISTER);
        LOG("[layered] registered appearance preset %u\n", slot + 1);
    } else if (preset_action == EQUIP_PRESET && chosen->used) {
        if (!set_current(chosen->ids)) return;
        menu_sound(1, SND_EQUIP);
        LOG("[layered] equipped appearance preset %u\n", slot + 1);
    } else if (preset_action == MOVE_PRESET && move_source != NO_PRESET) {
        if (move_source == slot) {
            move_source = NO_PRESET;
            menu_sound(0, SND_CANCEL);
            refresh_preset_rows(ui);
            return;
        }
        preset_draft = presets;
        preset_draft.slots[slot] = presets.slots[move_source];
        preset_draft.slots[move_source] = *chosen;
        if (!commit_presets()) return;
        menu_sound(0, SND_MOVE_DONE);
        LOG("[layered] moved appearance preset %u to %u\n", move_source + 1, slot + 1);
        move_source = NO_PRESET;
    } else if (preset_action == MOVE_PRESET && (chosen->used || chosen->name[0])) {
        move_source = slot;
        menu_sound(0, SND_MOVE_PICK);
    } else if (preset_action == REMOVE_PRESET && (chosen->used || chosen->name[0])) {
        preset_draft = presets;
        memset(&preset_draft.slots[slot], 0, sizeof *chosen);
        if (!commit_presets()) return;
        menu_sound(0, SND_CONFIRM);
        LOG("[layered] removed appearance preset %u\n", slot + 1);
    } else {
        menu_sound(0, SND_ERROR);
    }
    refresh_preset_rows(ui);
}

static void begin_preset_confirm(uint32_t ui, unsigned slot) {
    confirm_slot = slot;
    ST8(ui + 0x6c, 0);
    set_preset_focus(ui, 1);
    menu_sound(0, preset_action == REMOVE_PRESET ? SND_REMOVE_PROMPT : SND_CONFIRM);
}

static void update_preset_confirm(uint32_t ui, uint32_t pressed) {
    unsigned slot = confirm_slot;
    unsigned message = 0x96;
    if (preset_action == REGISTER_PRESET) message = presets.slots[slot].used ? 0xa1 : 0x9a;
    if (preset_action == REMOVE_PRESET) message = 0xa0;
    unsigned choice = ((uint32_t (*)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t))0x74fd94u)
        (ui + 0x6c, pressed, DPAD_LEFT, DPAD_RIGHT, 0);
    if (!choice) {
        preset_message(ui, message, 1);
        ((void (*)(uint32_t, uint32_t, uint32_t, uint32_t))0x918a0cu)(ui, 0x29, 1, 1);
        return;
    }
    confirm_slot = NO_PRESET;
    set_preset_focus(ui, 0);
    if (choice == 1) apply_preset_slot(ui, slot);
    else menu_sound(0, SND_CANCEL);
    preset_guide(ui);
}

static void set_preset_capacity(uint32_t ui, unsigned count) {
    ST16(ui + PRESET_SELECTOR + 0x1a, (count + PRESETS_PER_PAGE - 1) / PRESETS_PER_PAGE);
    ST16(ui + PRESET_SELECTOR + 0x1c, count);
}

static void restore_kinsect_list(uint32_t ui) {
    uint32_t panel = box_widget(ui, PRESET_PANEL);
    if (panel) set_kinsect_last_row(panel, 1);
    set_preset_capacity(ui, PRESET_COUNT);
}

static void open_preset_slots(uint32_t ui, PresetAction action) {
    static const uint32_t open_list[] = {0x925f0cu, 0x92604cu, 0x9261a0u, 0x926280u};
    preset_action = action;
    move_source = confirm_slot = NO_PRESET;
    set_preset_capacity(ui, kinsect_picker ? KINSECT_COUNT : PRESET_COUNT);
    preset_native_names = 1;
    ((void (*)(uint32_t))open_list[action])(ui);
    preset_native_names = 0;
    ST32(ui + 0x5c, 2);
    ((void (*)(uint32_t, uint32_t))0x85cc0cu)(ui, 0);
    refresh_preset_rows(ui);
    if (kinsect_picker) kinsect_guide(ui);
    else preset_guide(ui);
    menu_sound(0, SND_DIRECT);
}

static void update_preset_cursor(uint32_t ui, uint32_t input, uint32_t pressed) {
    uint32_t repeated = input ? LD32(input + INPUT_REPEAT - INPUT_PRESSED) : 0;
    uint32_t buttons = pressed | repeated;
    if (pressed & BUTTON_ZL) buttons |= DPAD_LEFT;
    if (pressed & BUTTON_ZR) buttons |= DPAD_RIGHT;
    unsigned slot = preset_index(ui);
    ((void (*)(uint32_t, uint32_t))0x7530b8u)(ui + PRESET_SELECTOR, buttons);
    if (slot != preset_index(ui)) refresh_preset_rows(ui);
}

static uint32_t update_kinsect_picker(uint32_t ui, uint32_t input, uint32_t pressed) {
    uint32_t key = equipped_weapon_key();
    if ((pressed & BUTTON_B) || weapon_key_type(key) != INSECT_GLAIVE_TYPE) {
        ((void (*)(uint32_t))0x926e7cu)(ui);
        restore_kinsect_list(ui);
        // Finish the native submenu cancellation on the same update.
        return input ? with_button(g_update_set_menu, ui, input, BUTTON_B, 0) : 1;
    }
    if (pressed & BUTTON_A) {
        unsigned id = preset_index(ui) + 1;
        if (kinsect_data(id) && set_weapon_appearance(key, (uint16_t)id, 1)) {
            menu_sound(1, SND_EQUIP);
            LOG("[layered] applied kinsect appearance %u\n", id);
        } else menu_sound(0, SND_ERROR);
    } else {
        update_preset_cursor(ui, input, pressed);
    }
    kinsect_guide(ui);
    return 1;
}

static uint32_t update_set_menu_detour(uint32_t ui) {
    if (ui != preset_ui) return g_update_set_menu(ui);
    uint32_t input = input_address();
    uint32_t pressed = input ? LD32(input) : 0;
    if (kinsect_picker) return update_kinsect_picker(ui, input, pressed);
    if (LD32(ui + 0x5c) == 1) {
        if ((pressed & (BUTTON_A | BUTTON_B)) != BUTTON_A) return g_update_set_menu(ui);
        unsigned row = LD8(ui + 0x15d);
        if (row < 4) open_preset_slots(ui, (PresetAction)row);
        return 1;
    }
    unsigned slot = preset_index(ui);
    if (slot >= PRESET_COUNT) return 1;
    if (confirm_slot != NO_PRESET) {
        update_preset_confirm(ui, pressed);
        return 1;
    }
    if (preset_renaming) {
        preset_native_names = 1;
        uint32_t waiting = ((uint32_t (*)(uint32_t, uint32_t))0x92d644u)(ui, slot);
        preset_native_names = 0;
        if (!waiting) {
            preset_renaming = 0;
            set_preset_focus(ui, 0);
            refresh_preset_rows(ui);
        }
        return 1;
    }
    if (pressed & BUTTON_B) {
        if (move_source != NO_PRESET) {
            move_source = NO_PRESET;
            refresh_preset_rows(ui);
            preset_guide(ui);
        } else {
            ((void (*)(uint32_t))0x926e7cu)(ui);
        }
        menu_sound(0, SND_CANCEL);
        return 1;
    }
    if (pressed & BUTTON_A) {
        if (preset_action == MOVE_PRESET) apply_preset_slot(ui, slot);
        else if (preset_action == REGISTER_PRESET || presets.slots[slot].used ||
                 (preset_action == REMOVE_PRESET && presets.slots[slot].name[0])) {
            begin_preset_confirm(ui, slot);
            update_preset_confirm(ui, 0);
            return 1;
        } else menu_sound(0, SND_ERROR);
    } else if (pressed & ((uint32_t (*)(void))0x85f808u)()) {
        preset_renaming = 1;
        set_preset_focus(ui, 1);
        preset_native_names = 1;
        ((void (*)(uint32_t, uint32_t))0x92d588u)(ui, slot);
        preset_native_names = 0;
        menu_sound(0, SND_DIRECT);
    } else {
        update_preset_cursor(ui, input, pressed);
    }
    preset_guide(ui);
    return 1;
}

static uint32_t (*g_boxupd)(uint32_t);
static uint32_t capture_box_ui;
static int g_drive;
static int box_return;
static uint32_t (*g_hunter_grid)(uint32_t);
static void (*g_equipment_icon)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);

typedef enum {
    BOX_ARMOR, BOX_PRESETS, BOX_RESTORE_ARMOR, BOX_WEAPON, BOX_RESTORE_WEAPON,
    BOX_KINSECT, BOX_RESTORE_KINSECT, BOX_APPEARANCE_ROWS
} AppearanceBoxRow;

static const char *const appearance_box_labels[] = {
    g_our_label, preset_title, hunter_restore,
    "\xe6\xad\xa6\xe5\x99\xa8\xe5\xb9\xbb\xe5\x8c\x96", "\xe8\xbf\x98\xe5\x8e\x9f\xe6\xad\xa6\xe5\x99\xa8\xe5\xa4\x96\xe8\xa7\x82",
    kinsect_title, "\xe8\xbf\x98\xe5\x8e\x9f\xe7\x8c\x8e\xe8\x99\xab\xe5\xa4\x96\xe8\xa7\x82",
};
static AppearanceBoxRow capture_row;

static int weapon_donor_matches(uint32_t weapon, uint32_t entry) {
    return weapon && entry && (LD16(entry) & 0x1f) == weapon_key_type(weapon) && LD16(entry + 2);
}

static void equipment_icon_detour(uint32_t widget, uint32_t group, uint32_t node,
                                  uint32_t record, uint32_t style, uint32_t flags) {
    // Native equipment filters send 0 for unavailable grid icons.
    if (capture_box_ui && capture_row == BOX_WEAPON && group == 12 &&
        LD32(widget + 0x58) == capture_box_ui + 0x174 && record &&
        !weapon_donor_matches(equipped_weapon_key(), record + ENTRY_OFF))
        flags = 0;
    g_equipment_icon(widget, group, node, record, style, flags);
}

static int appearance_row_enabled(AppearanceBoxRow row, uint32_t weapon) {
    if (row == BOX_KINSECT || row == BOX_RESTORE_KINSECT)
        return weapon_key_type(weapon) == INSECT_GLAIVE_TYPE;
    if (row == BOX_WEAPON) return weapon && g_equipment_icon;
    if (row == BOX_RESTORE_WEAPON) return weapon != 0;
    return 1;
}

static void draw_appearance_box_rows(uint32_t panel, unsigned selected) {
    uint32_t weapon = equipped_weapon_key();
    box_set_visible(panel, BOX_APPEARANCE_ROWS);
    for (unsigned row = 0; row < BOX_APPEARANCE_ROWS; row++) {
        box_draw_row(panel, row, appearance_box_labels[row], row == selected,
                     appearance_row_enabled((AppearanceBoxRow)row, weapon));
    }
}

static uint32_t hunter_grid_detour(uint32_t ui) {
    uint32_t input = input_address();
    if (ui != capture_box_ui || LD32(ui + 0x88) != 0 || !input ||
        (LD32(input) & (BUTTON_A | BUTTON_B)) != BUTTON_A)
        return g_hunter_grid(ui);

    uint32_t box = LD32(G_EQUIP_BOX);
    uint32_t columns = LD8(ui + 0x18a);
    uint32_t rows = LD8(ui + 0x18b);
    uint32_t index = columns * (rows * LD16(ui + 0x18c) + LD8(ui + 0x189)) + LD8(ui + 0x188);
    if (!box || index >= MAX_IDX || !((int (*)(uint32_t, uint32_t))0x344c04u)(box, index)) {
        menu_sound(0, SND_ERROR);
        return 1;
    }
    uint32_t record = box + BOX_RECORDS + index * REC_STRIDE + ENTRY_OFF;
    uint32_t type = LD16(record) & 0x1f;
    if (capture_row == BOX_WEAPON) {
        uint32_t weapon = equipped_weapon_key();
        uint16_t id = LD16(record + 2);
        if (weapon_donor_matches(weapon, record) && hunter_item(type, id) &&
            set_weapon_appearance(weapon, id, 0)) {
            menu_sound(1, SND_EQUIP);
            LOG("[layered] applied weapon appearance type=%u id=%u (box index %u)\n", type, id, index);
        } else menu_sound(0, SND_ERROR);
        return 1;
    }
    uint16_t id = LD16(record + APP_OFF);
    if (!id) id = LD16(record + 2);
    if (type < 1 || type > 5 || !id || !hunter_item(type, id)) {
        menu_sound(0, SND_ERROR);
        return 1;
    }
    uint16_t ids[5];
    memcpy(ids, cosmetic, sizeof ids);
    ids[type - 1] = id;
    if (set_current(ids)) {
        menu_sound(1, SND_EQUIP);
        LOG("[layered] Hunter part %u appearance %04x (box index %u)\n", type, id, index);
    }
    return 1;
}

static void restore_box_root(uint32_t ui, unsigned row) {
    restore_root_selection(ui + BOX_SEL, row);
    ST32(ui + 0x70, 9 + row);
    box_indicator.flash = 0;
}

static uint32_t boxupd_detour(uint32_t ui) {
    uint32_t input = input_address();
    uint32_t pressed = input ? LD32(input) : 0;
    uint32_t state = LD32(ui + 0x40);
    uint32_t panel = box_widget(ui, 0);
    uint32_t sel = ui + BOX_SEL;
    int root = state == 1 && panel && LD32(panel + 0x5c) == 9 && LD32(ui + 0x3c) != 1;
    if (root && LD16(sel + 20) == 9 && LD8(sel + 14) == 9) {
        ((void (*)(uint32_t, uint32_t, uint32_t))FN_SEL_COUNT)(sel, 9 + BOX_APPEARANCE_ROWS, PAGE_INPUT_DPAD);
        LOG("[layered] item box appearance entries enabled\n");
    }
    int root_ready = root && LD16(sel + 20) == 9 + BOX_APPEARANCE_ROWS;

    if (root_ready) {
        g_box_ui = ui;
        g_box_panel = panel;
    } else if (g_box_ui == ui && state != 3) {
        // The next item box can reuse this GUI after the borrowed widget is destroyed.
        if (panel == g_box_panel) draw_page_indicator(panel, sel, &box_indicator, 0, 0);
        g_box_ui = 0;
        g_box_panel = 0;
    }

    if (ui == preset_ui && (root_ready || state == 0 || state == 4)) {
        if (root_ready) restore_box_root(ui, kinsect_picker ? BOX_KINSECT : BOX_PRESETS);
        if (kinsect_picker) restore_kinsect_list(ui);
        preset_ui = 0;
        preset_renaming = 0;
        kinsect_picker = 0;
    }
    if (ui == capture_box_ui) {
        if (root_ready || state == 0 || state == 4) {
            capture_box_ui = 0;
            g_drive = box_return = 0;
            if (root_ready) restore_box_root(ui, capture_row);
        } else if (state == 3 && input) {
            if (box_return) {
                return with_button(g_boxupd, ui, input, BUTTON_B, 1);
            }
            if (g_drive && !(pressed & BUTTON_B)) {
                g_drive = 0;
                return with_button(g_boxupd, ui, input, BUTTON_A, 0);
            }
            if (pressed & BUTTON_B) {
                g_drive = 0;
                box_return = 1;
            }
        }
    }
    if (root) {
        if (root_ready && LD16(sel + 16) == 1 &&
            (pressed & (BUTTON_A | BUTTON_B)) == BUTTON_A) {
            AppearanceBoxRow row = (AppearanceBoxRow)LD8(sel + 13);
            uint32_t weapon = equipped_weapon_key();
            if (row >= BOX_APPEARANCE_ROWS || !appearance_row_enabled(row, weapon)) {
                menu_sound(0, SND_ERROR);
                return 0;
            }
            if (row == BOX_RESTORE_ARMOR) {
                const uint16_t native[5] = {0};
                if (set_current(native)) {
                    menu_sound(0, SND_DIRECT);
                }
                return 0;
            }
            if (row == BOX_RESTORE_WEAPON || row == BOX_RESTORE_KINSECT) {
                if (set_weapon_appearance(weapon, 0, row == BOX_RESTORE_KINSECT)) {
                    menu_sound(0, SND_DIRECT);
                    LOG("[layered] restored %s appearance\n", row == BOX_RESTORE_KINSECT ? "kinsect" : "weapon");
                } else menu_sound(0, SND_ERROR);
                return 0;
            }
            int use_list = row == BOX_PRESETS || row == BOX_KINSECT;
            if (use_list && (!g_get_set_name || !g_set_set_name || !g_update_set_menu || !g_preview_set)) {
                menu_sound(0, SND_ERROR);
                return 0;
            }
            unsigned native_row = use_list ? 6 : 5;
            ST16(sel + 16, 0);
            ST8(sel + 13, native_row);
            ST32(ui + 0x70, native_row);
            box_set_visible(panel, 9);
            if (use_list) {
                preset_ui = ui;
                kinsect_picker = row == BOX_KINSECT;
            }
            uint32_t result = with_button(g_boxupd, ui, input, BUTTON_A, row != BOX_PRESETS);
            if (use_list) {
                if (LD32(ui + 0x40) != 3 || LD32(ui + 0x74) != 6) {
                    preset_ui = 0;
                    kinsect_picker = 0;
                } else {
                    restore_root_selection(sel, row);
                    if (kinsect_picker) open_preset_slots(ui, REGISTER_PRESET);
                }
            } else if (LD32(ui + 0x40) == 3) {
                capture_box_ui = ui;
                capture_row = row;
                g_drive = 1;
                box_return = 0;
            }
            return result;
        }
        if (root_ready) {
            uint32_t result = update_page_indicator(g_boxupd, ui, input, sel, &box_indicator);
            unsigned row = LD8(sel + 13);
            if (LD32(ui + 0x40) == 1 && LD16(sel + 16) == 1 && row >= BOX_WEAPON &&
                row < BOX_APPEARANCE_ROWS) {
                unsigned hint = row - BOX_WEAPON;
                if (row >= BOX_KINSECT && !appearance_row_enabled((AppearanceBoxRow)row, equipped_weapon_key()))
                    hint = 4;
                preset_message(ui, WEAPON_BOX_MESSAGE + hint, 0);
            }
            return result;
        }
    }
    return g_boxupd(ui);
}

static void draw_preset_submenu(uint32_t panel) {
    uint32_t child = LD32(panel + 0xb4);
    if (!child) return;
    uint32_t nodes = LD32(child + 0x70);
    if (!nodes) return;
    uint32_t focus = LD8(preset_ui + 0x15d);
    for (unsigned i = 0; i < 4; i++) {
        uint32_t node = LD32(nodes + 4 * i);
        ((void (*)(uint32_t, uint32_t, uint32_t, const char *))FN_LABEL)(child, node, 1, preset_actions[i]);
        uint32_t color = ((uint32_t (*)(uint32_t, uint32_t, uint32_t, uint32_t))FN_COLOR)(1, i == focus, 0, 0);
        ((void (*)(uint32_t, uint32_t, uint32_t, uint32_t))FN_PAINT)(child, node, 1, color);
    }
}

static uint32_t (*g_boxdraw)(uint32_t);
static uint32_t boxdraw_detour(uint32_t panel) {
    uint32_t result = g_boxdraw(panel);
    if (!panel) return result;
    draw_board_page(panel);
    uint32_t sel = LD32(panel + 0x54);
    unsigned state = g_box_ui && panel == g_box_panel ? LD32(g_box_ui + 0x40) : 0;
    int paged = (state == 1 || state == 3) && sel == g_box_ui + BOX_SEL &&
                LD32(panel + 0x5c) == 9 && LD16(sel + 20) == 9 + BOX_APPEARANCE_ROWS;
    int root = paged && state == 1;
    draw_page_indicator(panel, sel, &box_indicator, root, paged);

    int preset_submenu = preset_ui && !kinsect_picker && LD32(preset_ui + 0x40) == 3 &&
                         LD32(preset_ui + 0x5c) == 1 && box_widget(preset_ui, 0) == panel;
    if (preset_submenu) {
        set_menu_title(panel, g_box_title);
        draw_appearance_box_rows(panel, UINT32_MAX);
        draw_preset_submenu(panel);
        return result;
    }
    if (!root) return result;
    unsigned page = LD16(sel + 16);
    if (page == 1) {
        draw_appearance_box_rows(panel, LD8(sel + 13));
    } else {
        box_set_visible(panel, 9);
    }
    return result;
}

static int game_supported(void) {
    static const uint32_t signatures[][3] = {
        {FN_BOX_UPDATE, 0xe92d4070u, 0xe1a04000u}, // Item box update
        {0x7c1fa4u, 0xe92d4010u, 0xe1a04000u}, // Shared menu draw
        {0x946050u, 0xe92d48f0u, 0xe1a04000u}, // Board update
        {0x94a3a8u, 0xe92d4ff0u, 0xe24dd024u}, // Palico equipment selection
        {0x9201ccu, 0xe92d4ff0u, 0xe24dd014u}, // Hunter equipment selection
        {0x76ea38u, 0xe92d4bf0u, 0xe24dd010u}, // Equipment grid icon
        {0x4899fcu, 0xe92d4070u, 0xe1a05001u}, // Model request
        {0x4861f0u, 0xe92d4ff0u, 0xe24ddf59u}, // Equipment model refresh
    };
    if (forge_mem_getMainTextAddr() != 0x00200000u) return 0;
    for (unsigned i = 0; i < sizeof signatures / sizeof signatures[0]; i++) {
        uint32_t address = signatures[i][0];
        uint32_t entry = LD32(address), next = LD32(address + 4);
        if (entry == signatures[i][1] && next == signatures[i][2]) continue;

        // GU Camp Village Item Box also hooks this function. Recomp writes
        // an ARM literal jump at the entry and chains it via forge_hook_create.
        if (address == FN_BOX_UPDATE && entry == 0xe51ff004u &&
            next >= 0x03200000u && !(next & 3) &&
            LD32(address + 8) == 0xebfd2708u &&
            LD32(address + 12) == 0xe1a05000u &&
            LD32(address + 16) == 0xe3550000u) {
            LOG("[layered] chaining existing Forge item box hook at %08x\n", next);
            continue;
        }
        LOG("[layered] instruction check failed at %08x (%08x %08x)\n", address, entry, next);
        return 0;
    }
    return 1;
}

void forge_onInit(void) {
    if (!game_supported()) {
        LOG("[layered] unsupported or modified MHGU code; plugin disabled\n");
        return;
    }
    if (!load_file(hunter_path, cosmetic, sizeof cosmetic)) read_config();
    hunter_refresh = 0x3e;
    if (load_file(cat_path, cat_looks, sizeof cat_looks)) {
        for (unsigned slot = 0; slot < CAT_COUNT; slot++) {
            if (!(cat_looks[slot].id[0] | cat_looks[slot].id[1])) continue;
            for (unsigned part = 0; part < 3; part++) {
                if (cat_looks[slot].model[part] <= UINT16_MAX)
                    cat_refresh[slot] |= (uint8_t)(1u << part);
            }
        }
    }
    load_presets();
    load_file(weapon_appearance_path, weapon_appearances, sizeof weapon_appearances);
    { ForgeHook h = forge_hook_create((void *)(uintptr_t)0x74f260u, (void *)menu_sound_detour, (void **)&g_menu_sound); (void)h; }
    { ForgeHook h = forge_hook_create((void *)(uintptr_t)0x9182f4u, (void *)update_set_menu_detour, (void **)&g_update_set_menu); (void)h; }
    { ForgeHook h = forge_hook_create((void *)(uintptr_t)0x346408u, (void *)get_set_name_detour, (void **)&g_get_set_name); (void)h; }
    { ForgeHook h = forge_hook_create((void *)(uintptr_t)0x348044u, (void *)set_set_name_detour, (void **)&g_set_set_name); (void)h; }
    { ForgeHook h = forge_hook_create((void *)(uintptr_t)0x92d3d8u, (void *)preview_set_detour, (void **)&g_preview_set); (void)h; }
    { ForgeHook h = forge_hook_create((void *)(uintptr_t)0x7d9060u, (void *)draw_set_list_detour, (void **)&g_draw_preset_list); (void)h; }
    { ForgeHook uh = forge_hook_create((void *)(uintptr_t)0x86ed24u, (void *)update_detour, (void **)&g_update); (void)uh; }
    { ForgeHook bh = forge_hook_create((void *)(uintptr_t)FN_BOX_UPDATE, (void *)boxupd_detour, (void **)&g_boxupd); (void)bh; }
    { ForgeHook h = forge_hook_create((void *)(uintptr_t)0x9201ccu, (void *)hunter_grid_detour, (void **)&g_hunter_grid); (void)h; }
    { ForgeHook h = forge_hook_create((void *)(uintptr_t)0x76ea38u, (void *)equipment_icon_detour, (void **)&g_equipment_icon); (void)h; }
    { ForgeHook dh = forge_hook_create((void *)(uintptr_t)0x7c1fa4u, (void *)boxdraw_detour, (void **)&g_boxdraw); (void)dh; }
    { ForgeHook h = forge_hook_create((void *)(uintptr_t)0x946050u, (void *)board_update_detour, (void **)&g_board_update); (void)h; }
    { ForgeHook h = forge_hook_create((void *)(uintptr_t)0x94a3a8u, (void *)board_grid_detour, (void **)&g_board_grid); (void)h; }
    { ForgeHook h = forge_hook_create((void *)(uintptr_t)0x4899fcu, (void *)model_request_detour, (void **)&g_model_request); (void)h; }
    { ForgeHook h = forge_hook_create((void *)(uintptr_t)0x4861f0u, (void *)equip_update_detour, (void **)&g_equip_update); (void)h; }
    { ForgeHook h1 = forge_hook_create((void *)(uintptr_t)0x98f784u, (void *)init_detour, (void **)&g_init); (void)h1;
      ForgeHook h2 = forge_hook_create((void *)(uintptr_t)0x991408u, (void *)fill_detour, (void **)&g_fill); (void)h2;
      ForgeHook h3 = forge_hook_create((void *)(uintptr_t)0x9918ccu, (void *)sel_detour, (void **)&g_sel); (void)h3;
      ForgeHook h4 = forge_hook_create((void *)(uintptr_t)0x75bac0u, (void *)msg_detour, (void **)&g_msg); (void)h4; }
    LOG("[layered] hooks update=%d init=%d fill=%d sel=%d msg=%d\n",
        g_update ? 1 : 0, g_init ? 1 : 0, g_fill ? 1 : 0, g_sel ? 1 : 0, g_msg ? 1 : 0);
}

void forge_onUpdate(float dt) {
    (void)dt;
    if (!g_model_request) return;
    if (board_indicator.flash) board_indicator.flash--;
    if (box_indicator.flash) box_indicator.flash--;
    if (weapon_save_delay) weapon_save_delay--;
    if (weapon_save_pending && !weapon_save_delay && !save_weapon_appearances()) weapon_save_delay = 120;
    static int prev_r3;
    int r3 = forge_input_isPressed(5);
    if (r3 && !prev_r3) mark_current();
    prev_r3 = r3;
}
