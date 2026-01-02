; fp_blake3_compress.asm
; BLAKE3 compression (word-based) following FP_ASM_LIB conventions.

bits 64
default rel

%include "macros.inc"

%define FLAGS_OFFSET 0xD0
%define OUT_OFFSET   0xD8

%define MSG_OFFSET 0
%define SPILL14_OFFSET 512
%define SPILL15_OFFSET 544
%define SAVE_YMM14_OFFSET 576
%define SAVE_YMM15_OFFSET 608
%define LOCAL_SIZE 640
%define BLOCK_LEN 64

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

%macro TRANSPOSE4 0
    vpunpckldq xmm4, xmm0, xmm1
    vpunpckhdq xmm5, xmm0, xmm1
    vpunpckldq xmm6, xmm2, xmm3
    vpunpckhdq xmm7, xmm2, xmm3
    vpunpcklqdq xmm0, xmm4, xmm6
    vpunpckhqdq xmm1, xmm4, xmm6
    vpunpcklqdq xmm2, xmm5, xmm7
    vpunpckhqdq xmm3, xmm5, xmm7
%endmacro

%macro TRANSPOSE4_TMP8 0
    vpunpckldq xmm8, xmm0, xmm1
    vpunpckhdq xmm9, xmm0, xmm1
    vpunpckldq xmm10, xmm2, xmm3
    vpunpckhdq xmm11, xmm2, xmm3
    vpunpcklqdq xmm0, xmm8, xmm10
    vpunpckhqdq xmm1, xmm8, xmm10
    vpunpcklqdq xmm2, xmm9, xmm11
    vpunpckhqdq xmm3, xmm9, xmm11
%endmacro

%macro LOAD_MSG_GROUP 2
    vmovdqu xmm0, [rax + %1]
    vmovdqu xmm1, [rcx + %1]
    vmovdqu xmm2, [rdx + %1]
    vmovdqu xmm3, [r8 + %1]
    TRANSPOSE4
    vperm2i128 ymm0, ymm0, ymm0, 0x00
    vperm2i128 ymm1, ymm1, ymm1, 0x00
    vperm2i128 ymm2, ymm2, ymm2, 0x00
    vperm2i128 ymm3, ymm3, ymm3, 0x00
    vmovdqu [rbx + (%2 + 0)*32], ymm0
    vmovdqu [rbx + (%2 + 1)*32], ymm1
    vmovdqu [rbx + (%2 + 2)*32], ymm2
    vmovdqu [rbx + (%2 + 3)*32], ymm3
%endmacro

%macro G_AVX2 6
    vpaddd %1, %1, %2
    vpaddd %1, %1, [rbx + %5*32]
    vpxor %4, %4, %1
    vpsrld ymm15, %4, 16
    vpslld %4, %4, 16
    vpor %4, %4, ymm15
    vpaddd %3, %3, %4
    vpxor %2, %2, %3
    vpsrld ymm15, %2, 12
    vpslld %2, %2, 20
    vpor %2, %2, ymm15
    vpaddd %1, %1, %2
    vpaddd %1, %1, [rbx + %6*32]
    vpxor %4, %4, %1
    vpsrld ymm15, %4, 8
    vpslld %4, %4, 24
    vpor %4, %4, ymm15
    vpaddd %3, %3, %4
    vpxor %2, %2, %3
    vpsrld ymm15, %2, 7
    vpslld %2, %2, 25
    vpor %2, %2, ymm15
%endmacro

%macro G_AVX2_SPILL 6
    vmovdqu ymm14, [rsp + %4]
    G_AVX2 %1, %2, %3, ymm14, %5, %6
    vmovdqu [rsp + %4], ymm14
%endmacro

%macro ROUND_AVX2 16
    G_AVX2 ymm0, ymm4, ymm8, ymm12, %1, %2
    G_AVX2 ymm1, ymm5, ymm9, ymm13, %3, %4
    G_AVX2_SPILL ymm2, ymm6, ymm10, SPILL14_OFFSET, %5, %6
    G_AVX2_SPILL ymm3, ymm7, ymm11, SPILL15_OFFSET, %7, %8

    G_AVX2_SPILL ymm0, ymm5, ymm10, SPILL15_OFFSET, %9, %10
    G_AVX2 ymm1, ymm6, ymm11, ymm12, %11, %12
    G_AVX2 ymm2, ymm7, ymm8, ymm13, %13, %14
    G_AVX2_SPILL ymm3, ymm4, ymm9, SPILL14_OFFSET, %15, %16
%endmacro

