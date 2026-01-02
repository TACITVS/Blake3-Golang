param(
    [int]$Runs = 10
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$root = Resolve-Path (Join-Path $PSScriptRoot "..\..")

function Add-GoSamples {
    param($Lines, $Store)
    foreach ($line in $Lines) {
        if ($line -match '^BenchmarkSum256_1K-\d+\s+\d+\s+\d+\s+ns/op\s+([0-9.]+)\s+MB/s') { $Store['1K'] += [double]$Matches[1]; continue }
        if ($line -match '^BenchmarkSum256_8K-\d+\s+\d+\s+\d+\s+ns/op\s+([0-9.]+)\s+MB/s') { $Store['8K'] += [double]$Matches[1]; continue }
        if ($line -match '^BenchmarkSum256_1M-\d+\s+\d+\s+\d+\s+ns/op\s+([0-9.]+)\s+MB/s') { $Store['1M'] += [double]$Matches[1]; continue }
    }
}

function Add-RefSamples {
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

Push-Location $root
try {
    $go = @{ '1K' = @(); '8K' = @(); '1M' = @() }
    $ref = @{ '1K' = @(); '8K' = @(); '1M' = @() }

    for ($i = 1; $i -le $Runs; $i++) {
        $goOut = & go test ./blake3 -run=^$ -bench=Benchmark -benchmem
        Add-GoSamples $goOut $go

        if ($i -eq 1) {
            $refOut = & powershell -File tools\ref_bench\run.ps1
        } else {
            $refOut = & tools\ref_bench\ref_bench.exe
        }
        Add-RefSamples $refOut $ref
    }

    $goAvg = @{ '1K' = (Avg $go['1K']); '8K' = (Avg $go['8K']); '1M' = (Avg $go['1M']) }
    $refAvg = @{ '1K' = (Avg $ref['1K']); '8K' = (Avg $ref['8K']); '1M' = (Avg $ref['1M']) }

    $goMean = ($goAvg['1K'] + $goAvg['8K'] + $goAvg['1M']) / 3
    $refMean = ($refAvg['1K'] + $refAvg['8K'] + $refAvg['1M']) / 3
    $meanPct = (($goMean / $refMean) - 1) * 100

    $den = 1 + 8 + 1024
    $goWeighted = ($goAvg['1K'] + 8 * $goAvg['8K'] + 1024 * $goAvg['1M']) / $den
    $refWeighted = ($refAvg['1K'] + 8 * $refAvg['8K'] + 1024 * $refAvg['1M']) / $den
    $weightedPct = (($goWeighted / $refWeighted) - 1) * 100

    "GO_AVG 1K=$($goAvg['1K']) 8K=$($goAvg['8K']) 1M=$($goAvg['1M'])"
    "REF_AVG 1K=$($refAvg['1K']) 8K=$($refAvg['8K']) 1M=$($refAvg['1M'])"
    "MEAN_AVG GO=$goMean REF=$refMean IMPROVEMENT=$meanPct`%"
    "WEIGHTED_AVG GO=$goWeighted REF=$refWeighted IMPROVEMENT=$weightedPct`%"
    "GO_SAMPLES 1K=$($go['1K'].Count) 8K=$($go['8K'].Count) 1M=$($go['1M'].Count)"
    "REF_SAMPLES 1K=$($ref['1K'].Count) 8K=$($ref['8K'].Count) 1M=$($ref['1M'].Count)"
} finally {
    Pop-Location
}
