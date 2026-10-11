<#
.SYNOPSIS
    Downloads FFmpeg (prebuilt), builds SoundTouch (CMake) and
    Rubber Band (Meson) as static libraries, then generates
    third_party/deps.pri for inclusion from the .pro file.

.NOTES
    - Qt lives under $QtDir (default: E:\qt6).
    - The compiler lives under $MinGwDir (default: E:\llvm-mingw).
      Both "E:\llvm-mingw" and "E:\llvm_mingw" are accepted.
    - Qt Tools for llvm-mingw are picked up from
      $QtDir\6.7.3\llvm-mingw_64 if that folder exists.
    - Sources live under third_party\src\<lib>-src; build trees under
      third_party\src\<lib>-build; install trees under third_party\<lib>.
    - Both SoundTouch and Rubber Band are installed as static .a.
    - deps.pri is regenerated on every run and is meant to be pulled
      into the .pro with:  include(third_party/deps.pri)
    - If Python or Meson are missing, the script installs them via
      winget + pip, disables the Microsoft Store python stubs, and
      updates PATH for the current session.
    - Meson is always invoked as "python -m mesonbuild.mesonmain".
    - ASCII-only, no here-strings: parses in any encoding, works in
      PowerShell 5.1 and 7+.

.EXAMPLE
    .\setup_audio_deps.ps1
    .\setup_audio_deps.ps1 -QtDir E:\qt6 -MinGwDir E:\llvm_mingw
#>

param(
    [string]$QtDir    = "E:\qt6",
    [string]$MinGwDir = "E:\llvm-mingw",
    [string]$ProFile  = "audio_speed_changer.pro",
    [string]$OutDir   = "third_party"
)

$ErrorActionPreference = "Stop"
$ProgressPreference    = "SilentlyContinue"

# ---------------------------------------------------------------
# 0. Output helpers
# ---------------------------------------------------------------
function Write-Step([string]$msg)  { Write-Host ""; Write-Host "==> $msg" -ForegroundColor Cyan }
function Write-Ok([string]$msg)    { Write-Host "    OK: $msg" -ForegroundColor Green }
function Write-Warn2([string]$msg) { Write-Host "    !!: $msg" -ForegroundColor Yellow }

function Test-NativeSuccess {
    param(
        [Parameter(Mandatory)][string]$Exe,
        [Parameter(Mandatory)][string[]]$CmdArgs
    )
    $prev = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        & $Exe @CmdArgs 2>&1 | Out-Null
        return ($LASTEXITCODE -eq 0)
    } finally {
        $ErrorActionPreference = $prev
    }
}

function Invoke-Checked {
    param(
        [Parameter(Mandatory)][string]$Exe,
        [Parameter(ValueFromRemainingArguments)][string[]]$CmdArgs,
        [string]$FailMessage = "Command failed"
    )
    $prev = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        & $Exe @CmdArgs
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $prev
    }
    if ($code -ne 0) {
        throw "$FailMessage (exit code $code)"
    }
}

