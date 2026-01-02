Set-StrictMode -Version Latest

$gcc = "C:\msys64\mingw64\bin\gcc.exe"
$root = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$ref = Resolve-Path (Join-Path $root "..\_ref\BLAKE3\c")
$out = Join-Path $PSScriptRoot "ref_bench.exe"

$src = @(
    (Join-Path $PSScriptRoot "ref_bench.c"),
    (Join-Path $ref "blake3.c"),
    (Join-Path $ref "blake3_dispatch.c"),
    (Join-Path $ref "blake3_portable.c"),
    (Join-Path $ref "blake3_sse2_x86-64_windows_gnu.S"),
    (Join-Path $ref "blake3_sse41_x86-64_windows_gnu.S"),
    (Join-Path $ref "blake3_avx2_x86-64_windows_gnu.S")
)

$avx512 = Join-Path $ref "blake3_avx512_x86-64_windows_gnu.S"
if (Test-Path $avx512) {
    $src += $avx512
}

& $gcc -O3 -I $ref -o $out @src
if ($LASTEXITCODE -ne 0 -and $src -contains $avx512) {
    Write-Host "AVX-512 assembly failed to build, retrying without it..."
    $src = $src | Where-Object { $_ -ne $avx512 }
    & $gcc -O3 -I $ref -o $out @src
}

if ($LASTEXITCODE -ne 0) {
    throw "GCC build failed"
}

& $out
