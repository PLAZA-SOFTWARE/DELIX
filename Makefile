# Makefile - DELIX Kernel Build System (build ISO for x86 and x86_64 VM)

CC      = gcc
NASM    = nasm
LD      = ld
OBJCOPY = objcopy

CFLAGS  = -m32 -ffreestanding -fno-stack-protector -nostdlib -Wall
ASFLAGS = -f elf32
LDFLAGS = -m elf_i386

# Object files
OBJECTS = kernel.o boot.o terminal.o terminal_commands.o net/net.o net/e1000.o net/arp.o net/vfs.o lib/string.o

# Default: build both ISOs
all: iso32 iso64

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

# Link flat binary (ELF -> raw binary)
delix-kernel.elf: $(OBJECTS) linker.ld
	$(LD) $(LDFLAGS) -T linker.ld -o $@ $(OBJECTS)

delix-kernel.bin: delix-kernel.elf
	$(OBJCOPY) -O binary $< $@

# Create bootable ISOs (x86 and x86_64) using same kernel binary
iso32: delix-kernel.bin
	mkdir -p iso/boot/grub
	cp delix-kernel.bin iso/boot/
	@echo 'set timeout=0' > iso/boot/grub/grub.cfg
	@echo 'set default=0' >> iso/boot/grub/grub.cfg
	@echo 'menuentry "DELIX Kernel (i386)" {' >> iso/boot/grub/grub.cfg
	@echo '    multiboot /boot/delix-kernel.bin' >> iso/boot/grub/grub.cfg
	@echo '}' >> iso/boot/grub/grub.cfg
	grub-mkrescue -o delix-kernel-x86.iso iso/

iso64: delix-kernel.bin
	mkdir -p iso/boot/grub
	cp delix-kernel.bin iso/boot/
	@echo 'set timeout=0' > iso/boot/grub/grub.cfg
	@echo 'set default=0' >> iso/boot/grub/grub.cfg
	@echo 'menuentry "DELIX Kernel (x86_64 VM)" {' >> iso/boot/grub/grub.cfg
	@echo '    multiboot /boot/delix-kernel.bin' >> iso/boot/grub/grub.cfg
	@echo '}' >> iso/boot/grub/grub.cfg
	grub-mkrescue -o delix-kernel-x64.iso iso/

# Run targets
run32: iso32
	qemu-system-i386 -cdrom delix-kernel-x86.iso

run64: iso64
	qemu-system-x86_64 -cdrom delix-kernel-x64.iso

# Clean all build artifacts
clean:
	rm -rf *.o delix-kernel.elf delix-kernel.bin delix-kernel-x86.iso delix-kernel-x64.iso iso/ net/*.o lib/*.o

.PHONY: all iso32 iso64 run32 run64 clean
