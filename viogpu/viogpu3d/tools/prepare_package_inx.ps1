param(
  [Parameter(Mandatory = $true)]
  [string]$SourcePath,

  [Parameter(Mandatory = $true)]
  [string]$DestinationPath,

  [string]$NativeLvpPath = '',
  [string]$Wow64LvpPath = ''
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $SourcePath)) {
  throw "Missing package INX template: $SourcePath"
}

$sourceName = [System.IO.Path]::GetFileName($SourcePath)
$nativeTokens = switch -Wildcard ($sourceName) {
  'viogpu3d_x64*'   { @('vulkan_lvp_x64.dll', 'lvp_icd.x86_64.json'); break }
  'viogpu3d_arm64*' { @('vulkan_lvp_arm64.dll', 'lvp_icd.aarch64.json'); break }
  'viogpu3d_x86*'   { @('vulkan_lvp_x86.dll', 'lvp_icd.x86.json'); break }
  default           { throw "Unknown package INX template: $sourceName" }
}

$removeTokens = @()
$nativeLvpPresent = -not [string]::IsNullOrWhiteSpace($NativeLvpPath) -and
  (Test-Path -LiteralPath $NativeLvpPath)
if (-not $nativeLvpPresent) {
  $removeTokens += $nativeTokens
}

$hasWow64Payload = $sourceName -like '*_wow64.inx'
$wow64LvpPresent = $hasWow64Payload -and
  -not [string]::IsNullOrWhiteSpace($Wow64LvpPath) -and
  (Test-Path -LiteralPath $Wow64LvpPath)
if ($hasWow64Payload -and -not $wow64LvpPresent) {
  $removeTokens += @('vulkan_lvp_x86.dll', 'lvp_icd.x86.json')
}

$lines = [System.IO.File]::ReadAllLines($SourcePath)
if ($removeTokens.Count -ne 0) {
  $pattern = ($removeTokens | ForEach-Object { [regex]::Escape($_) }) -join '|'
  $lines = @($lines | Where-Object { $_ -notmatch $pattern })
}

$destinationDir = Split-Path -Parent $DestinationPath
if (-not (Test-Path -LiteralPath $destinationDir)) {
  New-Item -ItemType Directory -Path $destinationDir | Out-Null
}

$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllLines($DestinationPath, $lines, $utf8NoBom)

Write-Host "Prepared $DestinationPath (native Lavapipe=$nativeLvpPresent, wow64 Lavapipe=$wow64LvpPresent)"
