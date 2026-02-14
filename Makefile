CC = gcc
CFLAGS = -static -Wall
BINARY = rootfs/init
CPID_FILE = rootfs/initramfs.cpio
KERNEL = /boot/vmlinuz-$(shell uname -r)
ROOTFS = rootfs
CORE_SERVICES = rootfs/bin/init.d/core_services
CTL_BINARY  = rootfs/bin/initctl

all: $(CORE_SERVICES) $(CPID_FILE) $(BINARY) $(CTL_BINARY)

$(CORE_SERVICES):
	mkdir -p $(CORE_SERVICES)
	$(CC) $(CFLAGS)  src/mount_pfs.c -o $(CORE_SERVICES)/mount_pfs
	$(CC) $(CFLAGS)  src/up_lo.c -o $(CORE_SERVICES)/up_lo
	$(CC) $(CFLAGS)  src/set_hostname.c -o $(CORE_SERVICES)/set_hostname
	$(CC) $(CFLAGS)  src/seed_entropy.c -o $(CORE_SERVICES)/seed_entropy
	$(CC) $(CFLAGS)  src/systohc.c -o $(CORE_SERVICES)/systohc
	$(CC) $(CFLAGS)  src/loadkeys_setfont.c -o $(CORE_SERVICES)/loadkeys_setfont


$(CTL_BINARY): src/ctl.c
	$(CC) $(CFLAGS)  src/ctl.c -o $(CTL_BINARY)


# 1. Compile only the init code
$(BINARY): src/init.c
	$(CC) $(CFLAGS) src/phase_one.c src/phase_three.c src/sv.c src/init.c -o $(BINARY)

# 2. Pack the archive using the rootfs directory
$(CPID_FILE): $(BINARY) $(CORE_SERVICES) $(CTL_BINARY)
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
	rm -rf $(BINARY) $(CPID_FILE) $(CORE_SERVICES) $(CTL_BINARY)

.PHONY: all run clean
