Set-StrictMode -Version Latest

$gcc = "C:\msys64\mingw64\bin\gcc.exe"
$nasm = "C:\Users\baian\AppData\Local\bin\NASM\nasm.exe"
$out = Join-Path $PSScriptRoot "fp_bench.exe"
$obj = Join-Path $PSScriptRoot "fp_blake3_compress.obj"
$asm = Join-Path $PSScriptRoot "asm\fp_blake3_compress.asm"
$src = @(
    (Join-Path $PSScriptRoot "fp_bench.c"),
    (Join-Path $PSScriptRoot "fp_blake3_fast.c")
)

& $nasm -f win64 -O2 -o $obj $asm
if ($LASTEXITCODE -ne 0) {
    throw "NASM build failed"
}

& $gcc -O3 -mavx2 -foptimize-sibling-calls -I $PSScriptRoot -o $out @src $obj
if ($LASTEXITCODE -ne 0) {
    throw "GCC build failed"
}

& $out