function Test-Admin {
    $id = [Security.Principal.WindowsIdentity]::GetCurrent()
    $p  = New-Object Security.Principal.WindowsPrincipal($id)
    return $p.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Ensure-PythonAndMeson {
    param([ref]$OutPythonExe)

    $pythonExe = $null

    foreach ($cmd in @("python.exe", "python3.exe")) {
        $p = Get-Command $cmd -ErrorAction SilentlyContinue
        if (-not $p) { continue }
        if ($p.Source -like "*\WindowsApps\*") {
            Write-Warn2 "Skipping Microsoft Store python stub at $($p.Source)"
            continue
        }
        if (-not (Test-NativeSuccess -Exe $p.Source -CmdArgs @("--version"))) {
            Write-Warn2 "python at $($p.Source) did not run cleanly - skipping"
            continue
        }
        $pythonExe = $p.Source
        break
    }

    if (-not $pythonExe) {
        Write-Warn2 "No real Python found - installing Python 3.12 via winget"

        $winget = (Get-Command winget.exe -ErrorAction SilentlyContinue).Source
        if (-not $winget) {
            throw "winget is not available. Install App Installer from the Microsoft Store, or install Python manually from python.org."
        }

        $scope = if (Test-Admin) { "machine" } else { "user" }
        Write-Host "    Using winget scope: $scope"

        $overrideArgs = if ($scope -eq "machine") {
            '/quiet InstallAllUsers=1 PrependPath=1 Include_pip=1 Include_test=0 Include_launcher=1 InstallLauncherAllUsers=1'
        } else {
            '/quiet InstallAllUsers=0 PrependPath=1 Include_pip=1 Include_test=0 Include_launcher=1'
        }

        Invoke-Checked -Exe $winget -CmdArgs @(
            "install", "--id", "Python.Python.3.12", "-e",
            "--scope", $scope,
            "--silent",
            "--accept-source-agreements",
            "--accept-package-agreements",
            "--override", $overrideArgs
        ) -FailMessage "winget failed to install Python 3.12."

        Write-Host "    Disabling Microsoft Store python stubs..."
        try {
            Get-AppxPackage PythonSoftwareFoundation.PythonManager -ErrorAction SilentlyContinue |
                Remove-AppxPackage -ErrorAction SilentlyContinue
        } catch { }

        $stubDir = Join-Path $env:LOCALAPPDATA "Microsoft\WindowsApps"
        foreach ($name in @("python.exe", "python3.exe")) {
            $stub = Join-Path $stubDir $name
            if (Test-Path $stub) {
                try { Remove-Item $stub -Force -ErrorAction SilentlyContinue } catch { }
            }
        }

        $machinePath = [Environment]::GetEnvironmentVariable("Path", "Machine")
        $userPath    = [Environment]::GetEnvironmentVariable("Path", "User")
        $env:PATH    = "$machinePath;$userPath"

        $p = Get-Command python.exe -ErrorAction SilentlyContinue
        if (-not $p) {
            throw "Python was installed but is still not on PATH in this session. Close and reopen the terminal, then re-run the script."
        }
        $pythonExe = $p.Source
        Write-Ok "Python installed: $pythonExe"
    } else {
        Write-Ok "Python found: $pythonExe"
    }

    if (-not (Test-NativeSuccess -Exe $pythonExe -CmdArgs @("-m", "pip", "--version"))) {
        Write-Warn2 "pip not found - trying ensurepip"
        Invoke-Checked -Exe $pythonExe -CmdArgs @("-m", "ensurepip", "--upgrade") `
            -FailMessage "Could not bootstrap pip."
    }

    if (-not (Test-NativeSuccess -Exe $pythonExe -CmdArgs @("-m", "mesonbuild.mesonmain", "--version"))) {
        Write-Warn2 "Meson not found - installing with pip"
        Invoke-Checked -Exe $pythonExe -CmdArgs @("-m", "pip", "install", "--user", "meson") `
            -FailMessage "Failed to install Meson via pip."

        if (-not (Test-NativeSuccess -Exe $pythonExe -CmdArgs @("-m", "mesonbuild.mesonmain", "--version"))) {
            throw "Meson is still not importable after 'pip install --user meson'. Check pip output above."
        }
    }

    Write-Ok "Meson: python -m mesonbuild.mesonmain ($pythonExe)"
    $OutPythonExe.Value = $pythonExe
}

# ---------------------------------------------------------------
# 1. Environment discovery
# ---------------------------------------------------------------
Write-Step "Locating Qt, compiler and tools"

if (-not (Test-Path $QtDir)) {
    throw "Qt directory not found: $QtDir. Pass -QtDir <path>."
}

if (-not (Test-Path $MinGwDir)) {
    $alt = $MinGwDir -replace '-', '_'
    if (Test-Path $alt) {
        $MinGwDir = $alt
    } else {
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
        return [pscustomobject]@{
            Bin  = $bin
            C    = Join-Path $bin "gcc.exe"
            CXX  = $gxx
            Kind = "mingw"
        }
    }

    $cxx = Join-Path $bin "clang++.exe"
    if (Test-Path $cxx) {
        return [pscustomobject]@{
            Bin  = $bin
            C    = Join-Path $bin "clang.exe"
            CXX  = $cxx
            Kind = "llvm-mingw"
        }
    }

    return $null
}

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

if (-not $compilerBin) {
    $candidates = @(Get-ChildItem "$QtDir\Tools" -Directory -ErrorAction SilentlyContinue |
        Where-Object {
            $_.Name -like "mingw*"      -or
            $_.Name -like "llvm-mingw*" -or
            $_.Name -like "*llvm*mingw*"
        })
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

if (-not $compilerBin) {
    $g = Get-Command g++.exe -ErrorAction SilentlyContinue
    if ($g) {
        $compilerBin  = Split-Path $g.Source -Parent
        $compilerC    = Join-Path $compilerBin "gcc.exe"
        $compilerCXX  = $g.Source
        $compilerKind = "mingw"
    }
}
if (-not $compilerBin) {
    throw "No compiler (g++/clang++) found. Check -MinGwDir or Qt Tools."
}

$cmakeExe = Get-ChildItem "$QtDir\Tools" -Directory -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -like "CMake*" } |
    ForEach-Object { Join-Path $_.FullName "bin\cmake.exe" } |
    Where-Object { Test-Path $_ } |
    Select-Object -First 1
if (-not $cmakeExe) {
    $cmakeExe = (Get-Command cmake.exe -ErrorAction SilentlyContinue).Source
}
if (-not $cmakeExe) { throw "CMake not found. Install the CMake component or add cmake to PATH." }

$ninjaExe = Get-ChildItem "$QtDir\Tools" -Directory -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -like "Ninja*" } |
    ForEach-Object { Join-Path $_.FullName "ninja.exe" } |
    Where-Object { Test-Path $_ } |
    Select-Object -First 1
