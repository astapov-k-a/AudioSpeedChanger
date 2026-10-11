<#
.SYNOPSIS
    Downloads and builds FFmpeg, SoundTouch and Rubber Band for the
    audio-speed-changer Qt project, and wires them into the .pro file.

.NOTES
    - Qt lives under $QtDir (default E:\qt6).
    - The compiler lives under $MinGwDir (default E:\llvm-mingw).
      Both E:\llvm-mingw and E:\llvm_mingw are accepted.
    - Qt Tools for llvm-mingw are expected at
      $QtDir\6.7.3\llvm-mingw_64 (added to CMAKE_PREFIX_PATH if present).
    - ASCII-only, no here-strings.
#>

param(
    [string]$QtDir    = "E:\qt6",
    [string]$MinGwDir = "E:\llvm-mingw",
    [string]$ProFile  = "audio_speed_changer.pro",
    [string]$OutDir   = "third_party"
)

$ErrorActionPreference = "Stop"
$ProgressPreference    = "SilentlyContinue"

function Write-Step([string]$msg)  { Write-Host ""; Write-Host "==> $msg" -ForegroundColor Cyan }
function Write-Ok([string]$msg)    { Write-Host "    OK: $msg" -ForegroundColor Green }
function Write-Warn2([string]$msg) { Write-Host "    !!: $msg" -ForegroundColor Yellow }

# ---------------------------------------------------------------
# 1. Environment discovery
# ---------------------------------------------------------------
Write-Step "Locating Qt, compiler and tools"

if (-not (Test-Path $QtDir)) {
    throw "Qt directory not found: $QtDir. Pass -QtDir <path>."
}

# +++ accept both - and _ in the llvm-mingw folder name
if (-not (Test-Path $MinGwDir)) {
    $alt = $MinGwDir -replace '-', '_'
    if (Test-Path $alt) { $MinGwDir = $alt }
    else {
        $alt = $MinGwDir -replace '_', '-'
        if (Test-Path $alt) { $MinGwDir = $alt }
    }
}

$compilerBin  = $null
$compilerC    = $null
$compilerCXX  = $null
$compilerKind = $null

function Test-CompilerDir([string]$bin) {
    if (-not (Test-Path $bin)) { return $null }
    $gxx = Join-Path $bin "g++.exe"
    if (Test-Path $gxx) {
        return [pscustomobject]@{ Bin = $bin; C = Join-Path $bin "gcc.exe"; CXX = $gxx; Kind = "mingw" }
    }
    $cxx = Join-Path $bin "clang++.exe"
    if (Test-Path $cxx) {
        return [pscustomobject]@{ Bin = $bin; C = Join-Path $bin "clang.exe"; CXX = $cxx; Kind = "llvm-mingw" }
    }
    return $null
}

# 1) user-provided MinGwDir
if (Test-Path $MinGwDir) {
    $bin = Join-Path $MinGwDir "bin"
    $res = Test-CompilerDir $bin
    if (-not $res -and (Test-Path (Join-Path $MinGwDir "g++.exe"))) {
        $res = Test-CompilerDir $MinGwDir
    }
    if ($res) {
        $compilerBin  = $res.Bin
        $compilerC    = $res.C
        $compilerCXX  = $res.CXX
        $compilerKind = $res.Kind
    } else {
        Write-Warn2 "No g++/clang++ in $MinGwDir - falling back to Qt Tools."
    }
} else {
    Write-Warn2 "MinGwDir not found: $MinGwDir - falling back to Qt Tools."
}

# 2) Qt Tools
if (-not $compilerBin) {
    $candidates = @(Get-ChildItem "$QtDir\Tools" -Directory -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -like "mingw*" -or $_.Name -like "llvm-mingw*" -or $_.Name -like "*llvm*mingw*" })
    foreach ($dir in $candidates) {
        $res = Test-CompilerDir (Join-Path $dir.FullName "bin")
        if ($res) {
            $compilerBin  = $res.Bin
            $compilerC    = $res.C
            $compilerCXX  = $res.CXX
            $compilerKind = $res.Kind
            break
        }
    }
}

# 3) PATH
if (-not $compilerBin) {
    $g = Get-Command g++.exe -ErrorAction SilentlyContinue
    if ($g) {
        $compilerBin  = Split-Path $g.Source -Parent
        $compilerC    = Join-Path $compilerBin "gcc.exe"
        $compilerCXX  = $g.Source
        $compilerKind = "mingw"
    }
}
if (-not $compilerBin) { throw "No compiler (g++/clang++) found. Check -MinGwDir or Qt Tools." }

# --- CMake / Ninja / git (как раньше) ---
$cmakeExe = Get-ChildItem "$QtDir\Tools" -Directory -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -like "CMake*" } |
    ForEach-Object { Join-Path $_.FullName "bin\cmake.exe" } |
    Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $cmakeExe) { $cmakeExe = (Get-Command cmake.exe -ErrorAction SilentlyContinue).Source }
