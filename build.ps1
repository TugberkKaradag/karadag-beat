param(
    [switch]$SkipTests,
    [switch]$Configure,
    [switch]$Validate,
    [switch]$Installer
)

$ErrorActionPreference = "Stop"
$root = $PSScriptRoot

$cmake = "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if (-not (Test-Path $cmake)) {
    $found = Get-Command cmake -ErrorAction SilentlyContinue
    if ($null -eq $found) { throw "CMake not found." }
    $cmake = $found.Source
}

if ($Configure -or -not (Test-Path "$root\build\CMakeCache.txt")) {
    Write-Host "`n== Configuring ==" -ForegroundColor Cyan
    & $cmake -S $root -B "$root\build" -A x64
    if ($LASTEXITCODE -ne 0) { throw "CMake configure failed." }
}

if (-not $SkipTests) {
    Write-Host "`n== Building DSP tests ==" -ForegroundColor Cyan
    & $cmake --build "$root\build" --config Release --target DspTest
    if ($LASTEXITCODE -ne 0) { throw "Test build failed." }

    Write-Host "`n== Running DSP tests ==" -ForegroundColor Cyan
    & "$root\build\DspTest_artefacts\Release\DspTest.exe"
    if ($LASTEXITCODE -ne 0) { throw "DSP tests failed - not installing." }

    Write-Host "`n== Building state tests ==" -ForegroundColor Cyan
    & $cmake --build "$root\build" --config Release --target StateTest
    if ($LASTEXITCODE -ne 0) { throw "State test build failed." }

    Write-Host "`n== Running state tests ==" -ForegroundColor Cyan
    & "$root\build\StateTest_artefacts\Release\StateTest.exe"
    if ($LASTEXITCODE -ne 0) { throw "State tests failed - not installing." }
}

Write-Host "`n== Building VST3 ==" -ForegroundColor Cyan
& $cmake --build "$root\build" --config Release --target KaradagBeat_VST3
if ($LASTEXITCODE -ne 0) { throw "VST3 build failed." }

Write-Host "`n== Building standalone app ==" -ForegroundColor Cyan
& $cmake --build "$root\build" --config Release --target KaradagBeat_Standalone
if ($LASTEXITCODE -ne 0) { throw "Standalone build failed." }

$built = "$root\build\KaradagBeat_artefacts\Release\VST3\Karadag Beat.vst3"
if (-not (Test-Path $built)) { throw "Built plugin not found: $built" }

if ($Validate) {
    $pluginval = "$root\tools\pluginval\pluginval.exe"
    if (-not (Test-Path $pluginval)) {
        throw "pluginval not found: $pluginval  (https://github.com/Tracktion/pluginval/releases)"
    }

    Write-Host "`n== pluginval (strictness 10) ==" -ForegroundColor Cyan

    $log = "$root\build\pluginval.txt"
    $p = Start-Process -FilePath $pluginval -Wait -PassThru -NoNewWindow -RedirectStandardOutput $log `
           -ArgumentList @("--strictness-level", "10", "--validate-in-process", "--timeout-ms", "600000", "`"$built`"")
    Get-Content $log | Select-String -Pattern "Starting tests|FAILED|ERROR|SUCCESS|!!!"
    if ($p.ExitCode -ne 0) { throw "pluginval failed (exit code $($p.ExitCode)) - not installing. Details: $log" }
}

if ($Installer) {
    $iscc = @("${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
              "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe") |
            Where-Object { Test-Path $_ } | Select-Object -First 1
    if ($null -eq $iscc) { throw "Inno Setup 6 not found (https://jrsoftware.org/isinfo.php)" }

    $version = (Select-String -Path "$root\CMakeLists.txt" -Pattern 'project\(KaradagBeat VERSION ([0-9.]+)').Matches[0].Groups[1].Value

    Write-Host "`n== Installer ($version) ==" -ForegroundColor Cyan
    & $iscc "/DAppVersion=$version" "$root\installer\KaradagBeat.iss"
    if ($LASTEXITCODE -ne 0) { throw "Inno Setup failed." }
}

$systemDir = "C:\Program Files\Common Files\VST3"
$dest = Join-Path $systemDir "Karadag Beat.vst3"

$canWrite = $false
try {
    $probe = Join-Path $systemDir ".karadagbeat_write_test"
    New-Item -ItemType File -Path $probe -ErrorAction Stop | Out-Null
    Remove-Item $probe -Force
    $canWrite = $true
} catch { }

if ($canWrite) {
    if (Test-Path -LiteralPath $dest) { Remove-Item -LiteralPath $dest -Recurse -Force }
    Copy-Item -LiteralPath $built -Destination $systemDir -Recurse -Force
}
else {
    Write-Host "`n== Asking for admin rights to install ==" -ForegroundColor Yellow
    Write-Host "  Approve the UAC prompt." -ForegroundColor Yellow

    $inner = "if (Test-Path -LiteralPath '$dest') { Remove-Item -LiteralPath '$dest' -Recurse -Force }; " +
             "Copy-Item -LiteralPath '$built' -Destination '$systemDir' -Recurse -Force"

    try {
        Start-Process powershell -Verb RunAs `
            -ArgumentList "-NoProfile","-ExecutionPolicy","Bypass","-Command",$inner -Wait | Out-Null
    } catch {
        throw "Admin rights not granted, plugin not installed."
    }
}

if (-not (Test-Path -LiteralPath "$dest\Contents\x86_64-win\Karadag Beat.vst3")) {
    throw "Copy failed: $dest"
}

Write-Host "`n== Installed ==" -ForegroundColor Green
Write-Host "  $dest"
Write-Host "`n  If your DAW is open, rescan plugins to pick up the new build." -ForegroundColor Cyan
