#import "@preview/touying:0.7.3": *
#import "theme.typ": *
#import "@local/tree-sitter:0.1.0": (
    codeblock, tree-sitter-styles, tree-sitter-theme,
)
#import "settings.typ": *

#set text(font: "Inter")
#show title: set text(font: "Inter Display")
#show heading: set text(font: "Inter Display")
#tree-sitter-theme.update(theme)
#tree-sitter-styles.update(styles)
#show raw: set text(font: "CommitMono", fill: rgb("#abb2bf"))

#show: wboot-theme.with(
    aspect-ratio: "16-9",
    footer: [wboot],
    config-info(
        title: [wboot: A Linux UEFI bootloader, from scratch],
        subtitle: [A look at the x86 boot process, UEFI programming, and low-level OS concepts],
        author: [Caleb Chan],
        date: [Spring 2026],
        institution: [CS 502: Operating Systems],
    ),
)

#title-slide()

= Bootloading Basics

== From Firmware to OS

We've talked a whole lot about operating systems, but how do we actually get one running?

When a computer powers on, the OS kernel isn't magically in memory and ready to go.

The firmware (BIOS or UEFI) is responsible for initializing the hardware and then loading other programs to eventually get to the OS.

It loads a small program called a bootloader, which then loads the OS kernel and starts it.

We'll be writing our own bootloader, which will load a Linux kernel from disk and start it up.

== BIOS and UEFI

*BIOS*: The older standard, which has been around since the 1980s. 16-bit real mode, limited to 1MB of memory; MBR partitioning scheme; fixed 512-byte boot sector

*UEFI*: The modern replacement for BIOS, common on all PCs made in the last decade; 64-bit protected mode; can access much more memory; GPT partitioning scheme; many advanced features

#h(1em)

We'll only be talking about UEFI bootloading, since it's the modern standard and much less painful to work with than BIOS.

Also, x64 only. 32-bit computers are basically extinct at this point.

== UEFI Bootloading

UEFI bootloaders are just regular executables!

Except:

- Windows Portable Executable (PE) format, not ELF
- Windows x64 calling convention, not System V AMD64
- *No standard library!* You have the _UEFI Boot Services_ instead, which does some basic things but is pretty awkward to use. Bring your own implementation for most things.

We'll need to set up a custom build environment with the right headers and a cross-compiler.

== Development Environment

- Compiler: *clang*, with `-target x86_64-unknown-windows`, `-ffrestanding`, etc. to generate PE32+ binaries without CRT dependencies
- Linker: *lld* with some UEFI-specific flags
- UEFI headers: *gnu-efi* project
- Emulator: *QEMU* with OVMF firmware, which provides a UEFI environment for testing (no rebooting into a real machine every time!)

#codeblock(```make
# Simple as:
CC := clang
CFLAGS := -target x86_64-unknown-windows -ffreestanding -fshort-wchar -mno-red-zone ...
LDFLAGS := -target x86_64-unknown-windows -nostdlib -Wl,-entry:efi_main ...
BOOTX64.EFI: main.o
    $(CC) $(LDFLAGS) -o $@ $^
```)

== Hello, World!

#codeblock(```c
#include <efi.h>

