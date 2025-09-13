param(
  [string]$Configuration = "Debug",
  [switch]$Rebuild
)

$ErrorActionPreference = 'Stop'

function Ensure-Dir($p) { if (-not (Test-Path $p)) { New-Item -ItemType Directory -Path $p | Out-Null } }

$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$Repo = Resolve-Path (Join-Path $Root "..")
Set-Location $Repo

$artifacts = Join-Path $Repo 'artifacts'
$bin = Join-Path $artifacts 'bin'
$results = Join-Path $artifacts 'test-results'
Ensure-Dir $artifacts; Ensure-Dir $bin; Ensure-Dir $results

Write-Host "[Build] Compiling ModeSwitchRunner with cl..."

# Detect cl
$cl = Get-Command cl -ErrorAction SilentlyContinue
if (-not $cl) {
  Write-Host "[Warn] 'cl' not found in PATH. Please run from 'Developer Command Prompt for VS'." -ForegroundColor Yellow
}

# Build test runner
$exe = Join-Path $bin 'ModeSwitchRunner.exe'
if ($Rebuild -and (Test-Path $exe)) { Remove-Item $exe -Force }

$src = @(
  'tests/ModeSwitchRunner.cpp',
  'Protocol/ReliableChannel.cpp',
  'Protocol/FrameCodec.cpp',
  'Protocol/CRC32.cpp',
  'Transport/LoopbackTransport.cpp'
)

$include = "."
$cmd = @(
  'cl','/nologo','/std:c++17','/EHsc',
  '/I', $include
) + $src + @('/Fe:' + $exe)

& $cmd 2>&1 | Tee-Object -FilePath (Join-Path $results 'build.log')
if (-not (Test-Path $exe)) {
  Write-Error "Build failed. See artifacts/test-results/build.log. Ensure you use a VS Developer Prompt with C++ toolchain installed."
}

Write-Host "[Run] Executing ModeSwitchRunner..."
$outLog = Join-Path $results 'runner_out.txt'
& $exe *>&1 | Tee-Object -FilePath $outLog

$summary = Join-Path $results 'summary.txt'
if (Test-Path $summary) {
  Write-Host "[Summary]" (Get-Content $summary | Select-Object -First 5)
}

if (-not $LASTEXITCODE -eq 0) {
  Write-Error "Tests failed with exit code $LASTEXITCODE. See artifacts/test-results for details."
}

Write-Host "Done. Artifacts in artifacts/test-results and artifacts/bin."

