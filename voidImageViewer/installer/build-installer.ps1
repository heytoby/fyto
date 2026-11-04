param([Parameter(Mandatory=$true)][string]$WixDll,[Parameter(Mandatory=$true)][string]$UiExtension,[Parameter(Mandatory=$true)][string]$Payload,[Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$OutputDirectory=(Resolve-Path $OutputDirectory).Path
$env:DOTNET_ROLL_FORWARD='Major'
$msi=Join-Path $OutputDirectory 'fyto.msi'
Push-Location $PSScriptRoot
try {
 dotnet $WixDll build Package.wxs -arch x64 -ext $UiExtension -d "Payload=$Payload" -culture zh-CN -o $msi
 if($LASTEXITCODE -ne 0){throw 'MSI build failed'}
 & .\build-bootstrap.ps1 -Msi $msi -Output (Join-Path $OutputDirectory 'fyto-Setup.exe')
} finally { Pop-Location }
