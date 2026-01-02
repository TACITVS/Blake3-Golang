; =============================================================================
; FP-ASM Library: Standardized Prologue/Epilogue Macros
; WINDOWS x64 ABI PATCHED VERSION
; =============================================================================

; --- Helper Macros ---
%macro PROLOGUE 0
    push    rbp
    mov     rbp, rsp
    push    rbx
    push    r12
    push    r13
    push    r14
    push    r15
    
    ; WINDOWS FIX: Fixed stack allocation (no dynamic alignment)
    ; 56 bytes pushed + ret addr. Need 8 mod 16.
    ; 264 = 256 + 8.
    sub     rsp, 264

    vmovdqu [rsp],      ymm6
    vmovdqu [rsp+32],   ymm7
    vmovdqu [rsp+64],   ymm8
    vmovdqu [rsp+96],   ymm9
    vmovdqu [rsp+128],  ymm10
    vmovdqu [rsp+160],  ymm11
    vmovdqu [rsp+192],  ymm12
    vmovdqu [rsp+224],  ymm13
%endmacro

%macro EPILOGUE 0
    vmovdqu ymm6,   [rsp]
    vmovdqu ymm7,   [rsp+32]
    vmovdqu ymm8,   [rsp+64]
    vmovdqu ymm9,   [rsp+96]
    vmovdqu ymm10,  [rsp+128]
    vmovdqu ymm11,  [rsp+160]
    vmovdqu ymm12,  [rsp+192]
    vmovdqu ymm13,  [rsp+224]

    add     rsp, 264
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbx
    pop     rbp
    vzeroupper
    ret
%endmacro

; --- Horizontal Reduction Macros ---

; Horizontal sum of 4x f32 in XMM register
%macro HSUM_F32_XMM 1
    vhaddps xmm%1, xmm%1, xmm%1
    vhaddps xmm%1, xmm%1, xmm%1
%endmacro

; Horizontal sum of 8x f32 in YMM register
%macro HSUM_F32_YMM 2
    vextractf128 xmm%2, ymm%1, 1
    vaddps xmm%1, xmm%1, xmm%2
    HSUM_F32_XMM %1
%endmacro

; Horizontal sum of 4x i32 in XMM register
%macro HSUM_I32_XMM 2
    vpshufd xmm%2, xmm%1, 0x4E
    vpaddd xmm%1, xmm%1, xmm%2
    vpshufd xmm%2, xmm%1, 0xB1
    vpaddd xmm%1, xmm%1, xmm%2
%endmacro

; Horizontal sum of 8x i32 in YMM register
%macro HSUM_I32_YMM 2
    vextracti128 xmm%2, ymm%1, 1
    vpaddd xmm%1, xmm%1, xmm%2
    HSUM_I32_XMM %1, %2
%endmacro

; Horizontal product of 4x i32 in XMM register
%macro HPROD_I32_XMM 2
    vpshufd xmm%2, xmm%1, 0x4E
    vpmulld xmm%1, xmm%1, xmm%2
    vpshufd xmm%2, xmm%1, 0xB1
    vpmulld xmm%1, xmm%1, xmm%2
%endmacro

; Horizontal product of 8x i32 in YMM register
%macro HPROD_I32_YMM 2
    vextracti128 xmm%2, ymm%1, 1
    vpmulld xmm%1, xmm%1, xmm%2
    HPROD_I32_XMM %1, %2
%endmacro

; Horizontal min of 4x i32 in XMM register
%macro HMIN_I32_XMM 2
    vpshufd xmm%2, xmm%1, 0x4E
    vpminsd xmm%1, xmm%1, xmm%2
    vpshufd xmm%2, xmm%1, 0xB1
    vpminsd xmm%1, xmm%1, xmm%2
%endmacro

; Horizontal min of 8x i32 in YMM register
%macro HMIN_I32_YMM 2
    vextracti128 xmm%2, ymm%1, 1
    vpminsd xmm%1, xmm%1, xmm%2
    HMIN_I32_XMM %1, %2
%endmacro

; Horizontal max of 4x i32 in XMM register
%macro HMAX_I32_XMM 2
    vpshufd xmm%2, xmm%1, 0x4E
    vpmaxsd xmm%1, xmm%1, xmm%2
    vpshufd xmm%2, xmm%1, 0xB1
    vpmaxsd xmm%1, xmm%1, xmm%2
%endmacro

; Horizontal max of 8x i32 in YMM register
%macro HMAX_I32_YMM 2
    vextracti128 xmm%2, ymm%1, 1
    vpmaxsd xmm%1, xmm%1, xmm%2
    HMAX_I32_XMM %1, %2
%endmacro

; Horizontal product of 4x f32 in XMM register
%macro HPROD_F32_XMM 2
    vshufps xmm%2, xmm%1, xmm%1, 0x4E
    vmulps xmm%1, xmm%1, xmm%2
    vshufps xmm%2, xmm%1, xmm%1, 0xB1
    vmulps xmm%1, xmm%1, xmm%2
%endmacro

