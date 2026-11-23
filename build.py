"""Build the Layered Armor Forge plug-in (AArch32) and pack it into an NRO."""
import argparse
import shutil
import struct
import subprocess
from pathlib import Path
from tempfile import TemporaryDirectory

def pack_nro(elf):
    if elf[:7] != b"\x7fELF\x01\x01\x01" or struct.unpack_from("<H", elf, 18)[0] != 40:
        raise ValueError("Expected a little-endian ARM ELF32 library")
    offset = struct.unpack_from("<I", elf, 28)[0]
    stride, count = struct.unpack_from("<HH", elf, 42)
    headers = [struct.unpack_from("<8I", elf, offset + stride * i) for i in range(count)]
    loads = [s for s in headers if s[0] == 1]
    if len(loads) != 3 or [s[6] for s in loads] != [4, 5, 6]:
        raise ValueError("Unexpected linker segment layout")
    dynamic, = [s for s in headers if s[0] == 2]
    data_start = loads[-1][2] & ~0xFFF
    text_end = loads[1][2] + loads[1][4]
    if loads[0][2] != 0 or loads[0][4] < 0xA0 or text_end > data_start:
        raise ValueError("ELF headers or segments cannot be mapped into an NRO")
    image_end = loads[-1][2] + loads[-1][4]
    bss = loads[-1][5] - loads[-1][4]
    image = bytearray(image_end)
    for _, file_offset, address, _, length, *_ in loads:
        image[address:address + length] = elf[file_offset:file_offset + length]
    image[:0xA0] = bytes(0xA0)
    struct.pack_into("<I", image, 4, 0x80)
    struct.pack_into("<4s9I", image, 0x10, b"NRO0", 0, image_end, 0,
                     0, text_end, data_start, 0, data_start, image_end - data_start)
    struct.pack_into("<I", image, 0x38, bss)
    struct.pack_into("<4sIII", image, 0x80, b"MOD0", dynamic[2] - 0x80,
                     image_end - 0x80, image_end + bss - 0x80)
    return image


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--zig", default=shutil.which("zig"), help="path to the Zig executable")
    parser.add_argument("--src", default=str(Path(__file__).resolve().parent))
    parser.add_argument("--out", default=str(Path(__file__).resolve().with_name("Native-Transmog.nro")))
    args = parser.parse_args()
    if not args.zig:
        parser.error("Zig was not found on PATH; pass its executable with --zig")
    src = Path(args.src)
    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    with TemporaryDirectory() as tmp:
        elf = Path(tmp) / "layered.elf"
        subprocess.run([
            args.zig, "cc", "--target=arm-linux-gnueabi", "-mcpu=cortex_a9", "-marm",
            "-mfloat-abi=softfp", "-Os", "-fPIC", "-ffreestanding", "-fno-builtin",
            "-fno-stack-protector", "-fno-strict-aliasing", "-fno-unwind-tables",
            "-nostdlib", "-shared",
            "-Wl,--hash-style=sysv,-z,max-page-size=4096,-z,norelro",
            str(src / "plugin.c"), "-o", str(elf),
        ], check=True)
        payload = pack_nro(elf.read_bytes())
    out.write_bytes(payload)
    print(out, len(payload))


if __name__ == "__main__":
    main()
