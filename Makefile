CC := clang

PWD := $(shell pwd)

CLANG_RESOURCE_DIR := $(shell clang -print-resource-dir)

CFLAGS := -target x86_64-unknown-windows \
	-ffreestanding \
	-fshort-wchar \
	-mno-red-zone \
	-mno-stack-arg-probe \
    -nostdinc \
	-isystem $(CLANG_RESOURCE_DIR)/include \
	-I$(PWD)/src/shared \
	-I$(PWD)/vendor/shim \
	-I$(PWD)/vendor/include \
	-I$(PWD)/vendor/gnu-efi/inc \
	-I$(PWD)/vendor/gnu-efi/inc/x86_64 \
	-I$(PWD)/vendor/gnu-efi/inc/protocol

LDFLAGS := -target x86_64-unknown-windows \
	-nostdlib \
	-Wl,-entry:efi_main \
	-Wl,-subsystem:efi_application \
	-fuse-ld=lld-link

TARGET := phase3

%/BOOTX64.EFI: export PKG_CONFIG_PATH := $(PWD)/vendor/lib/pkgconfig
%/BOOTX64.EFI:
	$(MAKE) -C $(@D) BOOTX64.EFI CC=$(CC) CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)"

cc: Makefile
	@bear -o .vscode/compile_commands.json -- $(MAKE) -B src/$(TARGET)/BOOTX64.EFI

zstd:
	$(MAKE) -C vendor/zstd/lib install-pc install-static install-includes PREFIX=$(PWD)/vendor \
		CC=$(CC) \
		AR=llvm-ar \
		RANLIB=llvm-ranlib \
		ZSTD_LIB_COMPRESSION=0 \
		ZSTD_LIB_DICTBUILDER=0 \
		ZSTD_LIB_DEPRECATED=0 \
		ZSTD_LEGACY_SUPPORT=0 \
		CFLAGS="$(CFLAGS) \
			-UZSTD_MULTITHREAD \
			-DZSTD_HAVE_WEAK_SYMBOLS=0 \
			-DZSTD_DISABLE_ASM=1 \
			-DZSTD_NO_INTRINSICS=1 \
			-U_MSC_VER \
			-DXXH_NO_STDLIB \
			-D_byteswap_ulong=__builtin_bswap32 \
			-D_byteswap_uint64=__builtin_bswap64 \
			-D_byteswap_ushort=__builtin_bswap16 \
			-DDEBUGLEVEL=0" \
		LDFLAGS="-target x86_64-unknown-windows -fuse-ld=lld-link"
	cp $(PWD)/vendor/lib/libzstd.a $(PWD)/vendor/lib/zstd.lib

clean:
	rm -f src/**/*.o src/**/BOOTX64.EFI

FIRMWARE_DIR := qemu/firmware
UEFI_DEBUG_LOG := qemu/uefi_debug.log
ESP_DIR := qemu/esp
ROOTIMG := $(PWD)/qemu/root.img
ROOT := $(PWD)/qemu/root
VCPUS ?= $(shell nproc)
QEMU_CPU ?= host

ovmf: export WORKSPACE := $(PWD)/vendor/edk2
ovmf: export EDK_TOOLS_PATH := $(PWD)/vendor/edk2/BaseTools
ovmf: export CONF_PATH := $(PWD)/vendor/edk2/Conf
ovmf: export PATH := "$(PATH):$(PWD)/vendor/edk2/BaseTools/BinWrappers/PosixLike"
ovmf:
	git submodule update --init
	cd vendor/edk2 && \
		git submodule update --init && \
		PATH=${PATH} build -a X64 -t GCC -p OvmfPkg/OvmfPkgX64.dsc -b DEBUG
	mkdir -p $(FIRMWARE_DIR)
	cp vendor/edk2/Build/OvmfX64/DEBUG_GCC/FV/OVMF_*.fd $(FIRMWARE_DIR)

root:
	@printf "Are you sure? Erasing $(ROOTIMG) and creating a new root filesystem. [y/N] "
	@read -r answer && [ "$$answer" = "y" ] || exit 1
	./scripts/mkroot.sh $(ROOTIMG)
	./scripts/mount.sh $(ROOTIMG) $(ROOT) $(ESP_DIR)
	$(MAKE) root-install
	$(MAKE) root-chpasswd
	$(MAKE) root-genfstab
	$(MAKE) root-services
	./scripts/unmount.sh $(ROOT) $(ESP_DIR)

root-install:
	sudo pacstrap -K -c $(ROOT) base base-devel linux linux-firmware vim neovim networkmanager gnome gdm

root-chpasswd:
	sudo arch-chroot $(ROOT) /bin/bash -c "echo 'root:root' | chpasswd"

root-genfstab:
	echo "/dev/vda2 / ext4 defaults 0 1" | sudo tee -a $(ROOT)/etc/fstab

root-services:
	sudo arch-chroot $(ROOT) /bin/bash -c "systemctl enable NetworkManager systemd-resolved"
	sudo arch-chroot $(ROOT) /bin/bash -c "systemctl enable gdm"

mount:
	./scripts/mount.sh $(ROOTIMG) $(ROOT) $(ESP_DIR)

unmount:
	./scripts/unmount.sh $(ROOT) $(ESP_DIR)

QEMUFLAGS := -machine q35,accel=kvm:tcg,kernel-irqchip=on \
	-cpu $(QEMU_CPU) \
	-smp $(VCPUS) \
	-drive if=pflash,format=raw,unit=0,file=$(FIRMWARE_DIR)/OVMF_CODE.fd,readonly=on \
	-drive if=pflash,format=raw,unit=1,file=$(FIRMWARE_DIR)/OVMF_VARS.fd \
	-debugcon file:$(UEFI_DEBUG_LOG) -global isa-debugcon.iobase=0x402 \
	-drive if=virtio,format=raw,file=$(ROOTIMG),cache=none,aio=native,discard=unmap,detect-zeroes=unmap \
	-m 8G \
	-netdev user,id=net0 -device virtio-net-pci,netdev=net0 \
	-serial stdio

run: src/$(TARGET)/BOOTX64.EFI
	sudo mkdir -p $(ESP_DIR)/EFI/BOOT
	sudo cp src/$(TARGET)/BOOTX64.EFI $(ESP_DIR)/EFI/BOOT/BOOTX64.EFI
# wait for the file to be fully written before starting QEMU, otherwise OVMF might try to read it before it's done writing and fail to boot
	sync
	@truncate -s 0 $(UEFI_DEBUG_LOG)
	@qemu-system-x86_64 $(QEMUFLAGS)

run-direct:
	sync
	@truncate -s 0 $(UEFI_DEBUG_LOG)
	@qemu-system-x86_64 $(QEMUFLAGS) \
		-kernel qemu/root/boot/vmlinuz-linux \
		-initrd qemu/root/boot/initramfs-linux.img \
		-append "rw root=/dev/vda2 console=ttyS0"

.PHONY: cc clean run ovmf root mount unmount zstd
