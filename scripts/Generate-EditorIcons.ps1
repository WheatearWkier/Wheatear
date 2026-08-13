# Generate editor toolbar/menu icons from Lucide (via the Iconify API).
#
# Usage:
#   powershell -File scripts/Generate-EditorIcons.ps1
#
# Icons are fetched as SVG (https://api.iconify.design/lucide/<name>.svg),
# rasterized at 96x96 with a distance-field stroke renderer embedded below
# (no external tools required), and written to WheatearEditor/Resources/Icons/Editor/.
#
# Icon source: Lucide icon library (ISC license), see
# WheatearEditor/Resources/Icons/Editor/Source/LICENSE.

param(
    [string]$OutputDir = "WheatearEditor/Resources/Icons/Editor",
    [string]$Only = ""   # regenerate a single icon by manifest name (debug)
)

$ErrorActionPreference = "Stop"
$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$repositoryRoot = (Resolve-Path (Join-Path $scriptRoot "..")).Path
$outputDir = Join-Path $repositoryRoot $OutputDir

# --- Embedded SVG -> PNG rasterizer (distance-field stroke renderer) ---------
Add-Type -ReferencedAssemblies "System.Drawing" -TypeDefinition @"
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Imaging;
using System.Text.RegularExpressions;

public static class SvgToPng
{
    struct V { public double X, Y; public V(double x, double y) { X = x; Y = y; } }
    struct Seg { public V A, B; public Seg(V a, V b) { A = a; B = b; } }

    static List<Seg> s_Segs = new List<Seg>();
    static double s_StrokeW = 2.0;
    static List<string> s_T = new List<string>();
    static int s_I = 0;

    static double Num()
    {
        return double.Parse(s_T[s_I++], System.Globalization.CultureInfo.InvariantCulture);
    }

    static void LineTo(List<V> pts, V p) { pts.Add(p); }
    static void CubicTo(List<V> pts, V p0, V p1, V p2, V p3)
    {
        const int N = 12;
        for (int i = 1; i <= N; i++)
        {
            double t = i / (double)N, u = 1.0 - t;
            double x = u*u*u*p0.X + 3*u*u*t*p1.X + 3*u*t*t*p2.X + t*t*t*p3.X;
            double y = u*u*u*p0.Y + 3*u*u*t*p1.Y + 3*u*t*t*p2.Y + t*t*t*p3.Y;
            pts.Add(new V(x, y));
        }
    }
    static void QuadTo(List<V> pts, V p0, V p1, V p2)
    {
        const int N = 10;
        for (int i = 1; i <= N; i++)
        {
            double t = i / (double)N, u = 1.0 - t;
            double x = u*u*p0.X + 2*u*t*p1.X + t*t*p2.X;
            double y = u*u*p0.Y + 2*u*t*p1.Y + t*t*p2.Y;
            pts.Add(new V(x, y));
        }
    }
    // SVG elliptical arc -> polyline (endpoint to center parameterization).
    static void ArcTo(List<V> pts, V p0, double rx, double ry, double rot, bool large, bool sweep, V p1)
    {
        rx = Math.Abs(rx); ry = Math.Abs(ry);
        if (rx < 1e-9 || ry < 1e-9) { pts.Add(p1); return; }
        double dx = (p0.X - p1.X) / 2, dy = (p0.Y - p1.Y) / 2;
        double phi = rot * Math.PI / 180.0, cp = Math.Cos(phi), sp = Math.Sin(phi);
        double x1p = cp * dx + sp * dy, y1p = -sp * dx + cp * dy;
        double lam = (x1p*x1p)/(rx*rx) + (y1p*y1p)/(ry*ry);
        if (lam > 1) { double s = Math.Sqrt(lam); rx *= s; ry *= s; }
        double num = rx*rx*ry*ry - rx*rx*y1p*y1p - ry*ry*x1p*x1p;
        double den = rx*rx*y1p*y1p + ry*ry*x1p*x1p;
        double rad = (den <= 1e-9) ? 0 : Math.Sqrt(Math.Max(0, num / den));
        double sgn = (large == sweep) ? -1.0 : 1.0;
        double cxp = sgn * rad * rx * y1p / ry, cyp = sgn * rad * -ry * x1p / rx;
        double cx = cp * cxp - sp * cyp + (p0.X + p1.X) / 2;
        double cy = sp * cxp + cp * cyp + (p0.Y + p1.Y) / 2;
        V u = new V((x1p - cxp) / rx, (y1p - cyp) / ry);
        V v = new V((-x1p - cxp) / rx, (-y1p - cyp) / ry);
        double a1 = Angle(u, new V(1, 0));
        double da = Angle(v, u);
        if (!sweep && da > 0) da -= 2 * Math.PI;
        if (sweep && da < 0) da += 2 * Math.PI;
        int n = Math.Max(4, (int)(Math.Abs(da) / (Math.PI / 24)) + 1);
        for (int i = 1; i <= n; i++)
        {
            double t = a1 + da * i / n;
            double x = cx + rx * Math.Cos(t) * cp - ry * Math.Sin(t) * sp;
            double y = cy + rx * Math.Cos(t) * sp + ry * Math.Sin(t) * cp;
            pts.Add(new V(x, y));
        }
    }