if (-not $ninjaExe) {
    $ninjaExe = (Get-Command ninja -ErrorAction SilentlyContinue).Source
}
if (-not $ninjaExe) {
    $cand = Join-Path $compilerBin "ninja.exe"
    if (Test-Path $cand) { $ninjaExe = $cand }
}
if (-not $ninjaExe) { Write-Warn2 "Ninja not found - CMake will use mingw32-make (slower)." }

$gitExe = (Get-Command git -ErrorAction SilentlyContinue).Source
if (-not $gitExe) { throw "Git not found in PATH. Install Git for Windows." }

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

$env:CC  = $compilerC
$env:CXX = $compilerCXX

$env:PATH = "$compilerBin;$env:PATH"
if ($ninjaExe) {
    $ninjaDir = Split-Path $ninjaExe -Parent
    $env:PATH = "$ninjaDir;$env:PATH"
}

$qtToolsLlvm = Join-Path $QtDir "6.7.3\llvm-mingw_64"
$cmakePrefixArg = $null
if (Test-Path $qtToolsLlvm) {
    $cmakePrefixArg = "-DCMAKE_PREFIX_PATH=$qtToolsLlvm"
    Write-Ok "Qt Tools (llvm-mingw): $qtToolsLlvm"
} else {
    Write-Warn2 "Qt Tools (llvm-mingw) not found at $qtToolsLlvm - continuing without it."
}

$cmakeCompilerArgs = @(
    "-DCMAKE_C_COMPILER=$compilerC",
    "-DCMAKE_CXX_COMPILER=$compilerCXX"
)
if ($cmakePrefixArg) { $cmakeCompilerArgs += $cmakePrefixArg }

# ---------------------------------------------------------------
# 2. FFmpeg
# ---------------------------------------------------------------
Write-Step "FFmpeg: downloading a prebuilt MinGW build"

