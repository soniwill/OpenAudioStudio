[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Config = "Release",
    [string]$TargetPath,
    [switch]$Rebuild
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path "$PSScriptRoot\..").Path
$buildScript = Join-Path $repoRoot "scripts\build.ps1"
$sourceDll = Join-Path $repoRoot "build\$Config\$Config\oae_audio.dll"
$sourcePdb = Join-Path $repoRoot "build\$Config\$Config\oae_audio.pdb"

if (-not $TargetPath) {
    $TargetPath = Join-Path $repoRoot "unity\com.openaudio.core\Plugins\x86_64"
}

if ($Rebuild -or -not (Test-Path $sourceDll)) {
    Write-Host "[DeployUnity] Building native engine in $Config configuration..." -ForegroundColor Cyan
    & $buildScript -Config $Config
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Native build failed with exit code $LASTEXITCODE."
        exit $LASTEXITCODE
    }
}

if (-not (Test-Path $sourceDll)) {
    Write-Error "Source DLL not found at: $sourceDll"
    exit 1
}

if (-not (Test-Path $TargetPath)) {
    Write-Host "[DeployUnity] Creating target directory: $TargetPath" -ForegroundColor Cyan
    New-Item -ItemType Directory -Path $TargetPath -Force | Out-Null
}

$destDll = Join-Path $TargetPath "oae_audio.dll"
Write-Host "[DeployUnity] Copying $sourceDll -> $destDll" -ForegroundColor Green
Copy-Item -Path $sourceDll -Destination $destDll -Force

if (Test-Path $sourcePdb) {
    $destPdb = Join-Path $TargetPath "oae_audio.pdb"
    Write-Host "[DeployUnity] Copying $sourcePdb -> $destPdb" -ForegroundColor Green
    Copy-Item -Path $sourcePdb -Destination $destPdb -Force
}

# Ensure plugin .meta file exists and is configured for Windows x86_64
$metaPath = "$destDll.meta"
if (-not (Test-Path $metaPath)) {
    Write-Host "[DeployUnity] Generating Unity plugin meta file: $metaPath" -ForegroundColor Cyan
    $metaContent = @"
fileFormatVersion: 2
guid: 8e5f7c3a9d1b4a2e8f1c0d2e3b4a5f6e
PluginImporter:
  externalObjects: {}
  serializedVersion: 2
  iconMap: {}
  executionOrder: {}
  isPreloaded: 1
  isOverridable: 0
  isExplicitlyReferenced: 0
  validateReferences: 1
  platformData:
  - first:
      Any: 
    second:
      enabled: 0
      settings: {}
  - first:
      Editor: Editor
    second:
      enabled: 1
      settings:
        CPU: x86_64
        OS: Windows
  - first:
      Standalone: Win64
    second:
      enabled: 1
      settings:
        CPU: x86_64
  userData: 
  assetBundleName: 
  assetBundleVariant: 
"@
    [System.IO.File]::WriteAllText($metaPath, $metaContent)
}

Write-Host "[DeployUnity] Deployment completed successfully!" -ForegroundColor Green
exit 0
