#requires -Version 5.1
<#
Full source audit for PortMaster (ASCII-only PowerShell 5.1)
- Walk all source/script/doc files
- Fully read file content via .NET
- Compute hash/lines/size
- Heuristic spec compliance checks
- Write manifest and machine-readable summary (JSON)

Outputs:
  reports\fullscan_manifest_latest.txt
  reports\fullscan_summary.json
#>

param(
  [string]$Root = (Resolve-Path '.').Path
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function New-Dir($Path){ if(-not (Test-Path $Path)){ New-Item -ItemType Directory -Path $Path | Out-Null } }

function Get-Sha256Hex([string]$Path){
  $sha = [System.Security.Cryptography.SHA256]::Create()
  $fs = [System.IO.File]::OpenRead($Path)
  try {
    $hash = $sha.ComputeHash($fs)
    -join ($hash | ForEach-Object { $_.ToString('x2') })
  } finally { $fs.Dispose(); $sha.Dispose() }
}

function Get-Text([string]$Path){
  try { return [System.IO.File]::ReadAllText($Path, [System.Text.Encoding]::UTF8) } catch {}
  try { return [System.IO.File]::ReadAllText($Path, [System.Text.Encoding]::Unicode) } catch {}
  return [System.IO.File]::ReadAllText($Path)
}

function Count-Lines([string]$Text){ if([string]::IsNullOrEmpty($Text)){return 0}; return ($Text -split "`r?`n").Count }

# Collect files
$includeExt = @('.h','.hpp','.c','.cpp','.cc','.rc','.rc2','.def',
  '.json','.ini','.cfg','.conf','.toml','.yml','.yaml',
  '.bat','.cmd','.ps1','.py','.sh','.txt','.md')

$allFiles = Get-ChildItem -Path $Root -Recurse -File |
  Where-Object { $includeExt -contains $_.Extension.ToLower() } |
  Where-Object { $_.FullName -notmatch '\\(bin|x64|Win32)\\' } |
  Sort-Object FullName

if(-not $allFiles){ Write-Error 'No files to scan'; exit 2 }

# Scan
$items = @()
foreach($f in $allFiles){
  $t = Get-Text $f.FullName
  $lines = Count-Lines $t
  $sha = Get-Sha256Hex $f.FullName
  $items += [pscustomobject]@{
    Path = $f.FullName
    RelPath = (Resolve-Path -LiteralPath $f.FullName -Relative)
    Ext = $f.Extension.ToLower()
    Size = $f.Length
    Lines = $lines
    Sha256 = $sha
    Text = $t
  }
}

# Helpers
function Find-Item([string]$rel){ return $items | Where-Object { $_.RelPath -ieq $rel } }
function Find-ItemBySuffix([string]$suffix){
  $pattern = [Regex]::Escape($suffix).Replace('\\','\\\\') + '$'
  return $items | Where-Object { $_.Path -imatch $pattern }
}
function Has-Text([string]$rel,[string]$pattern){ $i = Find-Item $rel; if(-not $i){return $false}; return ($i.Text -match $pattern) }
function Has-TextSuffix([string]$suffix,[string]$pattern){ $i = Find-ItemBySuffix $suffix; if(-not $i){return $false}; return ($i.Text -match $pattern) }

# Checks (English, ASCII only)
$checkList = @(
  @{ Key='Transport.Serial'; Desc='CreateFile/SetCommState/OVERLAPPED present'; Ok=((Has-TextSuffix 'Transport/SerialTransport.cpp' 'CreateFileA') -and (Has-TextSuffix 'Transport/SerialTransport.cpp' 'SetCommState') -and (Has-TextSuffix 'Transport/SerialTransport.cpp' 'OVERLAPPED')); Evidence='Transport/SerialTransport.cpp' },
  @{ Key='Transport.TCP'; Desc='Client/Server and async I/O'; Ok=((Has-TextSuffix 'Transport/TcpTransport.cpp' 'WSAStartup') -and (Has-TextSuffix 'Transport/TcpTransport.cpp' 'connect\(') -and ((Has-TextSuffix 'Transport/TcpTransport.cpp' 'listen\(') -or (Has-TextSuffix 'Transport/TcpTransport.cpp' 'Accept'))); Evidence='Transport/TcpTransport.cpp' },
  @{ Key='Transport.UDP'; Desc='bind/recvfrom present'; Ok=((Has-TextSuffix 'Transport/UdpTransport.cpp' 'bind\(') -and (Has-TextSuffix 'Transport/UdpTransport.cpp' 'recvfrom\(')); Evidence='Transport/UdpTransport.cpp' },
  @{ Key='Transport.LPT'; Desc='OpenPrinter/StartDocPrinter/WritePrinter present'; Ok=((Has-TextSuffix 'Transport/LptSpoolerTransport.cpp' 'OpenPrinterA') -and (Has-TextSuffix 'Transport/LptSpoolerTransport.cpp' 'StartDocPrinter') -and (Has-TextSuffix 'Transport/LptSpoolerTransport.cpp' 'WritePrinter')); Evidence='Transport/LptSpoolerTransport.cpp' },
  @{ Key='Transport.USB'; Desc='SetupAPI or EnumPrinters present'; Ok=((Has-TextSuffix 'Transport/UsbPrinterTransport.cpp' 'SetupDiGetClassDevs') -or (Has-TextSuffix 'Transport/UsbPrinterTransport.cpp' 'EnumPrinters')); Evidence='Transport/UsbPrinterTransport.cpp' },
  @{ Key='Transport.Loopback'; Desc='Delay/loss simulation fields present'; Ok=((Has-TextSuffix 'Transport/LoopbackTransport.cpp' 'deliveryTime') -and (Has-TextSuffix 'Transport/LoopbackTransport.cpp' 'm_errorRate')); Evidence='Transport/LoopbackTransport.cpp' },
  @{ Key='Protocol.FrameCodec'; Desc='Encode/Decode and CRC present'; Ok=((Has-TextSuffix 'Protocol/FrameCodec.h' 'FRAME_HEADER') -and (Has-TextSuffix 'Protocol/FrameCodec.cpp' 'EncodeFrame') -and (Has-TextSuffix 'Protocol/FrameCodec.cpp' 'DecodeFrame') -and (Has-TextSuffix 'Protocol/CRC32.cpp' 'InitializeTable')); Evidence='Protocol/FrameCodec.* / CRC32.*' },
  @{ Key='Protocol.ReliableChannel'; Desc='Sliding window/ACK/NAK/timeout present'; Ok=((Has-TextSuffix 'Protocol/ReliableChannel.cpp' 'm_windowSize') -and (Has-TextSuffix 'Protocol/ReliableChannel.cpp' 'SendFrameInWindow') -and (Has-TextSuffix 'Protocol/ReliableChannel.cpp' 'HandleWindowAck') -and (Has-TextSuffix 'Protocol/ReliableChannel.cpp' 'HandleWindowNak') -and (Has-TextSuffix 'Protocol/ReliableChannel.cpp' 'CheckWindowTimeouts')); Evidence='Protocol/ReliableChannel.*' },
  @{ Key='Common.IOWorker'; Desc='WSASend/WSARecv present'; Ok=((Has-TextSuffix 'Common/IOWorker.cpp' 'WSARecv') -and (Has-TextSuffix 'Common/IOWorker.cpp' 'WSASend')); Evidence='Common/IOWorker.*' },
  @{ Key='Common.ConfigManager'; Desc='JSON read/write and user-dir fallback'; Ok=((Has-TextSuffix 'Common/ConfigManager.cpp' 'ParseJson') -and (Has-TextSuffix 'Common/ConfigManager.cpp' 'WriteJsonFile') -and (Has-TextSuffix 'Common/ConfigManager.cpp' 'SHGetFolderPathA')); Evidence='Common/ConfigManager.*' },
  @{ Key='UI.Resource'; Desc='App icon and splash packed'; Ok=((Has-TextSuffix 'PortMaster.rc' 'IDI_MAIN_ICON') -and (Has-TextSuffix 'PortMaster.rc' 'IDB_SPLASH')); Evidence='PortMaster.rc' }
)

$checks = foreach($c in $checkList){
  $r = if($c.Ok){ 'pass' } else { 'partial/missing' }
  [pscustomobject]@{ Key=$c.Key; Desc=$c.Desc; Result=$r; Evidence=$c.Evidence }
}

# Unimplemented advanced interfaces (ProtocolManager)
$unimpl = @()
$pmHeader = Find-ItemBySuffix 'Protocol/ProtocolManager.h'
$pmBody   = Find-ItemBySuffix 'Protocol/ProtocolManager.cpp'
if($pmHeader -and $pmBody){
  $decls = @('EnableEncryption','DisableEncryption','EnableCompression','DisableCompression','EnableMultiplexing','SetPriorityLevel','EnableFlowControl','RecoverSession','SetBufferSize','SetConcurrencyLevel','EnableBatching')
  foreach($d in $decls){ if($pmHeader.Text -match "\b$d\b" -and -not ($pmBody.Text -match "\b$d\b")){ $unimpl += $d } }
}

# Splash message-driven close capability
$splash = Find-ItemBySuffix 'SplashDialog.cpp'
$splashMsgDriven = $false
if($splash){ $splashMsgDriven = ($splash.Text -match 'WM_SPLASH_INIT_COMPLETE' -or $splash.Text -match 'OnInitializationComplete') }

# DeviceManager placeholders
$dm = Find-ItemBySuffix 'Common/DeviceManager.cpp'
$dmPartial = $false
if($dm){ $dmPartial = ($dm.Text -match 'TODO' -or $dm.Text -match 'SaveDeviceConfig\(.*\)\s*\{\s*//|return\s+true;') }

# Write outputs
$reportsDir = Join-Path $Root 'reports'
New-Dir $reportsDir

$manifestPath = Join-Path $reportsDir 'fullscan_manifest_latest.txt'
($items | ForEach-Object { "{0}`t{1}`t{2}" -f $_.Path,$_.Lines,$_.Sha256 }) | Set-Content -Encoding UTF8 -Path $manifestPath

$summary = [pscustomobject]@{
  scanned_files = $items.Count
  total_lines = ($items | Measure-Object -Property Lines -Sum).Sum
  checks = $checks
  unimplemented = $unimpl
  splash_msg_driven = $splashMsgDriven
  devicemanager_partial = $dmPartial
}

$jsonPath = Join-Path $reportsDir 'fullscan_summary.json'
$summary | ConvertTo-Json -Depth 6 | Set-Content -Encoding UTF8 -Path $jsonPath

Write-Host "[OK] Full scan completed: $($items.Count) files. Checks=$($checks.Count) Unimpl=$($unimpl.Count)"
Write-Host "[OK] Manifest: $manifestPath"
Write-Host "[OK] Summary:  $jsonPath"