$ffmpegDir = Join-Path $thirdParty "ffmpeg"
if ((Test-Path (Join-Path $ffmpegDir "include")) -and
    (Test-Path (Join-Path $ffmpegDir "lib"))) {
    Write-Ok "FFmpeg already present - skipping."
} else {
    $ffmpegUrl = "https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/ffmpeg-master-latest-win64-gpl-shared.zip"
    $ffmpegZip = Join-Path $srcDir "ffmpeg.zip"

    Write-Host "    Downloading $ffmpegUrl"
    Invoke-WebRequest -Uri $ffmpegUrl -OutFile $ffmpegZip -UseBasicParsing

    Write-Host "    Extracting..."
    Expand-Archive -Path $ffmpegZip -DestinationPath $srcDir -Force
    $extracted = @(Get-ChildItem $srcDir -Directory | Where-Object { $_.Name -like "ffmpeg-*" })[0]
    if (-not $extracted) { throw "Could not find the extracted FFmpeg directory." }

    if (Test-Path $ffmpegDir) { Remove-Item $ffmpegDir -Recurse -Force }
    New-Item -ItemType Directory -Force -Path $ffmpegDir | Out-Null
    Move-Item (Join-Path $extracted.FullName "include") (Join-Path $ffmpegDir "include") -Force
    Move-Item (Join-Path $extracted.FullName "lib")     (Join-Path $ffmpegDir "lib")     -Force
    Move-Item (Join-Path $extracted.FullName "bin")     (Join-Path $ffmpegDir "bin")     -Force
    Remove-Item $ffmpegZip -Force
    Remove-Item $extracted.FullName -Recurse -Force
    Write-Ok "FFmpeg ready: $ffmpegDir"
}

# ---------------------------------------------------------------
# 3. SoundTouch
# ---------------------------------------------------------------
Write-Step "SoundTouch: building from source"

$stDir     = Join-Path $thirdParty "soundtouch"
$stLibFile = Join-Path $stDir "lib\libSoundTouch.a"

if ((Test-Path (Join-Path $stDir "include")) -and (Test-Path $stLibFile)) {
    Write-Ok "SoundTouch already built - skipping."
} else {
    $stSrc = Join-Path $srcDir "soundtouch-src"
    if (-not (Test-Path (Join-Path $stSrc "CMakeLists.txt"))) {
        if (Test-Path $stSrc) { Remove-Item $stSrc -Recurse -Force }
        Invoke-Checked -Exe $gitExe -CmdArgs @(
            "clone", "--depth", "1",
            "https://codeberg.org/soundtouch/soundtouch.git",
            $stSrc
        ) -FailMessage "git clone SoundTouch failed."
    }

    $stBuild = Join-Path $srcDir "soundtouch-build"
    if (Test-Path $stBuild) { Remove-Item $stBuild -Recurse -Force }
    New-Item -ItemType Directory -Force -Path $stBuild | Out-Null
    if (Test-Path $stDir)   { Remove-Item $stDir   -Recurse -Force }

    $cmakeArgs = @(
        "-S", $stSrc,
        "-B", $stBuild,
        "-DCMAKE_BUILD_TYPE=Release",
        "-DCMAKE_INSTALL_PREFIX=$stDir",
        "-DBUILD_SHARED_LIBS=OFF",
        "-DSOUNDTOUCH_DLL=OFF",
        "-DSOUNDSTRETCH=OFF",
        "-DBUILD_TESTING=OFF"
    ) + $cmakeCompilerArgs

    if ($ninjaExe) { $cmakeArgs += @("-G", "Ninja", "-DCMAKE_MAKE_PROGRAM=$ninjaExe") }

    Invoke-Checked -Exe $cmakeExe -CmdArgs $cmakeArgs -FailMessage "CMake configure SoundTouch failed."
    Invoke-Checked -Exe $cmakeExe -CmdArgs @(
        "--build", $stBuild, "--config", "Release", "--target", "install"
    ) -FailMessage "CMake build/install SoundTouch failed."

    if (-not (Test-Path $stLibFile)) {
        throw "SoundTouch build reported success but $stLibFile is missing."
    }
    Write-Ok "SoundTouch built: $stLibFile"
}

# ---------------------------------------------------------------
# 4. Rubber Band
# ---------------------------------------------------------------
Write-Step "Rubber Band: building from source with Meson"

