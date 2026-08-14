# Migrates the VN UI atlas (vn_ui_atlas.png) to the reusable .wtsheet
# named-rect pipeline, same as Migrate-SideCombatHudSheet.ps1:
#   1. Scans the VN scenes for every UIImageComponent referencing the atlas,
#      derives its pixel rect from UV + texture size.
#   2. Writes vn_ui_atlas.wtsheet with a `rects:` list (entity Tag as name;
#      identical rects across scenes/entities merge into one entry).
#   3. Rewrites each scene: affected UIImageComponent gains
#      SpriteSheet + SubRect keys (baked UVs stay as fallback).
# One-time migration; run from the repository root.

$scenes = @(
    "WheatearEditor/assets/scenes/VerticalSliceIntro.wt",
    "WheatearEditor/assets/scenes/VerticalSlicePostFake.wt",
    "WheatearEditor/assets/scenes/VerticalSliceChapter3Preview.wt"
)
$atlasPng = "assets/vertical_slice/ui/atlases/vn_ui_atlas.png"
$sheetOut = "WheatearEditor/assets/vertical_slice/ui/atlases/vn_ui_atlas.wtsheet"

$pngBytes = [System.IO.File]::ReadAllBytes((Join-Path "WheatearEditor" $atlasPng))
$texW = [System.Net.IPAddress]::NetworkToHostOrder([BitConverter]::ToInt32($pngBytes, 16))
$texH = [System.Net.IPAddress]::NetworkToHostOrder([BitConverter]::ToInt32($pngBytes, 20))
Write-Host "Atlas: $texW x $texH"

$rects = @{}
$nameCount = @{}
$byRect = @{}
$totalMatched = 0

foreach ($scenePath in $scenes) {
    $text = [System.IO.File]::ReadAllText($scenePath, [System.Text.Encoding]::UTF8)
    $blocks = [regex]::Split($text, '(?=^  - Entity: \d+\r?$)', [System.Text.RegularExpressions.RegexOptions]::Multiline)
    $output = [System.Text.StringBuilder]::new()
    $matched = 0

    foreach ($block in $blocks) {
        if ($block -notmatch 'TexturePath: ' + [regex]::Escape($atlasPng)) {
            [void]$output.Append($block)
            continue
        }

        $tag = ""
        if ($block -match '(?m)^\s{6}Tag: (\S+)') { $tag = $Matches[1] }
        $uvMin = $null; $uvMax = $null
        if ($block -match 'UVMin: \[([0-9.eE+-]+), ([0-9.eE+-]+)\]') { $uvMin = @([double]$Matches[1], [double]$Matches[2]) }
        if ($block -match 'UVMax: \[([0-9.eE+-]+), ([0-9.eE+-]+)\]') { $uvMax = @([double]$Matches[1], [double]$Matches[2]) }
        if ($null -eq $uvMin -or $null -eq $uvMax) {
            [void]$output.Append($block)
            continue
        }

        # GL UV (v=0 bottom) -> texture pixels (top-left origin).
        $left = [int][math]::Round($uvMin[0] * $texW)
        $top  = [int][math]::Round((1.0 - $uvMax[1]) * $texH)
        $w    = [int][math]::Round(($uvMax[0] - $uvMin[0]) * $texW)
        $h    = [int][math]::Round(($uvMax[1] - $uvMin[1]) * $texH)
        $left = [math]::Max(0, [math]::Min($left, $texW - 1))
        $top  = [math]::Max(0, [math]::Min($top, $texH - 1))
        $w    = [math]::Max(1, [math]::Min($w, $texW - $left))
        $h    = [math]::Max(1, [math]::Min($h, $texH - $top))

        $rectKey = "$left,$top,$w,$h"
        $name = ""
        if ($byRect.ContainsKey($rectKey)) {
            $name = $byRect[$rectKey]
        } else {
            $base = if ($tag) { $tag } else { "rect_$($rects.Count)" }
            if (-not $nameCount.ContainsKey($base)) { $nameCount[$base] = 0 }
            $nameCount[$base]++
            $name = if ($nameCount[$base] -gt 1) { "$base`_$($nameCount[$base])" } else { $base }
            $rects[$name] = "      - { name: '$name', left: $left, top: $top, width: $w, height: $h }"
            $byRect[$rectKey] = $name
        }

        $block = [regex]::Replace($block, '(?m)^(\s*)' + [regex]::Escape('TexturePath: ') + [regex]::Escape($atlasPng) + '\r?$',
            "`$1TexturePath: $atlasPng`n`$1SpriteSheet: assets/vertical_slice/ui/atlases/vn_ui_atlas.wtsheet`n`$1SubRect: $name")
        [void]$output.Append($block)
        $matched++
    }

    [System.IO.File]::WriteAllText($scenePath, $output.ToString(), [System.Text.Encoding]::UTF8)
    Write-Host "Updated $matched block(s) in $scenePath"
    $totalMatched += $matched
}

$sheetYaml = "texture: $atlasPng`ncolumns: 1`nrows: 1`nrects:`n" + (($rects.Values | Sort-Object) -join "`n") + "`n"
[System.IO.File]::WriteAllText($sheetOut, $sheetYaml, [System.Text.Encoding]::UTF8)
Write-Host "Total $totalMatched block(s), $($rects.Count) named rect(s) -> $sheetOut"
