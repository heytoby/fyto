param([string]$WixDll,[string]$UiExtension,[string]$Toolset='v143')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$versionHeader=[IO.File]::ReadAllText((Join-Path $root 'voidImageViewer/src/version.h'))
$parts=@('MAJOR','MINOR','REVISION','BUILD') | ForEach-Object {[int][regex]::Match($versionHeader,('VERSION_'+$_+'\s+(\d+)')).Groups[1].Value}
$version=$parts -join '.'
if($parts[2] -ne 0 -or $parts[3] -gt 65535){throw 'Unsupported MSI version mapping'}
if($env:GITHUB_REF_TYPE -eq 'tag' -and $env:GITHUB_REF_NAME -ne "v$version"){throw 'Git tag and source version differ'}
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$msbuild=& $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
if(!$msbuild){throw 'Visual Studio C++ Build Tools required'}
$sdkRoot=Join-Path ${env:ProgramFiles(x86)} 'Windows Kits/10'
$sdk=(Get-ChildItem (Join-Path $sdkRoot 'Lib') -Directory | Where-Object { $_.Name -match '^\d+\.\d+\.\d+\.\d+$' } | Sort-Object { [version]$_.Name } -Descending | Select-Object -First 1).Name
if(!$sdk){throw 'A versioned Windows SDK is required'}
$fontPath=Join-Path $root 'voidImageViewer/res/SourceHanSansCN-Regular.otf'
$fontBlob='5e6605995d5cbc432c4273f566a5f2b8c376c682'
if (!(Test-Path -LiteralPath $fontPath) -or (git hash-object --no-filters -- $fontPath) -ne $fontBlob) {
 $fontUrl='https://raw.githubusercontent.com/adobe-fonts/source-han-sans/6c709ca72d3d7c46ab42ebecc1a26e7d69595a37/SubsetOTF/CN/SourceHanSansCN-Regular.otf'
 $download="$fontPath.download"
 try {
  Invoke-WebRequest -UseBasicParsing -Uri $fontUrl -OutFile $download
  if ((git hash-object --no-filters -- $download) -ne $fontBlob) {throw 'Source Han Sans font checksum mismatch'}
  Move-Item -LiteralPath $download -Destination $fontPath -Force
 } finally {
  if (Test-Path -LiteralPath $download) {Remove-Item -LiteralPath $download}
 }
}
& python (Join-Path $root 'tests/test_playback_contract.py')
if($LASTEXITCODE -ne 0){throw 'Playback contract check failed'}
& python (Join-Path $root 'tests/prepare_presentation_test.py')
if($LASTEXITCODE -ne 0){throw 'Presentation test preparation failed'}
& $msbuild (Join-Path $root 'tests/presentation.vcxproj') /nologo /v:minimal /p:Configuration=Release /p:Platform=x64 "/p:PlatformToolset=$Toolset" "/p:WindowsTargetPlatformVersion=$sdk"
if($LASTEXITCODE -ne 0){throw 'Presentation test build failed'}
& (Join-Path $root 'build/presentation-tests/test_presentation.exe') $fontPath
if($LASTEXITCODE -ne 0){throw 'Presentation regression check failed'}
& $msbuild (Join-Path $root 'tests/primary_display.vcxproj') /nologo /v:minimal /p:Configuration=Release /p:Platform=x64 "/p:PlatformToolset=$Toolset" "/p:WindowsTargetPlatformVersion=$sdk"
if($LASTEXITCODE -ne 0){throw 'Primary display test build failed'}
& (Join-Path $root 'build/primary-display-tests/test_primary_display.exe')
if($LASTEXITCODE -ne 0){throw 'Primary display regression check failed'}
& $msbuild (Join-Path $root 'voidImageViewer/vs2019/voidImageViewer.vcxproj') /nologo /v:minimal /p:Configuration=Release /p:Platform=x64 /p:TargetName=fyto "/p:PlatformToolset=$Toolset" "/p:WindowsTargetPlatformVersion=$sdk" "/p:ForceImportBeforeCppTargets=$root/build-overrides.targets"
if($LASTEXITCODE -ne 0){throw 'Viewer build failed'}
$build=Join-Path $root 'build';$payload=Join-Path $build 'payload';$out=Join-Path $root 'release-assets'
New-Item -ItemType Directory -Force $payload,$out | Out-Null
Copy-Item (Join-Path $root 'voidImageViewer/vs2019/x64/Release/fyto.exe') $payload
Copy-Item (Join-Path $root 'LICENSE') $payload
Copy-Item (Join-Path $root 'voidImageViewer/libwebp/COPYING') (Join-Path $payload 'libwebp-COPYING')
Copy-Item (Join-Path $root 'voidImageViewer/libwebp/PATENTS') (Join-Path $payload 'libwebp-PATENTS')
Copy-Item (Join-Path $root 'THIRD_PARTY_NOTICES.md') (Join-Path $payload 'NOTICE.txt')
Get-Content -Raw (Join-Path $root 'voidImageViewer/res/SourceHanSans-OFL.txt') | Add-Content -Encoding utf8 (Join-Path $payload 'NOTICE.txt')
Copy-Item (Join-Path $root 'README.md') (Join-Path $payload 'README.txt')
if($WixDll -and $UiExtension){
 & (Join-Path $root 'voidImageViewer/installer/build-installer.ps1') -WixDll $WixDll -UiExtension $UiExtension -Payload $payload -OutputDirectory $build
}else{
 $tools=Join-Path $root '.tools'
 if(!(Test-Path (Join-Path $tools 'wix.exe'))){dotnet tool install wix --version 6.0.2 --tool-path $tools;if($LASTEXITCODE -ne 0){throw 'WiX install failed'}}
 $wix=Join-Path $tools 'wix.exe';$env:DOTNET_ROLL_FORWARD='Major'
 Push-Location (Join-Path $root 'voidImageViewer/installer')
 try{
  & $wix extension add WixToolset.UI.wixext/6.0.2
  if($LASTEXITCODE -ne 0){throw 'WiX extension restore failed'}
  & $wix build Package.wxs -arch x64 -ext WixToolset.UI.wixext -d "Payload=$payload" -culture zh-CN -o (Join-Path $build 'fyto.msi')
  if($LASTEXITCODE -ne 0){throw 'MSI build failed'}
  & ./build-bootstrap.ps1 -Msi (Join-Path $build 'fyto.msi') -Output (Join-Path $build 'fyto-Setup.exe')
 }finally{Pop-Location}
}
$msi=Join-Path $out "fyto-Setup-$version.msi"
Copy-Item (Join-Path $build 'fyto.msi') $msi
$updater=Join-Path $root 'voidImageViewer/res/fyto-updater.exe'
$test=Start-Process $updater -ArgumentList '--self-test' -PassThru -Wait -WindowStyle Hidden
if($test.ExitCode -ne 0){throw 'Updater self-test failed'}
$test=Start-Process $updater -ArgumentList "--verify-msi `"$msi`" $version" -PassThru -Wait -WindowStyle Hidden
if($test.ExitCode -ne 0){throw 'MSI update verification failed'}
$installer=Join-Path $out "fyto-Setup-$version.exe"
Copy-Item (Join-Path $build 'fyto-Setup.exe') $installer
$test=Start-Process $installer -ArgumentList '/verify' -PassThru -Wait -WindowStyle Hidden
if($test.ExitCode -ne 0){throw 'Installer verification failed'}
$zip=Join-Path $out "fyto-Setup-$version.zip"
Compress-Archive -Path $msi,(Join-Path $payload 'LICENSE'),(Join-Path $payload 'libwebp-COPYING'),(Join-Path $payload 'libwebp-PATENTS'),(Join-Path $payload 'NOTICE.txt'),(Join-Path $payload 'README.txt') -DestinationPath $zip -Force
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$source=Join-Path $out "fyto-source-$version.zip"
if(Test-Path $source){Remove-Item -LiteralPath $source}
$archive=[IO.Compression.ZipFile]::Open($source,[IO.Compression.ZipArchiveMode]::Create)
try{
 foreach($file in Get-ChildItem -LiteralPath $root -Recurse -File){
  $relative=$file.FullName.Substring($root.Length+1).Replace('\','/')
  if($relative -match '(^|/)(\.git|\.vs|\.tools|build|release-assets|x64|Release|Debug|__pycache__|\.wix)(/|$)' -or $file.Extension -match '^\.(exe|msi|zip|obj|pdb|pch|res|tlog|log|wixpdb|aps|cache)$'){continue}
  if($file.Name -in @('fyto.ini','associations.ini','updates.ini')){continue}
  [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive,$file.FullName,$relative,[IO.Compression.CompressionLevel]::Optimal) | Out-Null
 }
}finally{$archive.Dispose()}
$lines=@($msi,$installer,$zip,$source) | ForEach-Object { "$((Get-FileHash -LiteralPath $_ -Algorithm SHA256).Hash.ToLowerInvariant())  $([IO.Path]::GetFileName($_))" }
[IO.File]::WriteAllLines((Join-Path $out 'SHA256SUMS.txt'),$lines,(New-Object Text.UTF8Encoding($false)))
Write-Output "Release assets: $out"