EFI_STATUS efi_main(EFI_HANDLE handle, EFI_SYSTEM_TABLE *ST) {
    EFI_STATUS status = EFI_SUCCESS;
    EFI_INPUT_KEY key;

    // Don't forget your wide strings and CRLFs in UEFI!
    status = ST->ConOut->OutputString(ST->ConOut, L"Hello World\r\n");
    if (EFI_ERROR(status)) {
        return status;
    }

    while ((status = ST->ConIn->ReadKeyStroke(ST->ConIn, &key)) == EFI_NOT_READY) {}
    return status;
}
```)

== BYO Standard Library

#cols(columns: (1fr, auto))[
    #text(size: 0.9em)[
        Not having `printf` is kind of a pain, so now is also a good time to write some basic utilities for string manipulation, memory management, and formatted output.
    ]
    #codeblock(style: (..styles, text: (..styles.text, size: 0.54em)), ```c
    void *malloc(size_t size);
    void free(void *ptr);
    void *calloc(size_t nmemb, size_t size);
    EFI_STATUS printf(const CHAR16 *format, ...);
    CHAR16 read_char();
    const CHAR16 *strerror(EFI_STATUS status);
    VOID perror(const CHAR16 *message, EFI_STATUS status);
    INTN strcmp(const CHAR16 *str1, const CHAR16 *str2);
    INTN strcmp8(const CHAR8 *str1, const CHAR8 *str2);
    INTN strncmp(const CHAR16 *str1, const CHAR16 *str2, UINTN n);
    CHAR8 *strchr(const CHAR8 *str, CHAR8 c);
    CHAR8 *strtok_r(CHAR8 *str, const CHAR8 *delim, CHAR8 **saveptr);
    UINTN strtoul(const CHAR8 *str, CHAR8 **endptr, int base);
    UINTN strlen8(const CHAR8 *str);
    CHAR8 *strdup8(const CHAR8 *src);
    ```)
][
    #codeblock(style: (..styles, text: (..styles.text, size: 0.54em)), ```c
    EFI_STATUS printf(const CHAR16 *format, ...) {
        va_list args;
        va_start(args, format);
        for (UINTN i = 0; format[i] != L'\0'; i++) {
            if (format[i] != L'%') {
                _printf_write_char(format[i]);
                continue;
            }
            i++;
            switch (format[i]) {
                case L'u': {
                    UINTN num = va_arg(args, UINTN);
                    _printf_write_uintn(num);
                    break;
                }
                // ... handle other specifiers ...
            }
        }
        va_end(args);
        _printf_flush();
        return EFI_SUCCESS;
    }
    ```)
]


= Initial Setup

== Booting Linux 101

We will need to find the kernel on disk, load it into memory, setup the CPU state how the kernel wants, and jump to the kernel's entry point.

- It's in ELF format, not PE.
- It's compressed, so we need to decompress it to get executable code.
- It's wrapped in a special header that we need to parse to find the entry point.
- We need to tell the kernel how to find the rest of the system (initramfs, the root filesystem, etc.).

The entire process is documented in the *Linux x86 boot protocol specification*,
which we will be following closely.

_Note:_ The kernel can actually do all of this itself using its built-in EFI stub bootloader, but that's boring.

// = Digression 1: A Bootable System

// == Creating a Bootable Disk Image

// To test our bootloader, we'll need some kind of operating system to load.

// - We could load a fresh Linux kernel on its own. This is the most minimal option, but requires a lot of work to set up an initramfs and our own userland.
// - We could install a full-featured Linux distro like Ubuntu or Fedora, but that involves a lot of manual steps and is hard to automate.
// - Or, use Arch Linux, which is a suitable middle ground. Creating a brand new installation in a virtual disk is just a handful of commands and extremely replicable.

// Perhaps more importantly, it gives us a real Linux system to boot into, which is more satisfying than a custom initramfs with just a shell.

// == Virtual Disk Setup

// #slide(composer: (auto, 1fr))[
//     #codeblock(```bash
//     # create an empty 16GB file
//     touch disk.img
//     truncate -s 16G disk.img
//     # create some partitions
//     # "everything is a file",
//     # including disks
//     sfdisk disk.img << EOF
//     label: gpt
//     size=512M, type=uefi
//     size=, type=linux
//     EOF

//     losetup -fP --show disk.img
//     # → /dev/loop0
//     mkfs.fat -F32 /dev/loop0p1
//     mkfs.ext4 /dev/loop0p2
//     ```)
// ][
//     #codeblock(```bash
//     # mount and install Arch Linux
//     mount /dev/loop0p2 /mnt
//     mkdir /mnt/boot
//     mount /dev/loop0p1 /mnt/boot

//     pacstrap -K -c /mnt base base-devel linux \
//         linux-firmware vim neovim networkmanager \
//         gnome gdm

//     arch-chroot /mnt

//     # in chroot:
//     echo 'root:root' | chpasswd
//     systemctl enable gdm NetworkManager systemd-resolved
//     ```)
// ]

