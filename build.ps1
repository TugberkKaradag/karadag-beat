# Karadag Beat - derle, test et, kur
#
# Kullanim:
#   .\build.ps1            derle + testleri calistir + kur
#   .\build.ps1 -SkipTests testleri atla
#   .\build.ps1 -Configure CMake'i bastan yapilandir

param(
    [switch]$SkipTests,
    [switch]$Configure
)

$ErrorActionPreference = "Stop"
$root = $PSScriptRoot

$cmake = "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if (-not (Test-Path $cmake)) {
    $found = Get-Command cmake -ErrorAction SilentlyContinue
    if ($null -eq $found) { throw "CMake bulunamadi." }
    $cmake = $found.Source
}

if ($Configure -or -not (Test-Path "$root\build\CMakeCache.txt")) {
    Write-Host "`n== Yapilandiriliyor ==" -ForegroundColor Cyan
    & $cmake -S $root -B "$root\build" -G "Visual Studio 18 2026" -A x64
    if ($LASTEXITCODE -ne 0) { throw "CMake yapilandirmasi basarisiz." }
}

if (-not $SkipTests) {
    Write-Host "`n== DSP testleri derleniyor ==" -ForegroundColor Cyan
    & $cmake --build "$root\build" --config Release --target DspTest
    if ($LASTEXITCODE -ne 0) { throw "Test derlemesi basarisiz." }

    Write-Host "`n== DSP testleri calisiyor ==" -ForegroundColor Cyan
    & "$root\build\DspTest_artefacts\Release\DspTest.exe"
    if ($LASTEXITCODE -ne 0) { throw "DSP testleri basarisiz - kurulum yapilmadi." }

    Write-Host "`n== Proje durumu testleri derleniyor ==" -ForegroundColor Cyan
    & $cmake --build "$root\build" --config Release --target StateTest
    if ($LASTEXITCODE -ne 0) { throw "Durum testi derlemesi basarisiz." }

    Write-Host "`n== Proje durumu testleri calisiyor ==" -ForegroundColor Cyan
    & "$root\build\StateTest_artefacts\Release\StateTest.exe"
    if ($LASTEXITCODE -ne 0) { throw "Durum testleri basarisiz - kurulum yapilmadi." }
}

Write-Host "`n== VST3 derleniyor ==" -ForegroundColor Cyan
& $cmake --build "$root\build" --config Release --target KaradagBeat_VST3
if ($LASTEXITCODE -ne 0) { throw "VST3 derlemesi basarisiz." }

Write-Host "`n== Bagimsiz uygulama derleniyor ==" -ForegroundColor Cyan
& $cmake --build "$root\build" --config Release --target KaradagBeat_Standalone
if ($LASTEXITCODE -ne 0) { throw "Bagimsiz uygulama derlemesi basarisiz." }

$built = "$root\build\KaradagBeat_artefacts\Release\VST3\Karadag Beat.vst3"
if (-not (Test-Path $built)) { throw "Derlenmis eklenti bulunamadi: $built" }

# FL Studio yalnizca sistem VST3 klasorunu tariyor, oraya kurmak gerekiyor.
# Yazma izni yoksa UAC ile yukseltilmis bir kopyalama baslatiyoruz.
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
    Write-Host "`n== Kurulum icin yonetici izni isteniyor ==" -ForegroundColor Yellow
    Write-Host "  Cikacak UAC penceresini onayla." -ForegroundColor Yellow

    $inner = "if (Test-Path -LiteralPath '$dest') { Remove-Item -LiteralPath '$dest' -Recurse -Force }; " +
             "Copy-Item -LiteralPath '$built' -Destination '$systemDir' -Recurse -Force"

    try {
        Start-Process powershell -Verb RunAs `
            -ArgumentList "-NoProfile","-ExecutionPolicy","Bypass","-Command",$inner -Wait | Out-Null
    } catch {
        throw "Yonetici izni verilmedi, eklenti kurulamadi."
    }
}

if (-not (Test-Path -LiteralPath "$dest\Contents\x86_64-win\Karadag Beat.vst3")) {
    throw "Kopyalama basarisiz: $dest"
}

Write-Host "`n== Kuruldu ==" -ForegroundColor Green
Write-Host "  $dest"
Write-Host "`n  FL Studio aciksa yeni eklentiyi gormesi icin:" -ForegroundColor Cyan
Write-Host "  Options > Manage plugins > Find more plugins" -ForegroundColor Cyan
