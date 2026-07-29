param(
    [string]$Configuration = "Debug",
    [string]$Platform = "x64",
    [string]$Verbosity = "m"
)

$ErrorActionPreference = "Stop"

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$repositoryRoot = (Resolve-Path (Join-Path $scriptRoot "..")).Path

& (Join-Path $scriptRoot "Build-Windows.ps1") `
    -ProjectPath "WheatearEditor\WheatearEditor.vcxproj" `
    -Configuration $Configuration `
    -Platform $Platform `
    -Verbosity $Verbosity `
    -LogFile "build-editor-msbuild.log"

$sourceDir = Join-Path $repositoryRoot "bin\$Configuration-windows-x86_64\WheatearEditor"
$targetDir = Join-Path $repositoryRoot "Builds\Windows\Editor"

if (-not (Test-Path -LiteralPath $sourceDir)) {
    throw "Editor build output not found: $sourceDir"
}

New-Item -ItemType Directory -Path $targetDir -Force | Out-Null
Copy-Item -Path (Join-Path $sourceDir "*") -Destination $targetDir -Recurse -Force -Exclude "imgui.ini"

$targetIni = Join-Path $targetDir "imgui.ini"
if (Test-Path -LiteralPath $targetIni) {
    Remove-Item -LiteralPath $targetIni -Force
}

$editorExe = Join-Path $targetDir "WheatearEditor.exe"
if (-not (Test-Path -LiteralPath $editorExe)) {
    throw "Packaged editor executable was not copied: $editorExe"
}

Write-Host "WheatearEditor is ready: $editorExe"
