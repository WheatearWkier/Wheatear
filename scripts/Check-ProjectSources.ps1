param(
    [string]$WorkspaceRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
)

$ErrorActionPreference = "Stop"

function Normalize-ProjectPath([string]$Path) {
    return ($Path -replace "\\", "/").ToLowerInvariant()
}

function Get-RelativeProjectPath([string]$Root, [string]$Path) {
    $rootFull = [System.IO.Path]::GetFullPath($Root)
    $pathFull = [System.IO.Path]::GetFullPath($Path)
    if (!$rootFull.EndsWith([System.IO.Path]::DirectorySeparatorChar)) {
        $rootFull += [System.IO.Path]::DirectorySeparatorChar
    }

    if ($pathFull.StartsWith($rootFull, [System.StringComparison]::OrdinalIgnoreCase)) {
        return $pathFull.Substring($rootFull.Length)
    }

    return $pathFull
}

function Get-ProjectEntries([string]$ProjectFile) {
    [xml]$xml = Get-Content -LiteralPath $ProjectFile
    $entries = New-Object 'System.Collections.Generic.HashSet[string]'
    foreach ($node in $xml.Project.ItemGroup.ClCompile + $xml.Project.ItemGroup.ClInclude) {
        if ($null -eq $node.Include) { continue }
        $include = Normalize-ProjectPath $node.Include
        if ($include.StartsWith("src/") -and ($include.EndsWith(".cpp") -or $include.EndsWith(".h") -or $include.EndsWith(".hpp") -or $include.EndsWith(".inl"))) {
            [void]$entries.Add($include)
        }
    }
    return $entries
}

function Get-SourceFiles([string]$ProjectRoot, [string]$SourceRoot) {
    $files = New-Object 'System.Collections.Generic.HashSet[string]'
    if (!(Test-Path -LiteralPath $SourceRoot)) { return $files }

    Get-ChildItem -LiteralPath $SourceRoot -Recurse -File -Include *.cpp,*.h,*.hpp,*.inl | ForEach-Object {
        $relative = Get-RelativeProjectPath $ProjectRoot $_.FullName
        [void]$files.Add((Normalize-ProjectPath $relative))
    }
    return $files
}

$projects = @(
    @{ Name = "Wheatear"; Root = "Wheatear"; Project = "Wheatear/Wheatear.vcxproj"; Source = "Wheatear/src" },
    @{ Name = "WheatearEditor"; Root = "WheatearEditor"; Project = "WheatearEditor/WheatearEditor.vcxproj"; Source = "WheatearEditor/src" },
    @{ Name = "WheatearSandbox"; Root = "WheatearSandbox"; Project = "WheatearSandbox/WheatearSandbox.vcxproj"; Source = "WheatearSandbox/src" }
)

$issues = @()
foreach ($project in $projects) {
    $projectRoot = Join-Path $WorkspaceRoot $project.Root
    $projectFile = Join-Path $WorkspaceRoot $project.Project
    $sourceRoot = Join-Path $WorkspaceRoot $project.Source
    $sourceFiles = Get-SourceFiles $projectRoot $sourceRoot
    $projectEntries = Get-ProjectEntries $projectFile

    foreach ($file in $sourceFiles) {
        if (!$projectEntries.Contains($file)) {
            $issues += "$($project.Name): source file is missing from vcxproj: $file"
        }
    }

    foreach ($file in $projectEntries) {
        if (!$sourceFiles.Contains($file)) {
            $issues += "$($project.Name): vcxproj entry has no file on disk: $file"
        }
    }
}

if ($issues.Count -gt 0) {
    $issues | ForEach-Object { Write-Host "ERROR: $_" }
    exit 1
}

Write-Host "Project source sync check passed."