global fp_blake3_compress4_asm
fp_blake3_compress4_asm:
    PROLOGUE
    sub rsp, LOCAL_SIZE
    vmovdqu [rsp + SAVE_YMM14_OFFSET], ymm14
    vmovdqu [rsp + SAVE_YMM15_OFFSET], ymm15

    lea rbx, [rsp + MSG_OFFSET]

    mov r10, rcx
    lea r11, [rcx + 32]
    lea r12, [rcx + 64]
    lea r13, [rcx + 96]

    mov r14, rdx
    mov r15, r8

    mov rax, [r14]
    mov rcx, [r14 + 8]
    mov rdx, [r14 + 16]
    mov r8, [r14 + 24]

    LOAD_MSG_GROUP 0, 0
    LOAD_MSG_GROUP 16, 4
    LOAD_MSG_GROUP 32, 8
    LOAD_MSG_GROUP 48, 12

    vmovdqu xmm0, [r10 + 16]
    vmovdqu xmm1, [r11 + 16]
    vmovdqu xmm2, [r12 + 16]
    vmovdqu xmm3, [r13 + 16]
    TRANSPOSE4
    vperm2i128 ymm0, ymm0, ymm0, 0x00
    vperm2i128 ymm1, ymm1, ymm1, 0x00
    vperm2i128 ymm2, ymm2, ymm2, 0x00
    vperm2i128 ymm3, ymm3, ymm3, 0x00
    vmovdqu ymm4, ymm0
    vmovdqu ymm5, ymm1
    vmovdqu ymm6, ymm2
    vmovdqu ymm7, ymm3

    vmovdqu xmm0, [r10 + 0]
    vmovdqu xmm1, [r11 + 0]
    vmovdqu xmm2, [r12 + 0]
    vmovdqu xmm3, [r13 + 0]
    TRANSPOSE4_TMP8
    vperm2i128 ymm0, ymm0, ymm0, 0x00
    vperm2i128 ymm1, ymm1, ymm1, 0x00
    vperm2i128 ymm2, ymm2, ymm2, 0x00
    vperm2i128 ymm3, ymm3, ymm3, 0x00

    vpbroadcastd ymm8, [rel iv + 0]
    vpbroadcastd ymm9, [rel iv + 4]
    vpbroadcastd ymm10, [rel iv + 8]
    vpbroadcastd ymm11, [rel iv + 12]

    vmovd xmm12, dword [r15 + 0]
    vpinsrd xmm12, dword [r15 + 8], 1
    vpinsrd xmm12, dword [r15 + 16], 2
    vpinsrd xmm12, dword [r15 + 24], 3
    vperm2i128 ymm12, ymm12, ymm12, 0x00

    vmovd xmm13, dword [r15 + 4]
    vpinsrd xmm13, dword [r15 + 12], 1
    vpinsrd xmm13, dword [r15 + 20], 2
    vpinsrd xmm13, dword [r15 + 28], 3
    vperm2i128 ymm13, ymm13, ymm13, 0x00

    mov eax, BLOCK_LEN
    vmovd xmm14, eax
    vpbroadcastd ymm14, xmm14
    vmovdqu [rsp + SPILL14_OFFSET], ymm14

    vmovd xmm15, r9d
    vpbroadcastd ymm15, xmm15
    vmovdqu [rsp + SPILL15_OFFSET], ymm15

    ROUND_AVX2 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
    ROUND_AVX2 2, 6, 3, 10, 7, 0, 4, 13, 1, 11, 12, 5, 9, 14, 15, 8
    ROUND_AVX2 3, 4, 10, 12, 13, 2, 7, 14, 6, 5, 9, 0, 11, 15, 8, 1
    ROUND_AVX2 10, 7, 12, 9, 14, 3, 13, 15, 4, 0, 11, 2, 5, 8, 1, 6
    ROUND_AVX2 12, 13, 9, 11, 15, 10, 14, 8, 7, 2, 5, 3, 0, 1, 6, 4
    ROUND_AVX2 9, 14, 11, 5, 8, 12, 15, 1, 13, 3, 0, 10, 2, 6, 4, 7
    ROUND_AVX2 11, 15, 5, 0, 1, 9, 8, 6, 14, 10, 2, 12, 3, 4, 7, 13

    vmovdqu ymm14, [rsp + SPILL14_OFFSET]
    vmovdqu ymm15, [rsp + SPILL15_OFFSET]

    vpxor ymm0, ymm0, ymm8
    vpxor ymm1, ymm1, ymm9
    vpxor ymm2, ymm2, ymm10
    vpxor ymm3, ymm3, ymm11
    vpxor ymm4, ymm4, ymm12
    vpxor ymm5, ymm5, ymm13
    vpxor ymm6, ymm6, ymm14
    vpxor ymm7, ymm7, ymm15

    vextracti128 xmm0, ymm0, 0
    vextracti128 xmm1, ymm1, 0
    vextracti128 xmm2, ymm2, 0
    vextracti128 xmm3, ymm3, 0
    TRANSPOSE4_TMP8
    vmovdqu [r10 + 0], xmm0
    vmovdqu [r11 + 0], xmm1
    vmovdqu [r12 + 0], xmm2
    vmovdqu [r13 + 0], xmm3

    vextracti128 xmm0, ymm4, 0
    vextracti128 xmm1, ymm5, 0
    vextracti128 xmm2, ymm6, 0
    vextracti128 xmm3, ymm7, 0
    TRANSPOSE4
    vmovdqu [r10 + 16], xmm0
    vmovdqu [r11 + 16], xmm1
    vmovdqu [r12 + 16], xmm2
    vmovdqu [r13 + 16], xmm3

    vmovdqu ymm14, [rsp + SAVE_YMM14_OFFSET]
    vmovdqu ymm15, [rsp + SAVE_YMM15_OFFSET]
    add rsp, LOCAL_SIZE
    EPILOGUE