    static double Angle(V u, V v)
    {
        double a = Math.Atan2(u.Y, u.X) - Math.Atan2(v.Y, v.X);
        if (a > Math.PI) a -= 2 * Math.PI;
        if (a < -Math.PI) a += 2 * Math.PI;
        return a;
    }

    static void CloseSub(List<V> pts, ref double cx, ref double cy, ref double sx, ref double sy, ref bool started)
    {
        if (pts.Count >= 2)
        {
            for (int k = 0; k + 1 < pts.Count; k++) s_Segs.Add(new Seg(pts[k], pts[k + 1]));
            s_Segs.Add(new Seg(pts[pts.Count - 1], pts[0]));
        }
        pts.Clear(); cx = sx; cy = sy; started = false;
    }

    // Open paths (no Z): flush the collected points as segments without
    // closing the loop back to the start point.
    static void FlushOpen(List<V> pts, ref bool started)
    {
        if (pts.Count >= 2)
            for (int k = 0; k + 1 < pts.Count; k++) s_Segs.Add(new Seg(pts[k], pts[k + 1]));
        pts.Clear(); started = false;
    }

    static void AddPolyline(double[] coords, bool close)
    {
        if (coords.Length < 4) return;
        int n = coords.Length / 2;
        var p = new List<V>();
        for (int i = 0; i < n; i++) p.Add(new V(coords[i * 2], coords[i * 2 + 1]));
        for (int k = 0; k + 1 < p.Count; k++) s_Segs.Add(new Seg(p[k], p[k + 1]));
        if (close && p.Count >= 3) s_Segs.Add(new Seg(p[p.Count - 1], p[0]));
    }

    static void ParsePath(string d)
    {
        var tokens = Regex.Matches(d, "[A-Za-z]|-?\\d*\\.?\\d+(?:[eE][-+]?\\d+)?");
        s_T = new List<string>();
        foreach (Match m in tokens) s_T.Add(m.Value);
        s_I = 0; char cmd = 'M'; double cx = 0, cy = 0, sx = 0, sy = 0;
        var pts = new List<V>();
        bool started = false;
        while (s_I < s_T.Count)
        {
            string tk = s_T[s_I];
            if (tk.Length == 1 && char.IsLetter(tk[0]))
            {
                if (cmd == 'Z' || cmd == 'z') CloseSub(pts, ref cx, ref cy, ref sx, ref sy, ref started);
                cmd = tk[0]; s_I++; continue;
            }
            double relX = (cmd >= 'a' && cmd <= 'z') ? cx : 0;
            double relY = (cmd >= 'a' && cmd <= 'z') ? cy : 0;
            V rel = new V(relX, relY);
            switch (char.ToUpperInvariant(cmd))
            {
                case 'M':
                {
                    double x = Num() + rel.X, y = Num() + rel.Y;
                    if (started) { CloseSub(pts, ref cx, ref cy, ref sx, ref sy, ref started); }
                    sx = cx = x; sy = cy = y; pts.Add(new V(x, y)); started = true;
                    if (cmd == 'm') cmd = 'l'; else cmd = 'L';
                    break;
                }
                case 'L':
                {
                    double x = Num() + rel.X, y = Num() + rel.Y;
                    cx = x; cy = y; pts.Add(new V(x, y)); started = true;
                    break;
                }
                case 'H':
                {
                    double x = Num() + rel.X; cx = x; pts.Add(new V(x, cy)); started = true;
                    break;
                }
                case 'V':
                {
                    double y = Num() + rel.Y; cy = y; pts.Add(new V(cx, y)); started = true;
                    break;
                }
                case 'C':
                {
                    var c1 = new V(Num() + rel.X, Num() + rel.Y);
                    var c2 = new V(Num() + rel.X, Num() + rel.Y);
                    var p1 = new V(Num() + rel.X, Num() + rel.Y);
                    CubicTo(pts, new V(cx, cy), c1, c2, p1);
                    cx = p1.X; cy = p1.Y; started = true;
                    break;
                }
                case 'Q':
                {
                    var c1 = new V(Num() + rel.X, Num() + rel.Y);
                    var p1 = new V(Num() + rel.X, Num() + rel.Y);
                    QuadTo(pts, new V(cx, cy), c1, p1);
                    cx = p1.X; cy = p1.Y; started = true;
                    break;
                }
                case 'A':
                {
                    double rx = Num(), ry = Num(), rot = Num();
                    bool la = Num() != 0, sw = Num() != 0;
                    var p1 = new V(Num() + rel.X, Num() + rel.Y);
                    int before = pts.Count;
                    ArcTo(pts, new V(cx, cy), rx, ry, rot, la, sw, p1);
                    if (Environment.GetEnvironmentVariable("ICON_DEBUG") == "1")
                        Console.WriteLine("[debug] arc: p0=({0},{1}) rx={2} ry={3} p1=({4},{5}) -> +{6} pts",
                            cx, cy, rx, ry, p1.X, p1.Y, pts.Count - before);
                    cx = p1.X; cy = p1.Y; started = true;
                    break;
                }
                case 'Z':
                    if (started) CloseSub(pts, ref cx, ref cy, ref sx, ref sy, ref started);
                    s_I++; break;
                default: s_I++; break;
            }
        }
        if (cmd == 'Z' || cmd == 'z') CloseSub(pts, ref cx, ref cy, ref sx, ref sy, ref started);
        else FlushOpen(pts, ref started);
    }

