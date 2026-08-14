# Migrates the SideCombat HUD atlas (sidecombat_ui_sheet.png, irregular UV
# sub-rects) to the reusable .wtsheet named-rect pipeline:
#   1. Reads every UIImageComponent that references the atlas in
#      SideCombatBeastPath.wt, derives its pixel rect from UV + texture size.
#   2. Writes sidecombat_ui_sheet.wtsheet with a `rects:` list (named rects,
#      entity Tag used as the rect name).
#   3. Rewrites the scene: each affected UIImageComponent gains
#      SpriteSheet + SubRect keys (baked UVs stay as fallback).
# One-time migration; run from the repository root.

$scenePath = "WheatearEditor/assets/scenes/SideCombatBeastPath.wt"
$sheetPng  = "assets/vertical_slice/side_combat/ui/sidecombat_ui_sheet.png"
$sheetOut  = "WheatearEditor/assets/vertical_slice/side_combat/ui/sidecombat_ui_sheet.wtsheet"

$pngBytes = [System.IO.File]::ReadAllBytes((Join-Path "WheatearEditor" $sheetPng))
$texW = [System.Net.IPAddress]::NetworkToHostOrder([BitConverter]::ToInt32($pngBytes, 16))
$texH = [System.Net.IPAddress]::NetworkToHostOrder([BitConverter]::ToInt32($pngBytes, 20))
Write-Host "Atlas: $texW x $texH"

$text = [System.IO.File]::ReadAllText($scenePath, [System.Text.Encoding]::UTF8)

# Split into entity blocks on "- Entity:" boundaries (CRLF-tolerant).
$blocks = [regex]::Split($text, '(?=^  - Entity: \d+\r?$)', [System.Text.RegularExpressions.RegexOptions]::Multiline)
$rects = @{}          # name -> rect entry
$nameCount = @{}      # name -> how many distinct rects used it
$byRect = @{}         # "l,t,w,h" -> name
$output = [System.Text.StringBuilder]::new()
$matched = 0

foreach ($block in $blocks) {
    if ($block -notmatch 'TexturePath: ' + [regex]::Escape($sheetPng)) {
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

    # Insert SpriteSheet + SubRect after the TexturePath line (same indent).
    $block = [regex]::Replace($block, '(?m)^(\s*)' + [regex]::Escape('TexturePath: ') + [regex]::Escape($sheetPng) + '\r?$',
        "`$1TexturePath: $sheetPng`n`$1SpriteSheet: assets/vertical_slice/side_combat/ui/sidecombat_ui_sheet.wtsheet`n`$1SubRect: $name")
    [void]$output.Append($block)
    $matched++
}

$result = $output.ToString()
[System.IO.File]::WriteAllText($scenePath, $result, [System.Text.Encoding]::UTF8)
Write-Host "Updated $matched UIImage block(s) in $scenePath"

# Write the .wtsheet asset.
$sheetYaml = "texture: $sheetPng`ncolumns: 1`nrows: 1`nrects:`n" + (($rects.Values | Sort-Object) -join "`n") + "`n"
[System.IO.File]::WriteAllText($sheetOut, $sheetYaml, [System.Text.Encoding]::UTF8)
Write-Host "Wrote $($rects.Count) named rect(s) -> $sheetOut"
