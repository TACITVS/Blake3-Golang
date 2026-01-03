param(
    [int]$Runs = 10
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$root = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$refExe = Join-Path $PSScriptRoot '..\ref_bench\ref_bench.exe'
$fpExe = Join-Path $PSScriptRoot '..\fp_bench\fp_bench.exe'

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

function Avg {
    param($Arr)
    if ($Arr.Count -eq 0) { return 0 }
    return ($Arr | Measure-Object -Average).Average
}

function Stats {
    param($Arr)
    if ($Arr.Count -eq 0) {
        return [pscustomobject]@{ Avg = 0; Min = 0; Max = 0; Std = 0 }
    }
    $avg = (Avg $Arr)
    $minmax = $Arr | Measure-Object -Minimum -Maximum
    $sum = 0.0
    foreach ($v in $Arr) {
        $d = $v - $avg
        $sum += $d * $d
    }
    $std = [math]::Sqrt($sum / $Arr.Count)
    return [pscustomobject]@{ Avg = $avg; Min = $minmax.Minimum; Max = $minmax.Maximum; Std = $std }
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

    $refStats = @{ '1K' = (Stats $ref['1K']); '8K' = (Stats $ref['8K']); '1M' = (Stats $ref['1M']) }
    $goAsmStats = @{ '1K' = (Stats $goAsm['1K']); '8K' = (Stats $goAsm['8K']); '1M' = (Stats $goAsm['1M']) }
    $goPureStats = @{ '1K' = (Stats $goPure['1K']); '8K' = (Stats $goPure['8K']); '1M' = (Stats $goPure['1M']) }
    $fpStats = @{ '1K' = (Stats $fp['1K']); '8K' = (Stats $fp['8K']); '1M' = (Stats $fp['1M']) }

    $cpu = Get-CimInstance Win32_Processor | Select-Object -First 1
    $os = Get-CimInstance Win32_OperatingSystem | Select-Object -First 1
    $memGB = [math]::Round($os.TotalVisibleMemorySize / 1024 / 1024, 2)
    $goVersion = (& go version) -join ''
    $goEnv = & go env GOOS GOARCH
    $gomax = & go env GOMAXPROCS
    if (-not $gomax) {
        $gomax = 'default'
    }
    $gcc = 'C:\msys64\mingw64\bin\gcc.exe'
    $nasm = 'C:\Users\baian\AppData\Local\bin\NASM\nasm.exe'

    Write-Output 'MACHINE'
    Write-Output ('OS={0}' -f $os.Caption)
    Write-Output ('CPU={0}' -f $cpu.Name)
    Write-Output ('CORES={0}' -f $cpu.NumberOfLogicalProcessors)
    Write-Output ('RAM_GB={0}' -f $memGB)
    Write-Output ('GO={0}' -f $goVersion)
    Write-Output ('GOOS={0} GOARCH={1}' -f $goEnv[0], $goEnv[1])
    Write-Output ('GOMAXPROCS={0}' -f $gomax)
    Write-Output ('GCC={0}' -f $gcc)
    Write-Output ('NASM={0}' -f $nasm)

    Write-Output 'RESULTS'
    Write-Output ('ref 1K={0} 8K={1} 1M={2}' -f $refStats['1K'].Avg, $refStats['8K'].Avg, $refStats['1M'].Avg)
    Write-Output ('go_asm 1K={0} 8K={1} 1M={2}' -f $goAsmStats['1K'].Avg, $goAsmStats['8K'].Avg, $goAsmStats['1M'].Avg)
    Write-Output ('go_pure 1K={0} 8K={1} 1M={2}' -f $goPureStats['1K'].Avg, $goPureStats['8K'].Avg, $goPureStats['1M'].Avg)
    Write-Output ('fp_c 1K={0} 8K={1} 1M={2}' -f $fpStats['1K'].Avg, $fpStats['8K'].Avg, $fpStats['1M'].Avg)

    Write-Output 'MARKDOWN_TABLE'
    Write-Output '| Version | Size | Avg MB/s | Min | Max | Std | Rel vs Ref |'
    Write-Output '| --- | --- | --- | --- | --- | --- | --- |'

    $versions = @(
        @{ Name = 'Ref C'; Stats = $refStats; Rel = $null },
        @{ Name = 'Go asm'; Stats = $goAsmStats; Rel = $refStats },
        @{ Name = 'Go purego'; Stats = $goPureStats; Rel = $refStats },
        @{ Name = 'FP C'; Stats = $fpStats; Rel = $refStats }
    )
    $sizes = @('1K', '8K', '1M')
    foreach ($v in $versions) {
        foreach ($s in $sizes) {
            $st = $v.Stats[$s]
            $rel = '-'
            if ($v.Rel -ne $null) {
                $refAvg = $v.Rel[$s].Avg
                if ($refAvg -ne 0) {
                    $rel = [math]::Round((($st.Avg / $refAvg) - 1) * 100, 2)
                }
            }
            Write-Output ('| {0} | {1} | {2} | {3} | {4} | {5} | {6}% |' -f $v.Name, $s, [math]::Round($st.Avg, 2), [math]::Round($st.Min, 2), [math]::Round($st.Max, 2), [math]::Round($st.Std, 2), $rel)
        }
    }

    Write-Output 'MERMAID'
    Write-Output '```mermaid'
    Write-Output 'xychart-beta'
    Write-Output '  title "BLAKE3 Throughput (MB/s)"'
    Write-Output '  x-axis ["1K", "8K", "1M"]'
    $vals = @(
        $refStats['1K'].Avg, $refStats['8K'].Avg, $refStats['1M'].Avg,
        $goAsmStats['1K'].Avg, $goAsmStats['8K'].Avg, $goAsmStats['1M'].Avg,
        $goPureStats['1K'].Avg, $goPureStats['8K'].Avg, $goPureStats['1M'].Avg,
        $fpStats['1K'].Avg, $fpStats['8K'].Avg, $fpStats['1M'].Avg
    )
    $max = [math]::Ceiling((($vals | Measure-Object -Maximum).Maximum) / 100) * 100
    Write-Output ('  y-axis "MB/s" 0 --> {0}' -f $max)
    Write-Output ('  bar "Ref C" [{0}, {1}, {2}]' -f [math]::Round($refStats['1K'].Avg, 2), [math]::Round($refStats['8K'].Avg, 2), [math]::Round($refStats['1M'].Avg, 2))
    Write-Output ('  bar "Go asm" [{0}, {1}, {2}]' -f [math]::Round($goAsmStats['1K'].Avg, 2), [math]::Round($goAsmStats['8K'].Avg, 2), [math]::Round($goAsmStats['1M'].Avg, 2))
    Write-Output ('  bar "Go purego" [{0}, {1}, {2}]' -f [math]::Round($goPureStats['1K'].Avg, 2), [math]::Round($goPureStats['8K'].Avg, 2), [math]::Round($goPureStats['1M'].Avg, 2))
    Write-Output ('  bar "FP C" [{0}, {1}, {2}]' -f [math]::Round($fpStats['1K'].Avg, 2), [math]::Round($fpStats['8K'].Avg, 2), [math]::Round($fpStats['1M'].Avg, 2))
    Write-Output '```'
} finally {
    Pop-Location
}