= Initial Setup

== Finding the Kernel

We'll need to interact with the available storage devices and find the kernel file on disk. UEFI only guarantees support for FAT filesystems, so the kernel must be on the EFI system partition (unless we want to write/bundle our own ext4 driver).

Here is where everything lives on disk:

#cols(columns: (auto, auto))[
    #codeblock(```
    EFI system partition: FAT32
    ├── EFI/
    │   └── BOOT/
    │       └── BOOTX64.EFI   # our bootloader
    ├── vmlinuz-linux         # the kernel
    └── initramfs-linux.img   # the initramfs
    ```)
][
    #codeblock(```
    Root partition: ext4
    ├── boot/ → mounted ESP
    ├── bin/
    ├── lib/
    ├── usr/
    └── ... # the rest of the Linux system
    ```)
]

== Finding the Kernel

UEFI organizes devices and capabilities using *handles* (just opaque pointers) and _protocols_ (interfaces identified by a GUID). To find the kernel, we need to:

- Ask UEFI for a list of all handles that support the `EFI_SIMPLE_FILE_SYSTEM_PROTOCOL` (i.e. all FAT partitions)

- For each handle, open the filesystem to get the root directory (a `EFI_FILE_PROTOCOL`), then look for `vmlinuz-linux` and `initramfs-linux.img`.

- If we find them, we can use the `EFI_FILE_PROTOCOL` to read bytes into memory.

#codeblock(```c
EFI_HANDLE handles[64];
UINTN handles_size = sizeof(handles);
BS->LocateHandle(ByProtocol, &gEfiSimpleFileSystemProtocolGuid, NULL,
    &handles_size, handles);
```)

== Finding the Kernel

#codeblock(```c
EFI_FILE_PROTOCOL *kernel;
EFI_FILE_PROTOCOL *initramfs;

for (UINTN i = 0; i < handles_size / sizeof(EFI_HANDLE); i++) {
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs;
    BS->HandleProtocol(handles[i], &gEfiSimpleFileSystemProtocolGuid, (VOID **)&fs);
    EFI_FILE_PROTOCOL *root;
    fs->OpenVolume(fs, &root);

    err = root->Open(root, &kernel, L"vmlinuz-linux", EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(err)) continue;
    err = root->Open(root, &initramfs, L"initramfs-linux.img", EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(err)) continue;

    break; // Success!
}
```)

== The Setup Header

Each Linux kernel image contains a special header in a fixed location that contains important information about how to load and start the kernel. We need to parse this header to find the entry point, the size of the setup code, and other details.

We can define this type in C, seek to the right spot, and read it in.

#cols(columns: (auto, 1fr))[
    #codeblock(```c
    #pragma pack(push, 1)
    typedef struct {
        UINT8 setup_sects;
        UINT16 root_flags;
        UINT32 syssize;
        UINT16 ram_size;
       // ... many more fields ...
    } __attribute__((packed)) setup_header_t;
    #pragma pack(pop)
    ```)
][
    #codeblock(```c
    setup_header_t header;
    UINTN size = sizeof(setup_header_t);
    kernel->SetPosition(kernel, 0x1f1);
    kernel->Read(kernel, &size, &header);
    if (header.boot_flag != 0xAA55) {
        return EFI_LOAD_ERROR;
    }
    // use header fields to find entry
    // point, setup code size, etc.
    ```)
]

== The Setup Header

If you have the `file` utility and a Linux kernel handy, you can see a lot of the setup header yourself:

#codeblock(```bash
$ file /boot/vmlinuz-linux
qemu/root/boot/vmlinuz-linux: Linux kernel x86 boot executable, bzImage, version 6.19.14-arch1-1 (linux@archlinux) #1 SMP PREEMPT_DYNAMIC Thu, 23 Apr 2026 06:57:02 +0000, RO-rootFS, Normal VGA, setup size 512*39, syssize 0xfb120, jump 0x26c 0x8cd88ec0fc8cd239 instruction, protocol 2.15, from protected-mode code at offset 0x2cc 0xf7795d bytes ZST compressed, relocatable, handover offset 0xfa3fa0, legacy 64-bit entry point, can be above 4G, 32-bit EFI handoff entry point, 64-bit EFI handoff entry point, EFI kexec boot support, xloadflags bit 5, max cmdline size 2047, init_size 0x4463000
```)

== The Setup Header

Now we can calculate where the kernel payload (the actual executable code) is located in the file and how big it is. Let's read it into memory:

#codeblock(```c
UINTN kernel_size = header->payload_length;
VOID *kernel = malloc(kernel_size); // wrapper around UEFI's memory allocator

