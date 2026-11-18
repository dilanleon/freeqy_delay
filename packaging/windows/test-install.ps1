param(
    [Parameter(Mandatory)] [string] $Installer,
    [Parameter(Mandatory)] [string] $BuildDir
)

$ErrorActionPreference = 'Stop'
$installerPath = (Resolve-Path -LiteralPath $Installer).Path
$vst3 = Join-Path $env:CommonProgramFiles 'VST3\FREEQY DELAY.vst3'
$clap = Join-Path $env:CommonProgramFiles 'CLAP\FREEQY DELAY.clap'
$installDir = Join-Path $env:ProgramFiles 'Station Sound\FREEQY DELAY'
if ((Test-Path -LiteralPath $vst3) -or (Test-Path -LiteralPath $clap) -or (Test-Path -LiteralPath $installDir)) {
    throw 'Run installer tests on a clean Windows machine'
}

function Invoke-TestInstaller {
    param([string] $Components)
    $arguments = @('/SP-', '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART')
    if ($Components) { $arguments += "/COMPONENTS=$Components" }
    $process = Start-Process -FilePath $installerPath -ArgumentList $arguments -Wait -PassThru
    if ($process.ExitCode -ne 0) { throw "Installer failed with exit code $($process.ExitCode)" }
}

function Assert-InstalledFormats {
    param([bool] $Vst3Expected, [bool] $ClapExpected)
    $vst3Installed = Test-Path -LiteralPath (Join-Path $vst3 'Contents\x86_64-win\FREEQY DELAY.vst3')
    $clapInstalled = Test-Path -LiteralPath $clap -PathType Leaf
    if ($vst3Installed -ne $Vst3Expected -or $clapInstalled -ne $ClapExpected) {
        throw "Unexpected installation state. VST3: $vst3Installed, CLAP: $clapInstalled"
    }
}

function Invoke-TestUninstaller {
    $uninstaller = Get-ChildItem -LiteralPath $installDir -Filter 'unins*.exe' | Select-Object -First 1
    if (-not $uninstaller) { throw 'Installer did not register an uninstaller' }
    $process = Start-Process -FilePath $uninstaller.FullName -ArgumentList '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART' -Wait -PassThru
    if ($process.ExitCode -ne 0) { throw "Uninstaller failed with exit code $($process.ExitCode)" }
    Assert-InstalledFormats $false $false
}

Invoke-TestInstaller 'vst3'
Assert-InstalledFormats $true $false
Invoke-TestUninstaller
Invoke-TestInstaller 'clap'
Assert-InstalledFormats $false $true
Invoke-TestUninstaller
Invoke-TestInstaller
Assert-InstalledFormats $true $true

$env:FREEQY_TEST_INSTALLED_VST3 = $vst3
$env:FREEQY_TEST_INSTALLED_CLAP = $clap
ctest --test-dir $BuildDir -C Release --output-on-failure -R '^(vst3_host|clap_host)$'
if ($LASTEXITCODE -ne 0) { throw 'Installed plug-in host checks failed' }

$clapHash = (Get-FileHash -LiteralPath $clap).Hash
[System.IO.File]::AppendAllText($clap, 'Upgrade smoke test')
Invoke-TestInstaller 'vst3'
Assert-InstalledFormats $true $true
if ((Get-FileHash -LiteralPath $clap).Hash -eq $clapHash) { throw 'Deselecting CLAP replaced it' }
Invoke-TestInstaller 'clap'
if ((Get-FileHash -LiteralPath $clap).Hash -ne $clapHash) { throw 'Selecting CLAP did not replace it' }
Invoke-TestUninstaller
