param([string]$Msi,[string]$Output)
$ErrorActionPreference='Stop'
$src=$PSScriptRoot
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vsPath=(& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath)
$vcRoot=(Get-ChildItem -LiteralPath (Join-Path $vsPath 'VC\Tools\MSVC') -Directory | Sort-Object Name -Descending | Select-Object -First 1).FullName
$sdkRoot='C:\Program Files (x86)\Windows Kits\10'
$sdkVersion=(Get-ChildItem -LiteralPath "$sdkRoot\Lib" -Directory | Where-Object { $_.Name -match '^\d+\.\d+\.\d+\.\d+$' } | Sort-Object { [version]$_.Name } -Descending | Select-Object -First 1).Name
if(!$sdkVersion){throw 'A versioned Windows SDK is required'}
$env:INCLUDE="$vcRoot\include;$sdkRoot\Include\$sdkVersion\ucrt;$sdkRoot\Include\$sdkVersion\shared;$sdkRoot\Include\$sdkVersion\um"
$env:LIB="$vcRoot\lib\x64;$sdkRoot\Lib\$sdkVersion\ucrt\x64;$sdkRoot\Lib\$sdkVersion\um\x64"
$buildDir=Join-Path (Split-Path -Parent $Msi) 'setup-build'
New-Item -ItemType Directory -Force -Path $buildDir | Out-Null
$msiRc=$Msi.Replace('\','\\')
$iconRc=(Join-Path $src '..\res\voidImageViewer.ico').Replace('\','\\')
$manifestRc=(Join-Path $src 'setup.manifest').Replace('\','\\')
$versionHeader=[IO.File]::ReadAllText((Join-Path $src '..\src\version.h'))
$parts=@('MAJOR','MINOR','REVISION','BUILD') | ForEach-Object { [int][regex]::Match($versionHeader,('VERSION_'+$_+'\s+(\d+)')).Groups[1].Value }
$version=$parts -join '.'
$versionCommas=$parts -join ','
$rcText="101 RCDATA `"$msiRc`"`n1 ICON `"$iconRc`"`n1 24 `"$manifestRc`"`n"
$rcText+=@"
1 VERSIONINFO
FILEVERSION $versionCommas
PRODUCTVERSION $versionCommas
FILEOS 0x40004
FILETYPE 1
BEGIN
 BLOCK "StringFileInfo"
 BEGIN
  BLOCK "040904b0"
  BEGIN
   VALUE "FileVersion", "$version\0"
   VALUE "ProductVersion", "$version\0"
   VALUE "ProductName", "fyto Installer\0"
   VALUE "FileDescription", "fyto Setup\0"
  END
 END
 BLOCK "VarFileInfo"
 BEGIN
  VALUE "Translation", 0x409, 1200
 END
END
"@
[IO.File]::WriteAllText((Join-Path $buildDir 'setup.rc'),$rcText,[Text.Encoding]::ASCII)
& "$sdkRoot\bin\$sdkVersion\x64\rc.exe" /nologo /fo (Join-Path $buildDir 'setup.res') (Join-Path $buildDir 'setup.rc')
if($LASTEXITCODE -ne 0){throw 'Resource build failed'}
& "$vcRoot\bin\Hostx64\x64\cl.exe" /nologo /O2 /MT /utf-8 (Join-Path $src 'bootstrap.c') (Join-Path $buildDir 'setup.res') "/Fo:$buildDir\setup.obj" "/Fe:$Output" /link /SUBSYSTEM:WINDOWS /MACHINE:X64 /MANIFEST:NO msi.lib shlwapi.lib ole32.lib advapi32.lib user32.lib shell32.lib
if($LASTEXITCODE -ne 0){throw 'Installer executable build failed'}