if (-not $cmakeExe) { throw "CMake not found." }

$ninjaExe = Get-ChildItem "$QtDir\Tools" -Directory -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -like "Ninja*" } |
    ForEach-Object { Join-Path $_.FullName "ninja.exe" } |
    Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $ninjaExe) { $ninjaExe = (Get-Command ninja -ErrorAction SilentlyContinue).Source }
if (-not $ninjaExe) { $cand = Join-Path $compilerBin "ninja.exe"; if (Test-Path $cand) { $ninjaExe = $cand } }

$gitExe = (Get-Command git -ErrorAction SilentlyContinue).Source
if (-not $gitExe) { throw "Git not found in PATH." }

$projectDir = (Get-Location).Path
if (-not (Test-Path (Join-Path $projectDir $ProFile))) {
    throw "File $ProFile not found in $projectDir. Run this script from the project root."
}

Write-Ok "Qt:         $QtDir"
Write-Ok "Compiler:   $compilerKind ($compilerCXX)"
Write-Ok "CMake:      $cmakeExe"
Write-Ok "Ninja:      $(if ($ninjaExe) { $ninjaExe } else { 'not found' })"
Write-Ok "Project:    $projectDir"

$thirdParty = Join-Path $projectDir $OutDir
New-Item -ItemType Directory -Force -Path $thirdParty | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $thirdParty "src") | Out-Null
$srcDir = Join-Path $thirdParty "src"

$env:PATH = "$compilerBin;$env:PATH"

# +++ CMAKE_PREFIX_PATH: add Qt Tools for llvm-mingw if present
$qtToolsLlvm = Join-Path $QtDir "6.7.3\llvm-mingw_64"
$cmakePrefix = ""
if (Test-Path $qtToolsLlvm) {
    $cmakePrefix = "-DCMAKE_PREFIX_PATH=$qtToolsLlvm"
    Write-Ok "Qt Tools (llvm-mingw): $qtToolsLlvm"
} else {
    Write-Warn2 "Qt Tools (llvm-mingw) not found at $qtToolsLlvm - continuing without it."
}

$cmakeCompilerArgs = @(
    "-DCMAKE_C_COMPILER=$compilerC",
    "-DCMAKE_CXX_COMPILER=$compilerCXX"
)
if ($cmakePrefix) { $cmakeCompilerArgs += $cmakePrefix }

# ---------------------------------------------------------------
# 2. FFmpeg (как раньше)
# ---------------------------------------------------------------
# ... без изменений ...

# ---------------------------------------------------------------
# 3. SoundTouch
# ---------------------------------------------------------------
Write-Step "SoundTouch: building from source"

$stDir = Join-Path $thirdParty "soundtouch"
if (Test-Path (Join-Path $stDir "include")) {
    Write-Ok "SoundTouch already built - skipping."
} else {
    $stSrc = Join-Path $srcDir "soundtouch-src"
    if (-not (Test-Path $stSrc)) {
        & $gitExe clone --depth 1 https://codeberg.org/soundtouch/soundtouch.git $stSrc
        if ($LASTEXITCODE -ne 0) { throw "git clone SoundTouch failed." }
    }

    $stBuild = Join-Path $srcDir "soundtouch-build"
    New-Item -ItemType Directory -Force -Path $stBuild | Out-Null

    $cmakeArgs = @(
        "-S", $stSrc,
        "-B", $stBuild,
        "-DCMAKE_BUILD_TYPE=Release",
        "-DCMAKE_INSTALL_PREFIX=$stDir",
        "-DBUILD_SHARED_LIBS=OFF",
        "-DSOUNDTOUCH_DLL=OFF",
        "-DSOUNDSTRETCH=OFF",          # +++ ключевой фикс
        "-DBUILD_TESTING=OFF"
    ) + $cmakeCompilerArgs

    if ($ninjaExe) { $cmakeArgs += @("-G", "Ninja", "-DCMAKE_MAKE_PROGRAM=$ninjaExe") }

    & $cmakeExe @cmakeArgs
    if ($LASTEXITCODE -ne 0) { throw "CMake configure SoundTouch failed." }

    & $cmakeExe --build $stBuild --config Release --target install
    if ($LASTEXITCODE -ne 0) { throw "CMake build SoundTouch failed." }

    Write-Ok "SoundTouch built: $stDir"
}

# ---------------------------------------------------------------
# 4. Rubber Band (как раньше)
# ---------------------------------------------------------------
# ... без изменений ...

# ---------------------------------------------------------------
# 5–7. Патч .pro и копирование DLL (как раньше)
# ---------------------------------------------------------------