; Horizontal product of 8x f32 in YMM register
%macro HPROD_F32_YMM 2
    vextractf128 xmm%2, ymm%1, 1
    vmulps xmm%1, xmm%1, xmm%2
    HPROD_F32_XMM %1, %2
%endmacro

; Horizontal min of 4x f32 in XMM register
%macro HMIN_F32_XMM 2
    vshufps xmm%2, xmm%1, xmm%1, 0x4E
    vminps xmm%1, xmm%1, xmm%2
    vshufps xmm%2, xmm%1, xmm%1, 0xB1
    vminps xmm%1, xmm%1, xmm%2
%endmacro

; Horizontal min of 8x f32 in YMM register
%macro HMIN_F32_YMM 2
    vextractf128 xmm%2, ymm%1, 1
    vminps xmm%1, xmm%1, xmm%2
    HMIN_F32_XMM %1, %2
%endmacro

; Horizontal max of 4x f32 in XMM register
%macro HMAX_F32_XMM 2
    vshufps xmm%2, xmm%1, xmm%1, 0x4E
    vmaxps xmm%1, xmm%1, xmm%2
    vshufps xmm%2, xmm%1, xmm%1, 0xB1
    vmaxps xmm%1, xmm%1, xmm%2
%endmacro

; Horizontal max of 8x f32 in YMM register
%macro HMAX_F32_YMM 2
    vextractf128 xmm%2, ymm%1, 1
    vmaxps xmm%1, xmm%1, xmm%2
    HMAX_F32_XMM %1, %2
%endmacro

; Horizontal sum of 4x u32 in XMM register
%macro HSUM_U32_XMM 2
    vpshufd xmm%2, xmm%1, 0x4E
    vpaddd xmm%1, xmm%1, xmm%2
    vpshufd xmm%2, xmm%1, 0xB1
    vpaddd xmm%1, xmm%1, xmm%2
%endmacro

; Horizontal sum of 8x u32 in YMM register
%macro HSUM_U32_YMM 2
    vextracti128 xmm%2, ymm%1, 1
    vpaddd xmm%1, xmm%1, xmm%2
    HSUM_U32_XMM %1, %2
%endmacro

; Horizontal product of 4x u32 in XMM register
%macro HPROD_U32_XMM 2
    vpshufd xmm%2, xmm%1, 0x4E
    vpmulld xmm%1, xmm%1, xmm%2
    vpshufd xmm%2, xmm%1, 0xB1
    vpmulld xmm%1, xmm%1, xmm%2
%endmacro

; Horizontal product of 8x u32 in YMM register
%macro HPROD_U32_YMM 2
    vextracti128 xmm%2, ymm%1, 1
    vpmulld xmm%1, xmm%1, xmm%2
    HPROD_U32_XMM %1, %2
%endmacro

; Horizontal min of 4x u32 in XMM register
%macro HMIN_U32_XMM 2
    vpshufd xmm%2, xmm%1, 0x4E
    vpminud xmm%1, xmm%1, xmm%2
    vpshufd xmm%2, xmm%1, 0xB1
    vpminud xmm%1, xmm%1, xmm%2
%endmacro

; Horizontal min of 8x u32 in YMM register
%macro HMIN_U32_YMM 2
    vextracti128 xmm%2, ymm%1, 1
    vpminud xmm%1, xmm%1, xmm%2
    HMIN_U32_XMM %1, %2
%endmacro

; Horizontal max of 4x u32 in XMM register
%macro HMAX_U32_XMM 2
    vpshufd xmm%2, xmm%1, 0x4E
    vpmaxud xmm%1, xmm%1, xmm%2
    vpshufd xmm%2, xmm%1, 0xB1
    vpmaxud xmm%1, xmm%1, xmm%2
%endmacro

; Horizontal max of 8x u32 in YMM register
%macro HMAX_U32_YMM 2
    vextracti128 xmm%2, ymm%1, 1
    vpmaxud xmm%1, xmm%1, xmm%2
    HMAX_U32_XMM %1, %2
%endmacro

; Horizontal sum of 8x i16 in XMM register
%macro HSUM_I16_XMM 2
    vphaddw xmm%1, xmm%1, xmm%1
    vphaddw xmm%1, xmm%1, xmm%1
    vphaddw xmm%1, xmm%1, xmm%1
%endmacro

; Horizontal sum of 16x i16 in YMM register
%macro HSUM_I16_YMM 2
    vextracti128 xmm%2, ymm%1, 1
    vpaddw xmm%1, xmm%1, xmm%2
    HSUM_I16_XMM %1, %2
%endmacro

; Horizontal product of 8x i16 in XMM register
%macro HPROD_I16_XMM 2
    vpshufd xmm%2, xmm%1, 0x4E
    vpmullw xmm%1, xmm%1, xmm%2
    vpshufd xmm%2, xmm%1, 0xB1
    vpmullw xmm%1, xmm%1, xmm%2
    vpsrldq xmm%2, xmm%1, 2
    vpmullw xmm%1, xmm%1, xmm%2
%endmacro