// calculate where the payload starts based on the setup header fields
UINTN setup_sectors = header->setup_sects ? header->setup_sects : 4;
UINTN payload_start = (1 + setup_sectors) * 512 + header->payload_offset;

// seek and read the kernel payload into memory
kernel_file->SetPosition(kernel_file, payload_start);
kernel_file->Read(kernel_file, &kernel_size, kernel);
```)

= Zstd Compression

== The Kernel is Compressed

Unfortunately, the kernel payload is compressed on disk to save space :( We have
to decompress it before we can execute it.

The default Arch Linux kernel is compressed with Zstandard (zstd), which is a modern compression algorithm that offers a good balance of speed and compression ratio. We can double check by checking for the zstd magic number at the start of the payload:

#codeblock(```c
if ((*(UINT32 *)kernel) != 0xFD2FB528) {
    // custom printf implementation, of course
    printf(L"Missing zstd magic, is the kernel zstd compressed?\r\n");
    return EFI_LOAD_ERROR;
}
```)

== Bringing Our Own Decompressor

Perhaps unsurprisingly, UEFI doesn't have built-in support for zstd. We have to bring our own decompressor. Writing a zstd decompressor from scratch is a bit too much for this project, but the reference implementation is open source and written in C. It has depends on a lot of standard library features, though. Cross-compiling it for UEFI is also a bit tricky.

#codeblock(style: (..styles, text: (..styles.text, size: 0.5em)), ```bash
make -C vendor/zstd/lib install-pc install-static install-includes \
		CC=clang AR=llvm-ar RANLIB=llvm-ranlib \
		ZSTD_LIB_COMPRESSION=0 ZSTD_LIB_DICTBUILDER=0 ZSTD_LIB_DEPRECATED=0 ZSTD_LEGACY_SUPPORT=0 \
		CFLAGS="-target x86_64-unknown-windows -ffreestanding -fshort-wchar -mno-red-zone -mno-stack-arg-probe -nostdinc \
        -isystem $(clang -print-resource-dir)/include -I./src/shared -I./vendor/shim -I./vendor/include \
        -I./vendor/gnu-efi/inc -I./vendor/gnu-efi/inc/x86_64 -I./vendor/gnu-efi/inc/protocol \
        -UZSTD_MULTITHREAD -DZSTD_HAVE_WEAK_SYMBOLS=0 -DZSTD_DISABLE_ASM=1 -DZSTD_NO_INTRINSICS=1 \
        -U_MSC_VER -DXXH_NO_STDLIB \
        -D_byteswap_ulong=__builtin_bswap32 -D_byteswap_uint64=__builtin_bswap64 -D_byteswap_ushort=__builtin_bswap16\
        -DDEBUGLEVEL=0" \
		LDFLAGS="-target x86_64-unknown-windows -fuse-ld=lld-link"
	cp ./vendor/lib/libzstd.a ./vendor/lib/zstd.lib # fix naming for MSVC compatibility
# Later: make $(shell pkg-config --libs libzstd) to link against it
```)

== Bringing Our Own Decompressor

Now, we can link zstd as a regular shared library and call its decompression functions to get the kernel payload into an executable state in memory.

#codeblock(```c
size_t decompressed_size = ZSTD_findFrameCompressedSize(compressed, compressed_size);
UINTN decompressed_capacity = header->init_size;
UINTN pages = (decompressed_capacity + EFI_PAGE_SIZE - 1) / EFI_PAGE_SIZE;
UINTN alignment = (UINTN)header->kernel_alignment;
UINTN aligned_size = (decompressed_capacity + alignment - 1) & ~(alignment - 1);

