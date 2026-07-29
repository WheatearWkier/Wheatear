param(
    [string]$ProjectRoot = (Resolve-Path ".").Path
)

$sceneRelativePaths = @(
    "WheatearEditor/assets/scenes/VisualNovelMainMenu.wt",
    "WheatearEditor/assets/scenes/VisualNovelDemo.wt",
    "WheatearEditor/assets/scenes/VerticalSliceIntro.wt",
    "WheatearEditor/assets/scenes/VisualNovelBattle.wt",
    "WheatearEditor/assets/scenes/VerticalSlicePostFake.wt",
    "WheatearEditor/assets/scenes/SideCombatVerticalSlice.wt",
    "WheatearEditor/assets/scenes/VerticalSliceResult.wt",
    "WheatearEditor/assets/scenes/VerticalSliceHub.wt",
    "WheatearEditor/assets/scenes/VerticalSliceSkillTree.wt",
    "WheatearEditor/assets/scenes/VerticalSliceEquipment.wt",
    "WheatearEditor/assets/scenes/VerticalSliceChapter3Preview.wt",
    "WheatearEditor/assets/scenes/SideCombatBeastPath.wt",
    "WheatearEditor/assets/scenes/VerticalSliceDungeonSelect.wt",
    "WheatearEditor/assets/scenes/VerticalSliceRelationship.wt",
    "WheatearEditor/assets/scenes/VerticalSliceSupport.wt",
    "WheatearEditor/assets/scenes/VerticalSliceSettings.wt",
    "WheatearEditor/assets/scenes/VerticalSliceSaveLoad.wt"
)

function Get-CanvasEntityBlock {
    param(
        [long]$EntityId,
        [string]$CanvasName
    )

    return @(
        "  - Entity: $EntityId",
        "    TagComponent:",
        "      Tag: $CanvasName",
        "    TransformComponent:",
        "      Translation: [0, 0, 0]",
        "      Rotation: [0, 0, 0]",
        "      Scale: [1, 1, 1]",
        "    UICanvasComponent:",
        "      Visible: true",
        "      ReferenceWidth: 1920",
        "      ReferenceHeight: 1080",
        "    UIWidgetComponent:",
        "      Visible: true",
        "      Position: [0, 0]",
        "      Size: [1, 1]",
        "      Rotation: 0",
        "      Anchor: 0",
        "      SortOrder: 0",
        "      ParentTag: `"`""
    )
}

foreach ($relativePath in $sceneRelativePaths) {
    $path = Join-Path $ProjectRoot $relativePath
    if (!(Test-Path -LiteralPath $path)) {
        Write-Warning "Scene not found: $relativePath"
        continue
    }

    $lines = [System.Collections.Generic.List[string]]::new()
    [System.IO.File]::ReadAllLines($path) | ForEach-Object { [void]$lines.Add($_) }

    $hasUIWidget = $false
    foreach ($line in $lines) {
        if ($line -eq "    UIWidgetComponent:") {
            $hasUIWidget = $true
            break
        }
    }
    if (!$hasUIWidget) {
        Write-Host "Skipped no-UI scene: $relativePath"
        continue
    }

    $canvasName = "WT_UI_Canvas"
    $hasCanvas = $false
    for ($i = 0; $i -lt $lines.Count; $i++) {
        if ($lines[$i] -eq "    UICanvasComponent:") {
            $hasCanvas = $true
            break
        }
    }

    if (!$hasCanvas) {
        $maxEntityId = 0L
        foreach ($line in $lines) {
            if ($line -match "^\s*-\s+Entity:\s+([0-9]+)\s*$") {
                $candidate = [Int64]$Matches[1]
                if ($candidate -gt $maxEntityId) {
                    $maxEntityId = $candidate
                }
            }
        }

        $entitiesIndex = $lines.IndexOf("Entities:")
        if ($entitiesIndex -lt 0) {
            Write-Warning "Scene has no Entities root: $relativePath"
            continue
        }

        $canvasBlock = Get-CanvasEntityBlock -EntityId ($maxEntityId + 1) -CanvasName $canvasName
        for ($i = $canvasBlock.Count - 1; $i -ge 0; $i--) {
            $lines.Insert($entitiesIndex + 1, $canvasBlock[$i])
        }
    }

    $currentEntityTag = ""
    $insideWidget = $false
    $widgetIndent = ""
    $widgetHasParent = $false
    $widgetSortOrderIndex = -1
    $insertions = @()

    for ($i = 0; $i -lt $lines.Count; $i++) {
        $line = $lines[$i]

        if ($line -eq "    TagComponent:" -and $i + 1 -lt $lines.Count -and $lines[$i + 1] -match "^\s+Tag:\s*(.+)$") {
            $currentEntityTag = $Matches[1].Trim().Trim('"')
        }

        if ($line -eq "    UIWidgetComponent:") {
            $insideWidget = $true
            $widgetIndent = "      "
            $widgetHasParent = $false
            $widgetSortOrderIndex = -1
            continue
        }

        if ($insideWidget) {
            if (!$line.StartsWith("      ") -and $line.Trim().Length -gt 0) {
                if ($currentEntityTag -ne $canvasName -and !$widgetHasParent -and $widgetSortOrderIndex -ge 0) {
                    $insertions += [pscustomobject]@{ Index = $widgetSortOrderIndex + 1; Line = "${widgetIndent}ParentTag: $canvasName" }
                }
                $insideWidget = $false
            }
            else {
                if ($line.TrimStart().StartsWith("ParentTag:")) {
                    $widgetHasParent = $true
                }
                if ($line.TrimStart().StartsWith("SortOrder:")) {
                    $widgetSortOrderIndex = $i
                }
            }
        }
    }

    if ($insideWidget -and $currentEntityTag -ne $canvasName -and !$widgetHasParent -and $widgetSortOrderIndex -ge 0) {
        $insertions += [pscustomobject]@{ Index = $widgetSortOrderIndex + 1; Line = "${widgetIndent}ParentTag: $canvasName" }
    }

    foreach ($insertion in ($insertions | Sort-Object Index -Descending)) {
        $lines.Insert($insertion.Index, $insertion.Line)
    }

    [System.IO.File]::WriteAllLines($path, $lines, [System.Text.UTF8Encoding]::new($false))
    Write-Host "Updated $relativePath"
}