; Horizontal product of 16x i16 in YMM register
%macro HPROD_I16_YMM 2
    vextracti128 xmm%2, ymm%1, 1
    vpmullw xmm%1, xmm%1, xmm%2
    HPROD_I16_XMM %1, %2
%endmacro

; Horizontal min of 8x i16 in XMM register
%macro HMIN_I16_XMM 2
    vpshufd xmm%2, xmm%1, 0x4E
    vpminsw xmm%1, xmm%1, xmm%2
    vpshufd xmm%2, xmm%1, 0xB1
    vpminsw xmm%1, xmm%1, xmm%2
    vpsrldq xmm%2, xmm%1, 2
    vpminsw xmm%1, xmm%1, xmm%2
%endmacro

; Horizontal min of 16x i16 in YMM register
%macro HMIN_I16_YMM 2
    vextracti128 xmm%2, ymm%1, 1
    vpminsw xmm%1, xmm%1, xmm%2
    HMIN_I16_XMM %1, %2
%endmacro

; Horizontal max of 8x i16 in XMM register
%macro HMAX_I16_XMM 2
    vpshufd xmm%2, xmm%1, 0x4E
    vpmaxsw xmm%1, xmm%1, xmm%2
    vpshufd xmm%2, xmm%1, 0xB1
    vpmaxsw xmm%1, xmm%1, xmm%2
    vpsrldq xmm%2, xmm%1, 2
    vpmaxsw xmm%1, xmm%1, xmm%2
%endmacro

; Horizontal max of 16x i16 in YMM register
%macro HMAX_I16_YMM 2
    vextracti128 xmm%2, ymm%1, 1
    vpmaxsw xmm%1, xmm%1, xmm%2
    HMAX_I16_XMM %1, %2
%endmacro

; Horizontal sum of 8x u16 in XMM register
%macro HSUM_U16_XMM 2
    vphaddw xmm%1, xmm%1, xmm%1
    vphaddw xmm%1, xmm%1, xmm%1
    vphaddw xmm%1, xmm%1, xmm%1
%endmacro

; Horizontal sum of 16x u16 in YMM register
%macro HSUM_U16_YMM 2
    vextracti128 xmm%2, ymm%1, 1
    vpaddw xmm%1, xmm%1, xmm%2
    HSUM_U16_XMM %1, %2
%endmacro

; Horizontal product of 8x u16 in XMM register
%macro HPROD_U16_XMM 2
    vpshufd xmm%2, xmm%1, 0x4E
    vpmullw xmm%1, xmm%1, xmm%2
    vpshufd xmm%2, xmm%1, 0xB1
    vpmullw xmm%1, xmm%1, xmm%2
    vpsrldq xmm%2, xmm%1, 2
    vpmullw xmm%1, xmm%1, xmm%2
%endmacro

; Horizontal product of 16x u16 in YMM register
%macro HPROD_U16_YMM 2
    vextracti128 xmm%2, ymm%1, 1
    vpmullw xmm%1, xmm%1, xmm%2
    HPROD_U16_XMM %1, %2
%endmacro

; Horizontal min of 8x u16 in XMM register
%macro HMIN_U16_XMM 2
    vpshufd xmm%2, xmm%1, 0x4E
    vpminuw xmm%1, xmm%1, xmm%2
    vpshufd xmm%2, xmm%1, 0xB1
    vpminuw xmm%1, xmm%1, xmm%2
    vpsrldq xmm%2, xmm%1, 2
    vpminuw xmm%1, xmm%1, xmm%2
%endmacro

; Horizontal min of 16x u16 in YMM register
%macro HMIN_U16_YMM 2
    vextracti128 xmm%2, ymm%1, 1
    vpminuw xmm%1, xmm%1, xmm%2
    HMIN_U16_XMM %1, %2
%endmacro

; Horizontal max of 8x u16 in XMM register
%macro HMAX_U16_XMM 2
    vpshufd xmm%2, xmm%1, 0x4E
    vpmaxuw xmm%1, xmm%1, xmm%2
    vpshufd xmm%2, xmm%1, 0xB1
    vpmaxuw xmm%1, xmm%1, xmm%2
    vpsrldq xmm%2, xmm%1, 2
    vpmaxuw xmm%1, xmm%1, xmm%2
%endmacro

; Horizontal max of 16x u16 in YMM register
%macro HMAX_U16_YMM 2
    vextracti128 xmm%2, ymm%1, 1
    vpmaxuw xmm%1, xmm%1, xmm%2
    HMAX_U16_XMM %1, %2
%endmacro

; Horizontal sum of 2x u64 in XMM register
%macro HSUM_U64_XMM 2
    vpshufd xmm%2, xmm%1, 0x4E
    vpaddq xmm%1, xmm%1, xmm%2
%endmacro

; Horizontal sum of 4x u64 in YMM register
%macro HSUM_U64_YMM 2
    vextracti128 xmm%2, ymm%1, 1
    vpaddq xmm%1, xmm%1, xmm%2
    HSUM_U64_XMM %1, %2
%endmacro
