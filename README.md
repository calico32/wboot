# wboot

wboot is a minimal EFI bootloader for Linux, written for CS 502: Operating
Systems at WPI. It demonstrates the x86 boot process, UEFI programming, and
low-level OS concepts.

The presentation slides are available as PDF at
[presentation/presentation.pdf](presentation/presentation.pdf).

## Features/Limitations

-   Loads a Linux bzImage kernel and initramfs from any FAT filesystem
-   Configurable via a simple `wboot.conf` file on the ESP
-   Expects the kernel to be Zstandard-compressed
-   Does not support multiple initramfs files
-   Only tested with modern x86_64 Linux kernels, may not work with older kernels
-   Does not support ACPI or other advanced features, so the kernel may not boot
    on all hardware
-   Only tested with QEMU, may not work on real hardware without modifications
-   Does not have a user interface or robust error handling, so debugging may be
    difficult
-   Not intended for production use, just an educational project to demonstrate
    the boot process and UEFI programming

## Building and Running

Ensure submodules are initialized:

```bash
git submodule update --init --recursive
```

You will need Clang to build the bootloader and QEMU to run it. Additionally,
you'll need `pacstrap` to create the virtual Arch Linux disk image if you want
to test with a real Linux kernel and initramfs. You can install these tools on
Arch Linux with:

```bash
sudo pacman -S clang qemu arch-install-scripts`
```

To build the bootloader, run:

```bash
make zstd
make
```

This will build Zstandard and the bootloader and create a `BOOTX64.EFI` file in
the `src/phase3/` directory.

To create a virtual disk image with Arch Linux, run:

```bash
make root
```

This will create `qemu/root.img` with a minimal Arch Linux installation.

Finally, to run the bootloader in QEMU, run:

```bash
make ovmf
make mount
make run
```

This will compile OVMF firmware, mount the `root.img` as a virtual disk, copy
the bootloader to the ESP, and launch QEMU. You should see wboot's logs in the
serial console (stdio) and the Linux kernel booting up.

## Overview

`src` contains snapshots of the bootloader at various stages of development.

-   Phase 1 is a minimal UEFI application that prints "Hello, world!" and exits.
-   Phase 2 loads the Linux kernel and initramfs from the ESP, parses the setup
    header, and decompresses the kernel payload.
-   Phase 3 sets up boot_params, configures the CPU, and jumps to the kernel
    entry point.

Each phase has a `main.c` (entry point), a Makefile, and `wboot.h`/`wboot.c` for
the main bootloader logic. Phase 3 contains more files for graphics,
configuration, and the final handoff function in assembly. Set the `TARGET`
make variable to build and run a specific phase.

The `src/shared` directory contains common code used
across all phases, such as a minimal standard library, EFI globals, and type
definitions.

Vendored dependencies are in the `vendor` directory as git submodules. This
includes the Zstandard library for compression, and OVMF for UEFI firmware, and
gnu-efi for UEFI development headers.

The Linux x86 boot protocol was followed closely, and some parts are adapted
from the kernel EFI stub loader (mostly the boring parts, like setting up
graphics and translating the memory map).

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file
for details.