VOID *decompressed_kernel;
BS->AllocatePages(AllocateAnyPages, EfiLoaderData, pages, &decompressed_kernel);

ZSTD_decompressDCtx(dctx, decompressed_kernel, decompressed_capacity, compressed,
                    decompressed_size);
```)

== Bringing Our Own Decompressor

We can check our work by looking for the ELF magic number at the start of the decompressed kernel:

#codeblock(```c
if (*(UINT32 *)decompressed_kernel != 0x464c457f) {
    printf(L"Decompression failed, missing ELF magic\r\n");
    return EFI_LOAD_ERROR;
}
```)

The kernel is now in memory and almost ready to execute!

= Initial Setup

== Loading the Initramfs

An _initramfs_ is a small filesystem image that the kernel can use as a temporary root FS during boot, containing drivers and tools needed to mount the real FS.

The process is basically the same as loading the kernel, except we don't need to worry about decompression or anything because the kernel will handle that itself.

#codeblock(```c
EFI_FILE_INFO *file_info = malloc(sizeof(EFI_FILE_INFO) + 256);
initramfs_file->GetInfo(initramfs_file, &gEfiFileInfoGuid, &size, file_info);
UINTN file_size = file_info->FileSize;
free(file_info);

VOID *initramfs;
BS->AllocatePages(AllocateMaxAddress, EfiLoaderData, pages, &initramfs);
initramfs_file->Read(initramfs_file, &file_size, buffer);
```)

= Preparing to Boot

== Setting up boot_params

Next on the agenda is setting up the `boot_params` structure in memory, which is how we pass information to the kernel about the system state, the location of the initramfs, and other details. This is a big struct with a lot of fields, but we only need to fill in a few of them for our purposes.

The most important fields are:

- Where the initramfs is located in memory, and how big it is
- The command line arguments to pass to the kernel (e.g. where the root filesystem is)
- The memory map of the system, which tells the kernel what memory is available and what it's being used for

== Setting up boot_params

#codeblock(style: (..styles, text: (..styles.text, size: 0.7em)), ```c
typedef struct {
    screen_info_t screen_info;  // set so we can see the kernel's boot messages
    UINT8 _pad1[128];
    UINT32 ext_ramdisk_image;   // physical address of the initramfs
    UINT32 ext_ramdisk_size;    // size of the initramfs
    UINT32 ext_cmd_line_ptr;    // pointer to the kernel command line string
    UINT8 _pad2[116];
    edid_info_t edid_info;      // set for kernel boot messages
    efi_info_t efi_info;        // tell the kernel about our UEFI environment
    UINT8 _pad3[8];
    UINT8 e820_entries;         // number of entries in the e820 memory map
    UINT8 _pad4[8];
    setup_header_t hdr;         // copy of the setup header from the kernel
    UINT8 _pad5[100];
    e820_entry_t e820_table[E820_MAX_ENTRIES]; // memory map entries
    UINT8 _pad6[816];
} __attribute__((packed)) boot_params_t;
```)

== Setting up boot_params

We'll fill out what we can from the setup header we read earlier, then we'll fill in the rest as we go.

#codeblock(style: (..styles, text: (..styles.text, size: 0.6em)), ```c
boot_params_t *params = malloc(sizeof(boot_params_t));
memset(params, 0, sizeof(boot_params_t));
// copy setup header into boot_params
memcpy(&params->hdr, header, sizeof(setup_header_t));

params->hdr.ramdisk_image = (UINT32)(UINTN)initramfs;
params->ext_ramdisk_image = (UINT32)((UINTN)initramfs >> 32);
params->hdr.ramdisk_size = (UINT32)initramfs_size;
params->ext_ramdisk_size = (UINT32)(initramfs_size >> 32);
params->hdr.type_of_loader = 0xFF; // custom loader
params->hdr.initrd_addr_max = 0xFFFFFFFF;

