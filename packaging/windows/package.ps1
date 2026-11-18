param(
    [Parameter(Mandatory)] [string] $Payload,
    [Parameter(Mandatory)] [string] $Output
)

$ErrorActionPreference = 'Stop'
$Payload = (Resolve-Path -LiteralPath $Payload).Path
$Output = (New-Item -ItemType Directory -Force -Path $Output).FullName
$version = (Get-Content -Raw -LiteralPath (Join-Path $Payload 'version.txt')).Trim()
$compiler = Get-Command ISCC.exe -ErrorAction SilentlyContinue
$iscc = if ($compiler) { $compiler.Source } else { "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe" }

& $iscc "/DPayloadDir=$Payload" "/DPackageOut=$Output" "/DPluginVersion=$version" (Join-Path $PSScriptRoot 'FreeqyDelay.iss')
if ($LASTEXITCODE -ne 0) {
    throw "ISCC failed with exit code $LASTEXITCODE"
}