$rbDir     = Join-Path $thirdParty "rubberband"
$rbLibFile = Join-Path $rbDir "lib\librubberband.a"

if ((Test-Path (Join-Path $rbDir "include")) -and (Test-Path $rbLibFile)) {
    Write-Ok "Rubber Band already built - skipping."
} else {

    $pythonExe = $null
    Ensure-PythonAndMeson -OutPythonExe ([ref]$pythonExe)

    $script:PythonExe = $pythonExe
    function Invoke-Meson {
        param([Parameter(Mandatory)][string[]]$CmdArgs)
        $prev = $ErrorActionPreference
        $ErrorActionPreference = "Continue"
        try {
            & $script:PythonExe -m mesonbuild.mesonmain @CmdArgs
            $code = $LASTEXITCODE
        } finally {
            $ErrorActionPreference = $prev
        }
        if ($code -ne 0) {
            throw "Meson command failed (exit $code): meson $($CmdArgs -join ' ')"
        }
    }

    $rbSrc = Join-Path $srcDir "rubberband-src"
    if (-not (Test-Path (Join-Path $rbSrc "meson.build"))) {
        if (Test-Path $rbSrc) { Remove-Item $rbSrc -Recurse -Force }
        Invoke-Checked -Exe $gitExe -CmdArgs @(
            "clone", "--depth", "1", "--recursive",
            "https://github.com/breakfastquay/rubberband.git",
            $rbSrc
        ) -FailMessage "git clone Rubber Band failed."
    }

    $rbBuild = Join-Path $srcDir "rubberband-build"
    if (Test-Path $rbBuild) { Remove-Item $rbBuild -Recurse -Force }
    if (Test-Path $rbDir)   { Remove-Item $rbDir   -Recurse -Force }

    $rbPrefixMeson = $rbDir -replace '\\','/'

    Invoke-Meson -CmdArgs @(
        "setup", $rbBuild, $rbSrc,
        "--buildtype=release",
        "--default-library=static",
        "--prefix=$rbPrefixMeson",
        "-Dcmdline=disabled",
        "-Dtests=disabled",
        "-Dladspa=disabled",
        "-Dlv2=disabled",
        "-Dvamp=disabled",
        "-Djni=disabled"
    )
    Invoke-Meson -CmdArgs @("compile", "-C", $rbBuild)
    Invoke-Meson -CmdArgs @("install", "-C", $rbBuild)

    if (-not (Test-Path $rbLibFile)) {
        throw "Rubber Band build reported success but $rbLibFile is missing."
    }
    Write-Ok "Rubber Band built: $rbLibFile"
}

# ---------------------------------------------------------------
# 5. Generate third_party/deps.pri
# ---------------------------------------------------------------
Write-Step "Generating deps.pri"

function ToQmakePath([string]$p) { return ($p -replace '\\','/') }

$ffmpegInc = ToQmakePath (Join-Path $ffmpegDir "include")
$ffmpegLib = ToQmakePath (Join-Path $ffmpegDir "lib")
$ffmpegBin = ToQmakePath (Join-Path $ffmpegDir "bin")
$stInc     = ToQmakePath (Join-Path $stDir     "include")
$stLib     = ToQmakePath (Join-Path $stDir     "lib")
$rbInc     = ToQmakePath (Join-Path $rbDir     "include")
$rbLib     = ToQmakePath (Join-Path $rbDir     "lib")

# Use a distinct name so we do not clobber $compilerBin from section 1.
$compilerBinQmake = ToQmakePath $compilerBin