    public static void Convert(string svg, string outPath, int size, string colorHex)
    {
        s_Segs.Clear();
        var m = Regex.Match(svg, "stroke-width=\"([0-9.]+)\"");
        s_StrokeW = m.Success ? double.Parse(m.Groups[1].Value, System.Globalization.CultureInfo.InvariantCulture) : 2.0;
        foreach (Match pm in Regex.Matches(svg, "<path[^>]*d=\"([^\"]+)\""))
            ParsePath(pm.Groups[1].Value.Replace("&#10;", " "));

        // Non-path elements used by a few Lucide icons.
        foreach (Match rm in Regex.Matches(svg, "<rect[^>]*/>"))
        {
            double x = Attr(rm.Value, "x", 0), y = Attr(rm.Value, "y", 0);
            double w = Attr(rm.Value, "width", 0), h = Attr(rm.Value, "height", 0);
            double rx = Attr(rm.Value, "rx", 0);
            var p = new List<V>();
            if (rx > 0)
            {
                p.Add(new V(x + rx, y));
                ArcTo(p, new V(x + rx, y), rx, rx, 0, false, true, new V(x + w, y + rx));
                p.Add(new V(x + w, y + h - rx));
                ArcTo(p, new V(x + w, y + h - rx), rx, rx, 0, false, true, new V(x + w - rx, y + h));
                p.Add(new V(x + rx, y + h));
                ArcTo(p, new V(x + rx, y + h), rx, rx, 0, false, true, new V(x, y + h - rx));
                p.Add(new V(x, y + rx));
                ArcTo(p, new V(x, y + rx), rx, rx, 0, false, true, new V(x + rx, y));
            }
            else
            {
                p.Add(new V(x, y)); p.Add(new V(x + w, y));
                p.Add(new V(x + w, y + h)); p.Add(new V(x, y + h));
            }
            for (int k = 0; k + 1 < p.Count; k++) s_Segs.Add(new Seg(p[k], p[k + 1]));
            if (p.Count >= 3) s_Segs.Add(new Seg(p[p.Count - 1], p[0]));
        }
        foreach (Match cm in Regex.Matches(svg, "<circle[^>]*/>"))
        {
            double cx = Attr(cm.Value, "cx", 0), cy = Attr(cm.Value, "cy", 0);
            double r = Attr(cm.Value, "r", 0);
            var p = new List<V>();
            int n = 32;
            for (int i = 1; i <= n; i++)
            {
                double a = 2 * Math.PI * i / n;
                p.Add(new V(cx + r * Math.Cos(a), cy + r * Math.Sin(a)));
            }
            for (int k = 0; k + 1 < p.Count; k++) s_Segs.Add(new Seg(p[k], p[k + 1]));
            s_Segs.Add(new Seg(p[p.Count - 1], p[0]));
        }
        foreach (Match lm in Regex.Matches(svg, "<line[^>]*/>"))
        {
            double x1 = Attr(lm.Value, "x1", 0), y1 = Attr(lm.Value, "y1", 0);
            double x2 = Attr(lm.Value, "x2", 0), y2 = Attr(lm.Value, "y2", 0);
            s_Segs.Add(new Seg(new V(x1, y1), new V(x2, y2)));
        }
        foreach (Match pm in Regex.Matches(svg, "<(?:polygon|polyline)[^>]*points=\"([^\"]+)\""))
        {
            string pts = pm.Groups[1].Value.Replace(",", " ");
            var nums = Regex.Matches(pts, "-?\\d*\\.?\\d+");
            var coords = new double[nums.Count];
            for (int i = 0; i < nums.Count; i++)
                coords[i] = double.Parse(nums[i].Value, System.Globalization.CultureInfo.InvariantCulture);
            AddPolyline(coords, pm.Value.StartsWith("<polygon"));
        }
        if (Environment.GetEnvironmentVariable("ICON_DEBUG") == "1")
            Console.WriteLine("[debug] segments=" + s_Segs.Count);
        int radiusPx = (int)(s_StrokeW / 2.0 * (size / 24.0) + 0.5);
        if (radiusPx < 1) radiusPx = 1;
        double scale = size / 24.0;
        var color = ColorTranslator.FromHtml(colorHex);
        var bmp = new Bitmap(size, size, PixelFormat.Format32bppArgb);
        for (int y = 0; y < size; y++)
        {
            for (int x = 0; x < size; x++)
            {
                double px = (x + 0.5) / scale, py = (y + 0.5) / scale;
                double dmin = double.MaxValue;
                foreach (var s in s_Segs)
                {
                    double abx = s.B.X - s.A.X, aby = s.B.Y - s.A.Y;
                    double len2 = abx * abx + aby * aby;
                    double t = (len2 <= 1e-12) ? 0 : ((px - s.A.X) * abx + (py - s.A.Y) * aby) / len2;
                    t = Math.Max(0, Math.Min(1, t));
                    double dx = px - (s.A.X + t * abx), dy = py - (s.A.Y + t * aby);
                    double d = Math.Sqrt(dx * dx + dy * dy);
                    if (d < dmin) dmin = d;
                }
                if (dmin <= radiusPx)
                {
                    double a = Math.Min(1.0, radiusPx + 0.5 - dmin);
                    bmp.SetPixel(x, y, Color.FromArgb((int)(a * 255), color.R, color.G, color.B));
                }
            }
        }
        bmp.Save(outPath, ImageFormat.Png);
        bmp.Dispose();
    }

