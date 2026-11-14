param([string]$Root=(Split-Path -Parent $PSScriptRoot))
$ErrorActionPreference='Stop'
$version='3.2.1'
$sha='c27ad72a68da151177929aa969ae773a2da5ac2af9ce946079314fdac8cece7f'
$source=Join-Path $Root 'voidImageViewer/libjpeg-turbo'
$build=Join-Path $Root 'build/jpeg'
$archive=Join-Path $Root "build/libjpeg-turbo-$version.tar.gz"
New-Item -ItemType Directory -Path $build -Force | Out-Null
if (!(Test-Path -LiteralPath $archive)) {
    Invoke-WebRequest -Uri "https://github.com/libjpeg-turbo/libjpeg-turbo/releases/download/$version/libjpeg-turbo-$version.tar.gz" -OutFile $archive
}
if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ine $sha) { throw 'libjpeg-turbo source hash mismatch' }
New-Item -ItemType Directory -Path $source -Force | Out-Null
tar -xzf $archive --strip-components=1 -C $source
if ($LASTEXITCODE) { throw 'JPEG source extraction failed' }
$nasm=(Get-Command nasm -ErrorAction SilentlyContinue).Source
if (!$nasm) {
    foreach ($path in @('C:/Program Files/NASM/nasm.exe','C:/NASM/nasm.exe','C:/ProgramData/chocolatey/bin/nasm.exe')) {
        if (Test-Path -LiteralPath $path) { $nasm=$path; break }
    }
}
if (!$nasm) { throw 'NASM is required for the SIMD JPEG decoder; install NASM and add it to PATH.' }
cmake -S $source -B $build -G 'Visual Studio 17 2022' -A x64 "-DCMAKE_ASM_NASM_COMPILER=$($nasm.Replace('\','/'))" -DENABLE_SHARED=OFF -DENABLE_STATIC=ON -DWITH_TURBOJPEG=OFF -DWITH_TOOLS=OFF -DWITH_TESTS=OFF -DWITH_CRT_DLL=OFF -DREQUIRE_SIMD=ON
if ($LASTEXITCODE) { throw 'JPEG configuration failed' }
cmake --build $build --config Release --target jpeg-static --parallel 4
if ($LASTEXITCODE) { throw 'JPEG decoder build failed' }
Write-Output 'Static SIMD JPEG decoder built from verified libjpeg-turbo 3.2.1 source.'

