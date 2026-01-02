Set-StrictMode -Version Latest

$gcc = "C:\msys64\mingw64\bin\gcc.exe"
$out = Join-Path $PSScriptRoot "fp_bench.exe"
$src = @(
    (Join-Path $PSScriptRoot "fp_bench.c"),
    (Join-Path $PSScriptRoot "fp_blake3_fast.c")
)

& $gcc -O3 -mavx2 -foptimize-sibling-calls -I $PSScriptRoot -o $out @src
if ($LASTEXITCODE -ne 0) {
    throw "GCC build failed"
}

& $out
