param(
    [int]$Runs = 10
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$root = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$refExe = Join-Path $PSScriptRoot '..\ref_bench\ref_bench.exe'
$fpExe = Join-Path $PSScriptRoot '..\fp_bench\fp_bench.exe'
$outDir = Join-Path $root 'docs\data'

function Add-GoSamples {
    param($Lines, $Store)
    foreach ($line in $Lines) {
        if ($line -match '^BenchmarkSum256_1K-\d+\s+\d+\s+\d+\s+ns/op\s+([0-9.]+)\s+MB/s') { $Store['1K'] += [double]$Matches[1]; continue }
        if ($line -match '^BenchmarkSum256_8K-\d+\s+\d+\s+\d+\s+ns/op\s+([0-9.]+)\s+MB/s') { $Store['8K'] += [double]$Matches[1]; continue }
        if ($line -match '^BenchmarkSum256_1M-\d+\s+\d+\s+\d+\s+ns/op\s+([0-9.]+)\s+MB/s') { $Store['1M'] += [double]$Matches[1]; continue }
    }
}

function Add-C-Samples {
    param($Lines, $Store)
    foreach ($line in $Lines) {
        if ($line -match 'size=1024\s+bytes.*MB/s=([0-9.]+)') { $Store['1K'] += [double]$Matches[1]; continue }
        if ($line -match 'size=8192\s+bytes.*MB/s=([0-9.]+)') { $Store['8K'] += [double]$Matches[1]; continue }
        if ($line -match 'size=1048576\s+bytes.*MB/s=([0-9.]+)') { $Store['1M'] += [double]$Matches[1]; continue }
    }
}

function To-List {
    param($Store)
    return [ordered]@{
        '1K' = @($Store['1K'])
        '8K' = @($Store['8K'])
        '1M' = @($Store['1M'])
    }
}

Push-Location $root
try {
    $goAsm = @{ '1K' = @(); '8K' = @(); '1M' = @() }
    $goPure = @{ '1K' = @(); '8K' = @(); '1M' = @() }
    $ref = @{ '1K' = @(); '8K' = @(); '1M' = @() }
    $fp = @{ '1K' = @(); '8K' = @(); '1M' = @() }

    $refFirst = & powershell -File tools\ref_bench\run.ps1
    Add-C-Samples $refFirst $ref
    $fpFirst = & powershell -File tools\fp_bench\run.ps1
    Add-C-Samples $fpFirst $fp

    for ($i = 1; $i -le $Runs; $i++) {
        $goOut = & go test ./blake3 -run=^$ -bench=Benchmark -benchmem
        Add-GoSamples $goOut $goAsm

        $goPureOut = & go test -tags purego ./blake3 -run=^$ -bench=Benchmark -benchmem
        Add-GoSamples $goPureOut $goPure

        if ($i -gt 1) {
            $refOut = & $refExe
            Add-C-Samples $refOut $ref
            $fpOut = & $fpExe
            Add-C-Samples $fpOut $fp
        }
    }

    $cpu = Get-CimInstance Win32_Processor | Select-Object -First 1
    $os = Get-CimInstance Win32_OperatingSystem | Select-Object -First 1
    $memGB = [math]::Round($os.TotalVisibleMemorySize / 1024 / 1024, 2)
    $goVersion = (& go version) -join ''
    $goEnv = & go env GOOS GOARCH
    $gomax = & go env GOMAXPROCS
    if (-not $gomax) { $gomax = 'default' }
    $gcc = 'C:\msys64\mingw64\bin\gcc.exe'
    $nasm = 'C:\Users\baian\AppData\Local\bin\NASM\nasm.exe'

    $data = [ordered]@{
        generated_at = (Get-Date).ToString('o')
        runs = $Runs
        sizes = @('1K','8K','1M')
        git_commit = (& git rev-parse HEAD).Trim()
        machine = [ordered]@{
            os = $os.Caption
            cpu = $cpu.Name
            cores = $cpu.NumberOfLogicalProcessors
            ram_gb = $memGB
        }
        toolchain = [ordered]@{
            go = $goVersion
            goos = $goEnv[0]
            goarch = $goEnv[1]
            gomaxprocs = $gomax
            gcc = $gcc
            nasm = $nasm
        }
        versions = [ordered]@{
            ref_c = [ordered]@{ label = 'Ref C'; sizes = (To-List $ref) }
            go_asm = [ordered]@{ label = 'Go asm'; sizes = (To-List $goAsm) }
            go_purego = [ordered]@{ label = 'Go purego'; sizes = (To-List $goPure) }
            fp_c = [ordered]@{ label = 'FP C'; sizes = (To-List $fp) }
        }
    }

    New-Item -ItemType Directory -Force -Path $outDir | Out-Null
    $json = $data | ConvertTo-Json -Depth 6
    $json | Set-Content (Join-Path $outDir 'bench.json') -Encoding utf8
    ('window.BENCH_DATA = ' + $json + ';') | Set-Content (Join-Path $outDir 'bench.js') -Encoding utf8

    Write-Output ('Wrote ' + (Join-Path $outDir 'bench.json'))
    Write-Output ('Wrote ' + (Join-Path $outDir 'bench.js'))
} finally {
    Pop-Location
}
