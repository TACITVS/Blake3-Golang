Set-StrictMode -Version Latest

$gcc = "C:\msys64\mingw64\bin\gcc.exe"
$nasm = "C:\Users\baian\AppData\Local\bin\NASM\nasm.exe"
$out = Join-Path $PSScriptRoot "fp_bench.exe"
$obj = Join-Path $PSScriptRoot "fp_blake3_compress.obj"
$asmDir = Join-Path $PSScriptRoot "asm"
$asm = Join-Path $asmDir "fp_blake3_compress.asm"
$src = @(
    (Join-Path $PSScriptRoot "fp_bench.c"),
    (Join-Path $PSScriptRoot "fp_blake3_fast.c")
)

& $nasm -f win64 -O2 -I $asmDir -o $obj $asm
if ($LASTEXITCODE -ne 0) {
    throw "NASM build failed"
}

& $gcc -O3 -mavx2 -foptimize-sibling-calls -I $PSScriptRoot -o $out @src $obj
if ($LASTEXITCODE -ne 0) {
    throw "GCC build failed"
}

& $out
