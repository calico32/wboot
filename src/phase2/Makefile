BOOTX64.EFI: main.o boot.o ../shared/efiglobal.o ../shared/wstdlib.o
	$(CC) $(LDFLAGS) $(shell pkg-config --libs libzstd) -o $@ $^
