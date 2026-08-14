# Remove dormant C# ScriptComponent blocks from scene/prefab assets.
# The C# scripting runtime (mono, --csharp-scripting) is not used in current
# builds; these component blocks are dead data.
$ErrorActionPreference = "Stop"

$targets = @()
$targets += Get-ChildItem "WheatearEditor/assets/scenes" -Filter "*.wt"
$targets += Get-ChildItem "WheatearEditor/assets" -Recurse -Filter "*.wtprefab"
$targets += Get-ChildItem "WheatearEditor/assets" -Recurse -Filter "*.wtuit"

$removed = 0
foreach ($file in $targets) {
    $content = Get-Content $file.FullName -Encoding UTF8
    $out = New-Object System.Collections.Generic.List[string]
    $skip = $false
    foreach ($line in $content) {
        if ($line -match '^\s*ScriptComponent:\s*$') {
            $skip = $true
            $removed++
            continue
        }
        if ($skip) {
            # End of the component block: a non-empty line indented <= 4 spaces.
            if ($line -match '^\S' -or $line -match '^ {1,4}\S') {
                $skip = $false
            } else {
                continue
            }
        }
        $out.Add($line)
    }
    Set-Content -Path $file.FullName -Value $out -Encoding UTF8
}
Write-Output ("Removed ScriptComponent blocks: " + $removed)
