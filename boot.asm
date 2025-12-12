; boot.asm - Minimal Multiboot-compliant entry point

section .multiboot
align 4
dd 0x1BADB002          ; Multiboot magic
dd 0x0                 ; Flags (no special features)
dd -0x1BADB002         ; Checksum

section .text
global _start
extern kernel_main

_start:
    cli
    mov esp, stack_top

    ; Setup simple paging: identity-map first 4MB (kernel area) and high MMIO at 0xF0000000
    ; Create page directory and enable 4MB pages (PSE)
    ; page_dir is aligned in .bss below
    ; load page directory address into CR3, enable PSE in CR4 and PG in CR0

    ; load address of page_dir into eax
    mov eax, page_dir
    ; Clear page directory memory (optional, ensure zeros)
    mov ecx, 1024
    mov edi, eax
    xor eax, eax
    rep stosd

    ; set PDE 0 -> map 0x00000000 4MB with flags present|rw|ps (0x83)
    mov eax, 0x00000083
    mov dword [page_dir], eax

    ; set PDE for 0xF0000000: index = 0xF0000000 >> 22 = 960
    mov eax, 0xF0000000
    or eax, 0x83            ; flags present|rw|ps
    ; compute offset = 960*4 = 3840
    mov ebx, 3840
    lea edi, [page_dir + ebx]
    mov [edi], eax

    ; Load page directory into CR3
    mov eax, page_dir
    mov cr3, eax

    ; Enable PSE (bit 4) in CR4
    mov eax, cr4
    or eax, 0x10
    mov cr4, eax

    ; Enable paging (set PG bit in CR0)
    mov eax, cr0
    or eax, 0x80000000
    mov cr0, eax

    call kernel_main
    hlt
.loop:
    jmp .loop

section .bss
align 16
stack_bottom:
    resb 8192          ; 8 KB stack
stack_top:

; Page directory aligned to 4KB
align 4096
page_dir:
    times 1024 dd 0