// static command line; in a real bootloader you'd want to be let the user specify this
const char *cmdline = "root=/dev/sda2 rw quiet splash";
params->hdr.cmd_line_ptr = (UINT32)(UINTN)cmdline;
params->ext_cmd_line_ptr = (UINT32)((UINTN)cmdline >> 32);params->hdr.cmdline_size = strlen8(cmdline);
```)

== Exiting UEFI Boot Services

We're fairly close to transferring control to the kernel at this point. It's time
to _exit boot services_, which is a special UEFI function that tells the firmware we're done with all our UEFI interactions and ready to boot the OS.

#codeblock(```c
// Get the current memory and map key, which we need to pass to ExitBootServices
BS->GetMemoryMap(&map_size, &memmap, &map_key, &desc_size, &desc_ver);
status = BS->ExitBootServices(handle, map_key);
// ... handle error and retry if needed ...

// We're on our own now! No more BS->* calls, we have to do everything ourselves
// from here on out.
```)

// == Converting the Memory Map

// The kernel also wants to know the current state of the system's memory, so we need to convert the UEFI memory map we just got into the e820 format that the kernel expects.

// #codeblock(```c
// const char *signature = "EL64";
// memcpy(&params->efi_info.efi_loader_signature, signature, sizeof(UINT32));
// params->efi_info.efi_systab = (UINT32)(UINTN)ST;
// params->efi_info.efi_systab_hi = (UINT32)((UINTN)ST >> 32);
// params->efi_info.efi_memdesc_size = desc_size;
// params->efi_info.efi_memdesc_version = desc_ver;
// params->efi_info.efi_memmap = (UINT32)(UINTN)memmap;
// params->efi_info.efi_memmap_hi = (UINT32)((UINTN)memmap >> 32);
// params->efi_info.efi_memmap_size = map_size;
// ```)

// == Converting the Memory Map

// #cols(columns: (1fr, auto))[
//     It's pretty boring, just a loop that converts each UEFI memory descriptor into an e820 entry. If there are more entries than fit in `boot_params`, we overflow to a separate `e820ext` structure (allocated before `ExitBootServies`) and tell the kernel about it in `boot_params`.
// ][
//     #codeblock(style: (..styles, text: (..styles.text, size: 0.5em)), ```c
//     for (UINTN i = 0; i < nr_desc; i++) {
//         e820_type_t e820_type = 0;
//         UINTN map = efi->efi_memmap | ((UINT64)efi->efi_memmap_hi << 32);
//         EFI_MEMORY_DESCRIPTOR *d = ((void *)map + (i * efi->efi_memdesc_size));
//         switch (d->Type) {
//             // ... set e820_type based on d->Type ...
//         }
//         if (nr_entries == E820_MAX_ENTRIES_ZEROPAGE) {
//             entry = (boot_e820_entry_t *)e820ext->data;
//         }
//         entry->addr = d->PhysicalStart;
//         entry->size = d->NumberOfPages << 12;
//         entry->type = e820_type;
//         prev = entry++;
//         nr_entries++;
//     }

//     if (nr_entries > E820_MAX_ENTRIES_ZEROPAGE) {
//         UINT32 nr_e820ext = nr_entries - E820_MAX_ENTRIES_ZEROPAGE;
//         add_e820ext(params, e820ext, nr_e820ext);
//         nr_entries -= nr_e820ext;
//     }

//     params->e820_entries = (UINT8)nr_entries;
//     ```)
// ]

= Transferring Control

== Finding the Entry Point