$depsLines = @(
    "# ----- AUTO-GENERATED BY setup_audio_deps.ps1 -----",
    "# Do not edit manually - will be overwritten.",
    "",
    "# FFmpeg (shared)",
    "INCLUDEPATH += `"$ffmpegInc`"",
    "LIBS += -L`"$ffmpegLib`" -lavformat -lavcodec -lavutil -lswresample",
    "DEFINES += HAVE_FFMPEG",
    "FFMPEG_BIN_DIR = `"$ffmpegBin`"",
    "",
    "# SoundTouch (static)",
    "INCLUDEPATH += `"$stInc`"",
    "LIBS += -L`"$stLib`" -lSoundTouch",
    "DEFINES += HAVE_SOUNDTOUCH",
    "",
    "# Rubber Band (static)",
    "INCLUDEPATH += `"$rbInc`"",
    "LIBS += -L`"$rbLib`" -lrubberband",
    "DEFINES += HAVE_RUBBERBAND",
    "",
    "# Compiler runtime DLLs (llvm-mingw / MinGW)",
    "COMPILER_BIN_DIR = `"$compilerBinQmake`"",
    "",
    "# ----- AUTO-GENERATED BY setup_audio_deps.ps1 -----"
)

$depsText = ($depsLines -join "`r`n") + "`r`n"
$depsPath = Join-Path $thirdParty "deps.pri"

[System.IO.File]::WriteAllText($depsPath, $depsText, [System.Text.Encoding]::ASCII)
Write-Ok "written $depsPath"

# ---------------------------------------------------------------
# 6. Copy DLLs
# ---------------------------------------------------------------
Write-Step "Copying DLLs into build directories"

$runtimeDlls = @()
if ($compilerKind -eq "mingw") {
    $runtimeDlls += @(Get-ChildItem (Join-Path $compilerBin "libgcc_s_*.dll")      -ErrorAction SilentlyContinue)
    $runtimeDlls += @(Get-ChildItem (Join-Path $compilerBin "libstdc++-6.dll")     -ErrorAction SilentlyContinue)
    $runtimeDlls += @(Get-ChildItem (Join-Path $compilerBin "libwinpthread-1.dll") -ErrorAction SilentlyContinue)
} else {
    $runtimeDlls += @(Get-ChildItem (Join-Path $compilerBin "libc++.dll")    -ErrorAction SilentlyContinue)
    $runtimeDlls += @(Get-ChildItem (Join-Path $compilerBin "libunwind.dll") -ErrorAction SilentlyContinue)
}

$buildDirs = @(Get-ChildItem $projectDir -Directory -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -like "build-*" -or $_.Name -eq "debug" -or $_.Name -eq "release" })

if ($buildDirs.Count -eq 0) {
    Write-Warn2 "No build directories found - copy the DLLs manually after the first build."
} else {
    $ffmpegDlls = @(Get-ChildItem (Join-Path $ffmpegDir "bin\*.dll") -ErrorAction SilentlyContinue)
    foreach ($bd in $buildDirs) {
        $targetDirs = @($bd.FullName)
        foreach ($sub in @("debug","release")) {
            $p = Join-Path $bd.FullName $sub
            if (Test-Path $p) { $targetDirs += $p }
        }
        foreach ($td in $targetDirs) {
            foreach ($dll in $ffmpegDlls)  { Copy-Item $dll.FullName -Destination $td -Force }
            foreach ($dll in $runtimeDlls) { Copy-Item $dll.FullName -Destination $td -Force }
        }
        Write-Ok "DLLs copied into $($bd.Name)"
    }
}

# ---------------------------------------------------------------
# 7. Summary
# ---------------------------------------------------------------
Write-Step "Done"
Write-Host "Next steps in Qt Creator:" -ForegroundColor Cyan
Write-Host "  1. Make sure your .pro contains:  include(third_party/deps.pri)"
Write-Host "  2. Build -> Run qmake"
Write-Host "  3. Build -> Rebuild"
Write-Host ""
Write-Host "Static libraries built:" -ForegroundColor Cyan
Write-Host "  $stLibFile"
Write-Host "  $rbLibFile"
Write-Host ""
Write-Host "Install trees:" -ForegroundColor Cyan
Write-Host "  FFmpeg:     $ffmpegDir"
Write-Host "  SoundTouch: $stDir"
Write-Host "  RubberBand: $rbDir"
Write-Host ""
Write-Host "Generated: $depsPath" -ForegroundColor Cyan