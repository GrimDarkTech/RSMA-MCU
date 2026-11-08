[CmdletBinding()]
param(
    [ValidateSet("Ninja", "Visual Studio")]
    [string]$Generator = "Ninja",
    [ValidateSet("Release", "Debug")]
    [string]$Configuration = "Release",
    [string]$InstallPrefix = ""
)

$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot
$llvmSource = Join-Path $repo "external\llvm-project\llvm"
$llvmBuild = Join-Path $repo "external\llvm-project\build"

if ([string]::IsNullOrWhiteSpace($InstallPrefix)) {
    $InstallPrefix = Join-Path $repo "toolchains\llvm"
}

if (-not (Test-Path -LiteralPath (Join-Path $llvmSource "CMakeLists.txt"))) {
    throw "LLVM submodule is not initialized. Run: git submodule update --init --recursive"
}

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw "CMake is required for the LLVM bootstrap build."
}

if ($Generator -eq "Ninja" -and -not (Get-Command ninja -ErrorAction SilentlyContinue)) {
    throw "Ninja was selected but was not found in PATH. Use -Generator 'Visual Studio' or install Ninja."
}

$generatorArgs = @("-G", "Ninja")
if ($Generator -eq "Visual Studio") {
    $generatorArgs = @("-G", "Visual Studio 17 2022", "-A", "x64")
}

cmake -S $llvmSource -B $llvmBuild @generatorArgs `
    "-DCMAKE_BUILD_TYPE=$Configuration" `
    "-DLLVM_ENABLE_PROJECTS=clang" `
    "-DLLVM_TARGETS_TO_BUILD=Native" `
    "-DLLVM_ENABLE_TERMINFO=OFF" `
    "-DLLVM_ENABLE_ZLIB=OFF" `
    "-DLLVM_ENABLE_LIBXML2=OFF" `
    "-DCMAKE_INSTALL_PREFIX=$InstallPrefix"

cmake --build $llvmBuild --config $Configuration --target install

$llvmCommit = (git -C (Join-Path $repo "external\llvm-project") rev-parse HEAD).Trim()
New-Item -ItemType Directory -Force -Path $InstallPrefix | Out-Null
$manifest = Join-Path $InstallPrefix "rsma-llvm-manifest.txt"
@(
    "llvmCommit=$llvmCommit"
    "configuration=$Configuration"
    "generator=$Generator"
    "installPrefix=$InstallPrefix"
) | Set-Content -LiteralPath $manifest -Encoding utf8

Write-Host "LLVM/Clang installed to $InstallPrefix"
Write-Host "Manifest: $manifest"
