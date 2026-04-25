BOOTX64.EFI: main.o wboot.o wboot_config.o wboot_graphics.o wboot_handoff.s ../shared/efiglobal.o ../shared/wstdlib.o
	$(CC) $(LDFLAGS) $(shell pkg-config --libs libzstd) -o $@ $^