    static double Attr(string tag, string name, double def)
    {
        var m = Regex.Match(tag, name + "=\"([-0-9.]+)\"");
        return m.Success ? double.Parse(m.Groups[1].Value, System.Globalization.CultureInfo.InvariantCulture) : def;
    }
}
"@

# --- Icon manifest: name -> (lucide name, color) -----------------------------
# Functional toolbar icons (light stroke; run-state icons keep their colors).
$icons = @{
    "new_scene"     = @("file-plus",     "#E6EDF3")
    "open_scene"    = @("folder-open",   "#E6EDF3")
    "save_scene"    = @("save",          "#E6EDF3")
    "play"          = @("play",          "#59D5AF")   # mint green (run)
    "pause"         = @("pause",         "#59D5AF")
    "stop"          = @("square",        "#FD8068")   # coral (stop)
    "step"          = @("skip-forward",  "#59D5AF")
    "focus"         = @("focus",         "#E6EDF3")
    "ui_canvas"     = @("monitor-play",  "#E6EDF3")
    "sprite_sheet"  = @("image",         "#E6EDF3")
    "event_graph"   = @("network",       "#E6EDF3")
    "health"        = @("heart-pulse",   "#E6EDF3")
    "package"       = @("package",       "#E6EDF3")
    "reset_layout"  = @("layout-dashboard", "#E6EDF3")
    "check"         = @("badge-check",   "#E6EDF3")
    "gameplay"      = @("gamepad-2",     "#E6EDF3")
    "palette"       = @("palette",       "#E6EDF3")
    "panel"         = @("panel-top",     "#E6EDF3")
    "refresh"       = @("refresh-cw",    "#E6EDF3")
    "search"        = @("search",        "#E6EDF3")
    "settings"      = @("settings",      "#E6EDF3")
    # Menu / panel icons
    "undo"          = @("undo-2",        "#E6EDF3")
    "redo"          = @("redo-2",        "#E6EDF3")
    "chevron_left"  = @("chevron-left",  "#E6EDF3")
    "chevron_right" = @("chevron-right", "#E6EDF3")
    "arrow_up"      = @("arrow-up",      "#E6EDF3")
    "arrow_down"    = @("arrow-down",    "#E6EDF3")
    "close"         = @("x",             "#E6EDF3")
    "plus"          = @("plus",          "#E6EDF3")
    "trash"         = @("trash-2",       "#E6EDF3")
    "pencil"        = @("pencil",        "#E6EDF3")
    "copy"          = @("copy",          "#E6EDF3")
    "eye"           = @("eye",           "#E6EDF3")
    "eye_off"       = @("eye-off",       "#E6EDF3")
    "info"          = @("info",          "#E6EDF3")
    "language"      = @("languages",     "#E6EDF3")
    "logout"        = @("log-out",       "#E6EDF3")
    "bar_chart"     = @("bar-chart-3",   "#E6EDF3")
    "box"           = @("box",           "#E6EDF3")
    "eraser"        = @("eraser",        "#E6EDF3")
    "flask"         = @("flask-conical", "#E6EDF3")
    "wand"          = @("wand-2",        "#E6EDF3")
    "branch"        = @("git-branch",    "#E6EDF3")
    "corner"        = @("corner-down-right", "#E6EDF3")
    "crosshair"     = @("crosshair",     "#E6EDF3")
    "panel_left"    = @("panel-left",    "#E6EDF3")
    "bookmark"      = @("bookmark",      "#E6EDF3")
    "bookmark_x"    = @("bookmark-x",    "#E6EDF3")
    "template"      = @("layout-template", "#E6EDF3")
    "sliders"       = @("sliders-horizontal", "#E6EDF3")
    "wrench"        = @("wrench",        "#E6EDF3")
    "check_square"  = @("check-square",  "#E6EDF3")
    "folder_plus"   = @("folder-plus",   "#E6EDF3")
    "file_plus"     = @("file-plus",     "#E6EDF3")
    "download"      = @("download",      "#E6EDF3")
    "zoom_in"       = @("zoom-in",       "#E6EDF3")
    "zoom_out"      = @("zoom-out",      "#E6EDF3")
    "filter"        = @("filter-x",      "#E6EDF3")
    "file_text"     = @("file-text",     "#E6EDF3")
    "audio"         = @("audio-lines",   "#E6EDF3")
    "script"        = @("scroll-text",   "#E6EDF3")
    "folder"        = @("folder",        "#E6EDF3")
    "save"          = @("save",          "#E6EDF3")
    "film"          = @("film",          "#E6EDF3")
    "code"          = @("code",          "#E6EDF3")
}

