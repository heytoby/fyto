$ErrorActionPreference='Stop'
$framework=Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319'
& "$framework\csc.exe" /nologo /target:winexe /optimize+ /platform:x64 /codepage:65001 /r:System.Net.Http.dll /r:System.Web.Extensions.dll /r:System.Windows.Forms.dll /r:System.Drawing.dll "/out:$PSScriptRoot\..\res\fyto-updater.exe" "$PSScriptRoot\UpdateClient.cs"
if($LASTEXITCODE -ne 0){throw 'Updater build failed'}