#cols(columns: (1fr, auto))[
    Almost there! We need to know where exactly to jump to start executing the kernel. We will need to parse the ELF header of the decompressed kernel to find the entry point address; again, pretty boring code.
][
    #codeblock(style: (..styles, text: (..styles.text, size: 0.6em)), ```c
    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)kernel;
    Elf64_Phdr *phdrs = (Elf64_Phdr *)((UINT8 *)kernel + ehdr->e_phoff);
    for (UINTN i = 0; i < ehdr->e_phnum; i++) {
        if (phdrs[i].p_type == 1) { // PT_LOAD
            UINT8 *dest = (UINT8 *)phdrs[i].p_paddr;
            UINT8 *src = (UINT8 *)kernel + phdrs[i].p_offset;
            memmove(dest, src, phdrs[i].p_filesz);
            if (phdrs[i].p_memsz > phdrs[i].p_filesz) {
                memset(
                    dest + phdrs[i].p_filesz, 0,
                    phdrs[i].p_memsz - phdrs[i].p_filesz
                );
            }
        }
    }
    return (VOID *)ehdr->e_entry;
    ```)
]

// == Setting up the CPU

// Now, for all of the CPU state requirements of the Linux boot protocol:

// `At entry, the CPU must be in 64-bit mode with paging enabled.`

// ✔️ UEFI sets this up for us.

// `The range with setup_header.init_size from start address of loaded kernel and zero page and command line buffer get ident mapping.`

// ✔️ Most UEFI allocations are identity-mapped. As long as we load the kernel at a low enough address and allocate the zero page and command line buffer below `initrd_addr_max`, we should be good.

== Setting up the CPU

#cols(columns: (1fr, auto))[
    `A GDT must be loaded with the descriptors for selectors __BOOT_CS(0x10) and __BOOT_DS(0x18); both descriptors must be 4G flat segment; __BOOT_CS must have execute/read permission, and __BOOT_DS must have read/write permission.`\
    Here's where we'll need to start writing some assembly.

    A _Global Descriptor Table_ (GDT) defines memory segments in x86 to define memory segments.

    Each entry is a 64-bit number, and we can just hardcode them since we know exactly what we need.
][
    #codeblock(```asm
    gdt:
        # [0x00] always null
        .quad 0x0000000000000000
        # [0x08] unused
        .quad 0x0000000000000000
        # [0x10] __BOOT_CS
        .quad 0x00AF9A000000FFFF
        # [0x18] __BOOT_DS
        .quad 0x00CF92000000FFFF
    gdtr:
        .word gdtr - gdt - 1
        .quad gdt

    wboot_handoff:
        lgdt gdtr(%rip)
    ```)
]

== Setting up the CPU

#cols(columns: (1fr, auto))[
    `CS must be __BOOT_CS.`

    #text(size: 0.9em)[
        The code segment register (CS) can't be directly modified, but we can do a far jump to reload it.
    ]

    `DS, ES, SS must be __BOOT_DS.`

    #text(size: 0.9em)[
        The data segment registers can be reloaded with simple `mov` instructions.
    ]

    `Interrupts must be disabled.`

    #text(size: 0.9em)[
        A simple `cli` instruction will do the trick.
    ]
][
    #codeblock(style: (..styles, text: (..styles.text, size: 0.6em)), ```asm
    wboot_handoff:
        lgdt gdtr(%rip)
        # CS → __BOOT_CS via far jump
        leaq .Lreload_cs(%rip), %rax
        pushq $0x10
        pushq %rax
        lretq
    .Lreload_cs:
        # DS, ES, SS → __BOOT_DS
        movw $0x18, %ax
        movw %ax, %ds
        movw %ax, %es
        movw %ax, %ss
        # Clear FS and GS just in case
        xorw %ax, %ax
        movw %ax, %fs
        movw %ax, %gs
        # Disable interrupts
        cli
    ```)
]

== Jump!

Finally, we can jump to the kernel's entry point and start executing it!

#codeblock(```asm
# Set rsi to point to boot_params
movq params, %rsi
# Jump to kernel entry point
jmpq *%rdx
```)

And that's it! If everything goes well, the kernel should start booting, load the initramfs, and initialize the system until we get to the login prompt. And if not, we get to debug a bootloader, which is part of the fun :)

= Conclusion

== Thank you!

#set align(horizon)

Code for the entire bootloader, as well build scripts and this\
presentation, is available on GitHub:

#link("https://github.com/calico32/wboot")[
    #text(
        font: "Inter Display",
        size: 1.5em,
        weight: "bold",
        alert("https://github.com/calico32/wboot"),
    )
]

Thank you for listening!