New-Item -ItemType Directory -Path $outputDir -Force | Out-Null
$ok = 0; $fail = 0
foreach ($entry in $icons.GetEnumerator() | Sort-Object Key)
{
    $name = $entry.Key
    if ($Only -ne "" -and $name -ne $Only) { continue }
    $lucide = $entry.Value[0]
    $color = $entry.Value[1]
    $svgUrl = "https://api.iconify.design/lucide/$lucide.svg"
    $svgPath = Join-Path $env:TEMP "wheatear_icon_$lucide.svg"
    $pngPath = Join-Path $outputDir "$name.png"
    try
    {
        $svg = ""
        for ($attempt = 0; $attempt -lt 3 -and $svg.Length -lt 50; $attempt++)
        {
            Invoke-WebRequest -Uri $svgUrl -OutFile $svgPath -UseBasicParsing -TimeoutSec 15 | Out-Null
            $svg = Get-Content -Raw $svgPath
        }
        if ($svg.Length -lt 50 -or -not $svg.Contains("<path") -and -not $svg.Contains("<rect") -and -not $svg.Contains("<circle") -and -not $svg.Contains("<polygon") -and -not $svg.Contains("<line"))
        {
            throw "SVG download empty or unexpected: $svgUrl"
        }
        [SvgToPng]::Convert($svg, $pngPath, 96, $color)
        Write-Host ("  ok   {0,-16} <- {1}" -f $name, $lucide)
        $ok++
    }
    catch
    {
        Write-Host ("  FAIL {0,-16} <- {1} : {2}" -f $name, $lucide, $_.Exception.Message)
        $fail++
    }
}
Write-Host ("Generated {0} icons, {1} failures -> {2}" -f $ok, $fail, $outputDir)
