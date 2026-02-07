CC = gcc
CFLAGS = -static -Wall
BINARY = rootfs/init
CPID_FILE = rootfs/initramfs.cpio
KERNEL = /boot/vmlinuz-$(shell uname -r)
ROOTFS = rootfs

all: $(CPID_FILE)

# 1. Compile only the init code
$(BINARY): init.c
	$(CC) $(CFLAGS) init.c -o $(BINARY)

# 2. Pack the archive using the rootfs directory
$(CPID_FILE): $(BINARY)
	@echo "Updating init in rootfs..."
	mkdir -p $(ROOTFS)/proc $(ROOTFS)/sys $(ROOTFS)/dev
	@echo "Packing initramfs..."
	cd $(ROOTFS) && find . | cpio -o -H newc > ../$(CPID_FILE)

run: $(CPID_FILE)
	qemu-system-x86_64 \
		-kernel $(KERNEL) \
		-initrd $(CPID_FILE) \
		-append "init=/init console=ttyS0 quiet" \
		-nographic

clean:
	rm -f $(BINARY) $(CPID_FILE)

.PHONY: all run clean
