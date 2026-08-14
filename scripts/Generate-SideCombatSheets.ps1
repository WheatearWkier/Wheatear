# Generates .wtsheet assets for the SideCombat runtime character sheets so
# the sandbox demo uses the reusable sheet pipeline (hot-reloadable grid,
# per-cell trim, PPU sizing, collider follow) instead of baked UV slicing.
# One-time migration; run from the repository root.

$tuningPath = "WheatearEditor/assets/vertical_slice/data/side_combat_tuning.yaml"
$outRoot = "WheatearEditor/assets/vertical_slice/side_combat/sheets"

$lines = Get-Content $tuningPath -Encoding UTF8
$sheets = @{}   # sheetPath -> @{ cellWidth; cellHeight; columns; owner }

foreach ($line in $lines) {
    if ($line -match 'sheet:\s*"([^"]+)"') {
        $sheet = $Matches[1]
        $cellW = 0; $cellH = 0; $cols = 0
        if ($line -match 'cellWidth:\s*(\d+)') { $cellW = [int]$Matches[1] }
        if ($line -match 'cellHeight:\s*(\d+)') { $cellH = [int]$Matches[1] }
        if ($line -match 'columns:\s*(\d+)') { $cols = [int]$Matches[1] }
        if ($cellW -le 0 -or $cellH -le 0 -or $cols -le 0) { continue }
        if (-not $sheets.ContainsKey($sheet)) {
            $sheets[$sheet] = @{ CellWidth = $cellW; CellHeight = $cellH; Columns = $cols }
        }
    }
}

$generated = 0
foreach ($sheet in $sheets.Keys) {
    $meta = $sheets[$sheet]
    # "assets/..." in tuning files is relative to the editor asset root.
    $fullPath = [System.IO.Path]::GetFullPath((Join-Path "WheatearEditor" $sheet))
    if (-not (Test-Path $fullPath)) {
        Write-Host "SKIP (missing texture): $sheet"
        continue
    }
    $bytes = [System.IO.File]::ReadAllBytes($fullPath)
    $width  = [System.Net.IPAddress]::NetworkToHostOrder([BitConverter]::ToInt32($bytes, 16))
    $height = [System.Net.IPAddress]::NetworkToHostOrder([BitConverter]::ToInt32($bytes, 20))
    $rows = [math]::Max(1, [math]::Ceiling($height / $meta.CellHeight))

    $outPath = [System.IO.Path]::ChangeExtension($fullPath, ".wtsheet")
    $yaml = "texture: $sheet`ncolumns: $($meta.Columns)`nrows: $rows`n"
    [System.IO.File]::WriteAllText($outPath, $yaml, [System.Text.Encoding]::UTF8)
    Write-Host "OK: $($sheet -replace '^.*/', '')  ->  ${($meta.Columns)}x${rows}  (${width}x${height}, cell ${($meta.CellWidth)}x${($meta.CellHeight)})"
    $generated++
}

Write-Host ""
Write-Host "Generated $generated .wtsheet asset(s) under $outRoot"
