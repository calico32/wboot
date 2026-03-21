CC := clang

CFLAGS := -target x86_64-unknown-windows \
	-ffreestanding \
	-fshort-wchar \
	-mno-red-zone \
	-I./gnu-efi/inc \
	-I./gnu-efi/inc/x86_64 \
	-I./gnu-efi/inc/protocol

LDFLAGS := -target x86_64-unknown-windows \
	-nostdlib \
	-Wl,-entry:efi_main \
	-Wl,-subsystem:efi_application \
	-fuse-ld=lld-link

TARGET := phase1

src/phase1/BOOTX64.EFI: src/phase1/data.o src/phase1/main.o
	$(CC) $(LDFLAGS) -o $@ $^

cc: Makefile
	@bear -o .vscode/compile_commands.json -- $(MAKE) -B src/$(TARGET)/BOOTX64.EFI

clean:
	rm -f src/**/*.o src/**/BOOTX64.EFI

FIRMWARE_DIR := qemu/firmware
UEFI_DEBUG_LOG := qemu/uefi_debug.log
ESP_DIR := qemu/esp

ovmf:
	git submodule update --init
	WORKSPACE=$(pwd)/edk2 \
	EDK_TOOLS_PATH=$(pwd)/edk2/BaseTools \
	CONF_PATH=$(pwd)/edk2/Conf \
	PATH="$(PATH)":$(pwd)/edk2/BaseTools/BinWrappers/PosixLike \
		cd edk2 && \
		git submodule update --init && \
		build -a X64 -t GCC -p OvmfPkg/OvmfPkgX64.dsc -b DEBUG
	mkdir -p $(FIRMWARE_DIR)
	cp edk2/Build/OvmfX64/DEBUG_GCC/FV/OVMF_*.fd $(FIRMWARE_DIR)

run: src/$(TARGET)/BOOTX64.EFI
	@mkdir -p $(ESP_DIR)/EFI/BOOT
	@cp src/$(TARGET)/BOOTX64.EFI $(ESP_DIR)/EFI/BOOT/BOOTX64.EFI
	@truncate -s 0 $(UEFI_DEBUG_LOG)
	@qemu-system-x86_64 \
		-cpu qemu64 \
		-drive if=pflash,format=raw,unit=0,file=$(FIRMWARE_DIR)/OVMF_CODE.fd,readonly=on \
		-drive if=pflash,format=raw,unit=1,file=$(FIRMWARE_DIR)/OVMF_VARS.fd \
		-debugcon file:$(UEFI_DEBUG_LOG) -global isa-debugcon.iobase=0x402 \
		-drive format=raw,file=fat:rw:$(ESP_DIR) \
		-net none

.PHONY: cc clean run ovmf
