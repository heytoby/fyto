param([Parameter(Mandatory=$true)][string]$Version)
$ErrorActionPreference='Stop'
if($Version -notmatch '^1\.0\.0\.(\d+)$' -or [int]$Matches[1] -gt 65535){throw 'Expected 1.0.0.BUILD, BUILD <= 65535'}
$build=[int]$Matches[1]
$root=Split-Path -Parent $PSScriptRoot
$files=@('voidImageViewer/src/version.h','voidImageViewer/res/voidImageViewer.rc','voidImageViewer/installer/Package.wxs','voidImageViewer/installer/license.rtf','voidImageViewer/installer/setup.manifest','RELEASE_NOTES.md')
foreach($file in $files){
 $path=Join-Path $root $file
 # Latin-1 round trip preserves the original resource/RTF bytes.
 $encoding=[Text.Encoding]::GetEncoding(28591)
 $text=$encoding.GetString([IO.File]::ReadAllBytes($path))
 $text=[regex]::Replace($text,'1\.0\.0\.\d+',$Version)
 $text=[regex]::Replace($text,'1,0,0,\d+',"1,0,0,$build")
 $text=[regex]::Replace($text,'Version="1\.0\.\d+"',"Version=`"1.0.$build`"")
 $text=[regex]::Replace($text,'VERSION_BUILD\s+\d+',"VERSION_BUILD`t`t$build")
 [IO.File]::WriteAllBytes($path,$encoding.GetBytes($text))
}
Write-Output "Version set to $Version. Update RELEASE_NOTES.md before publishing."
