# Makefile - DELIX Kernel Build System

CC      = gcc
NASM    = nasm
LD      = ld

CFLAGS  = -m32 -ffreestanding -fno-stack-protector -nostdlib -Wall
ASFLAGS = -f elf32
LDFLAGS = -m elf_i386

# Object files (note: terminal_commands.c is now included)
OBJECTS = kernel.o boot.o terminal.o terminal_commands.o net/net.o net/e1000.o net/arp.o net/vfs.o lib/string.o

all: delix-kernel.iso

# Compile C source files
kernel.o: kernel.c terminal.h terminal_commands.h net/net.h
	$(CC) $(CFLAGS) -c $< -o $@

terminal.o: terminal.c terminal.h
	$(CC) $(CFLAGS) -c $< -o $@

terminal_commands.o: terminal_commands.c terminal_commands.h terminal.h
	$(CC) $(CFLAGS) -c $< -o $@

net/net.o: net/net.c net/net.h net/e1000.h net/arp.h
	$(CC) $(CFLAGS) -c $< -o $@

net/e1000.o: net/e1000.c net/e1000.h net/net.h
	$(CC) $(CFLAGS) -c $< -o $@

net/arp.o: net/arp.c net/arp.h net/net.h net/e1000.h
	$(CC) $(CFLAGS) -c $< -o $@

net/vfs.o: net/vfs.c net/vfs.h
	$(CC) $(CFLAGS) -c $< -o $@

lib/string.o: lib/string.c
	$(CC) $(CFLAGS) -c $< -o $@

# Assemble boot code
boot.o: boot.asm
	$(NASM) $(ASFLAGS) $< -o $@

# Link kernel binary
delix-kernel.bin: $(OBJECTS) linker.ld
	$(LD) $(LDFLAGS) -T linker.ld -o $@ $(OBJECTS)

# Create bootable ISO
delix-kernel.iso: delix-kernel.bin
	mkdir -p iso/boot/grub
	cp delix-kernel.bin iso/boot/
	@echo 'set timeout=0' > iso/boot/grub/grub.cfg
	@echo 'set default=0' >> iso/boot/grub/grub.cfg
	@echo 'menuentry "DELIX Kernel" {' >> iso/boot/grub/grub.cfg
	@echo '    multiboot /boot/delix-kernel.bin' >> iso/boot/grub/grub.cfg
	@echo '}' >> iso/boot/grub/grub.cfg
	grub-mkrescue -o $@ iso/

# Run in QEMU with e1000 device
run: delix-kernel.iso
	qemu-system-i386 -cdrom delix-kernel.iso -device e1000,netdev=net0 -netdev user,id=net0

# Clean all build artifacts
clean:
	rm -rf *.o delix-kernel.bin delix-kernel.iso iso/ net/*.o lib/*.o

.PHONY: all run clean
