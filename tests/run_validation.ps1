param(
    [string]$NativeBuild = (Join-Path $PSScriptRoot '..\build'),
    [string]$UnityExecutable = $env:UNITY_EXECUTABLE,
    [switch]$SkipUnity
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$unityProject = (Resolve-Path (Join-Path $repoRoot '..\RSMA')).Path

cmake --build $NativeBuild --config Debug --parallel 2
ctest --test-dir $NativeBuild -C Debug --output-on-failure

dotnet build (Join-Path $unityProject 'RSMA.Microcontroller.Virtual.csproj') --no-restore --disable-build-servers
dotnet build (Join-Path $unityProject 'RSMA.Microcontroller.Virtual.Tests.csproj') --no-restore --disable-build-servers
dotnet build (Join-Path $unityProject 'RSMA.Microcontroller.Virtual.PlayMode.Tests.csproj') --no-restore --disable-build-servers

if (-not $SkipUnity) {
    if ([string]::IsNullOrWhiteSpace($UnityExecutable)) {
        throw 'Set UNITY_EXECUTABLE or pass -SkipUnity when running outside a Unity Editor host.'
    }
    & $UnityExecutable -batchmode -nographics -quit -projectPath $unityProject `
        -runTests -testPlatform editmode -testResults (Join-Path $unityProject 'Temp\ci-editmode-results.xml') `
        -logFile (Join-Path $unityProject 'Temp\ci-editmode.log')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $UnityExecutable -batchmode -nographics -quit -projectPath $unityProject `
        -runTests -testPlatform playmode -testResults (Join-Path $unityProject 'Temp\ci-playmode-results.xml') `
        -logFile (Join-Path $unityProject 'Temp\ci-playmode.log')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}