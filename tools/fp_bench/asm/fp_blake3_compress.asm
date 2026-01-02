; fp_blake3_compress.asm
; BLAKE3 compression (word-based) following FP_ASM_LIB conventions.

bits 64
default rel

%define FLAGS_OFFSET 0xD0
%define OUT_OFFSET   0xD8

section .rdata
align 32
iv:
    dd 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a
    dd 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19

msg_schedule:
    db 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
    db 2, 6, 3, 10, 7, 0, 4, 13, 1, 11, 12, 5, 9, 14, 15, 8
    db 3, 4, 10, 12, 13, 2, 7, 14, 6, 5, 9, 0, 11, 15, 8, 1
    db 10, 7, 12, 9, 14, 3, 13, 15, 4, 0, 11, 2, 5, 8, 1, 6
    db 12, 13, 9, 11, 15, 10, 14, 8, 7, 2, 5, 3, 0, 1, 6, 4
    db 9, 14, 11, 5, 8, 12, 15, 1, 13, 3, 0, 10, 2, 6, 4, 7
    db 11, 15, 5, 0, 1, 9, 8, 6, 14, 10, 2, 12, 3, 4, 7, 13

section .text

%macro G 6
    mov eax, [r13 + 4*%1]
    add eax, [r13 + 4*%2]
    add eax, %5
    mov [r13 + 4*%1], eax

    mov edx, [r13 + 4*%4]
    xor edx, eax
    ror edx, 16
    mov [r13 + 4*%4], edx

    mov ecx, [r13 + 4*%3]
    add ecx, edx
    mov [r13 + 4*%3], ecx

    mov r10d, [r13 + 4*%2]
    xor r10d, ecx
    ror r10d, 12
    mov [r13 + 4*%2], r10d

    mov eax, [r13 + 4*%1]
    add eax, [r13 + 4*%2]
    add eax, %6
    mov [r13 + 4*%1], eax

    mov edx, [r13 + 4*%4]
    xor edx, eax
    ror edx, 8
    mov [r13 + 4*%4], edx

    mov ecx, [r13 + 4*%3]
    add ecx, edx
    mov [r13 + 4*%3], ecx

    mov r10d, [r13 + 4*%2]
    xor r10d, ecx
    ror r10d, 7
    mov [r13 + 4*%2], r10d
%endmacro

global fp_blake3_compress_words_asm
fp_blake3_compress_words_asm:
    push rbp
    mov rbp, rsp
    push rbx
    push r12
    push r13
    push r14
    sub rsp, 128

    mov r14, rcx
    mov rbx, rdx
    lea r12, [rsp]
    lea r13, [rsp + 64]

    mov eax, [r14 + 0]
    mov [r13 + 0], eax
    mov eax, [r14 + 4]
    mov [r13 + 4], eax
    mov eax, [r14 + 8]
    mov [r13 + 8], eax
    mov eax, [r14 + 12]
    mov [r13 + 12], eax
    mov eax, [r14 + 16]
    mov [r13 + 16], eax
    mov eax, [r14 + 20]
    mov [r13 + 20], eax
    mov eax, [r14 + 24]
    mov [r13 + 24], eax
    mov eax, [r14 + 28]
    mov [r13 + 28], eax

    mov eax, [rel iv + 0]
    mov [r13 + 32], eax
    mov eax, [rel iv + 4]
    mov [r13 + 36], eax
    mov eax, [rel iv + 8]
    mov [r13 + 40], eax
    mov eax, [rel iv + 12]
    mov [r13 + 44], eax

    mov eax, r8d
    mov [r13 + 48], eax
    mov rax, r8
    shr rax, 32
    mov [r13 + 52], eax

    mov eax, r9d
    mov [r13 + 56], eax
    mov eax, dword [rsp + FLAGS_OFFSET]
    mov [r13 + 60], eax

    xor r11d, r11d
.round_loop:
    lea r10, [rel msg_schedule]
    mov rax, r11
    shl rax, 4
    add r10, rax

    xor ecx, ecx
.perm_loop:
    movzx eax, byte [r10 + rcx]
    mov edx, [rbx + rax*4]
    mov [r12 + rcx*4], edx
    inc ecx
    cmp ecx, 16
    jne .perm_loop

    mov r8d, [r12 + 0]
    mov r9d, [r12 + 4]
    G 0, 4, 8, 12, r8d, r9d

    mov r8d, [r12 + 8]
    mov r9d, [r12 + 12]
    G 1, 5, 9, 13, r8d, r9d

    mov r8d, [r12 + 16]
    mov r9d, [r12 + 20]
    G 2, 6, 10, 14, r8d, r9d

    mov r8d, [r12 + 24]
    mov r9d, [r12 + 28]
    G 3, 7, 11, 15, r8d, r9d

    mov r8d, [r12 + 32]
    mov r9d, [r12 + 36]
    G 0, 5, 10, 15, r8d, r9d

    mov r8d, [r12 + 40]
    mov r9d, [r12 + 44]
    G 1, 6, 11, 12, r8d, r9d

    mov r8d, [r12 + 48]
    mov r9d, [r12 + 52]
    G 2, 7, 8, 13, r8d, r9d

    mov r8d, [r12 + 56]
    mov r9d, [r12 + 60]
    G 3, 4, 9, 14, r8d, r9d

    inc r11d
    cmp r11d, 7
    jne .round_loop

    mov rdx, [rsp + OUT_OFFSET]

    mov eax, [r13 + 0]
    mov ecx, [r13 + 32]
    xor eax, ecx
    mov [rdx + 0], eax
    xor ecx, [r14 + 0]
    mov [rdx + 32], ecx

    mov eax, [r13 + 4]
    mov ecx, [r13 + 36]
    xor eax, ecx
    mov [rdx + 4], eax
    xor ecx, [r14 + 4]
    mov [rdx + 36], ecx

    mov eax, [r13 + 8]
    mov ecx, [r13 + 40]
    xor eax, ecx
    mov [rdx + 8], eax
    xor ecx, [r14 + 8]
    mov [rdx + 40], ecx

    mov eax, [r13 + 12]
    mov ecx, [r13 + 44]
    xor eax, ecx
    mov [rdx + 12], eax
    xor ecx, [r14 + 12]
    mov [rdx + 44], ecx

    mov eax, [r13 + 16]
    mov ecx, [r13 + 48]
    xor eax, ecx
    mov [rdx + 16], eax
    xor ecx, [r14 + 16]
    mov [rdx + 48], ecx

    mov eax, [r13 + 20]
    mov ecx, [r13 + 52]
    xor eax, ecx
    mov [rdx + 20], eax
    xor ecx, [r14 + 20]
    mov [rdx + 52], ecx

    mov eax, [r13 + 24]
    mov ecx, [r13 + 56]
    xor eax, ecx
    mov [rdx + 24], eax
    xor ecx, [r14 + 24]
    mov [rdx + 56], ecx

    mov eax, [r13 + 28]
    mov ecx, [r13 + 60]
    xor eax, ecx
    mov [rdx + 28], eax
    xor ecx, [r14 + 28]
    mov [rdx + 60], ecx

    add rsp, 128
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret
