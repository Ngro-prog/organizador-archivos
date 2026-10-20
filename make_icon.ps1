Add-Type -AssemblyName System.Drawing

function New-RRect([double]$x, [double]$y, [double]$w, [double]$h, [double]$r) {
  $p = New-Object System.Drawing.Drawing2D.GraphicsPath
  $d = $r * 2
  $p.AddArc($x, $y, $d, $d, 180, 90)
  $p.AddArc($x + $w - $d, $y, $d, $d, 270, 90)
  $p.AddArc($x + $w - $d, $y + $h - $d, $d, $d, 0, 90)
  $p.AddArc($x, $y + $h - $d, $d, $d, 90, 90)
  $p.CloseFigure()
  return $p
}

$master = New-Object System.Drawing.Bitmap 256, 256
$g = [System.Drawing.Graphics]::FromImage($master)
$g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$g.Clear([System.Drawing.Color]::Transparent)

$dark  = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 30, 110, 190))
$light = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 66, 158, 255))
$white = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::White)

$g.FillPath($dark, (New-RRect 24 54 92 58 12))
$g.FillPath($dark, (New-RRect 24 76 208 138 16))
$g.FillPath($light, (New-RRect 24 96 208 118 14))

$g.FillRectangle($white, 116, 104, 24, 52)
$tri = New-Object System.Drawing.Drawing2D.GraphicsPath
$tri.AddPolygon(@(
  (New-Object System.Drawing.Point 98, 150),
  (New-Object System.Drawing.Point 158, 150),
  (New-Object System.Drawing.Point 128, 196)
))
$g.FillPath($white, $tri)
$g.Dispose()

$sizes = 16, 24, 32, 48, 64, 128, 256
$pngs = @()
foreach ($sz in $sizes) {
  $b = New-Object System.Drawing.Bitmap $sz, $sz
  $gg = [System.Drawing.Graphics]::FromImage($b)
  $gg.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
  $gg.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
  $gg.DrawImage($master, 0, 0, $sz, $sz)
  $gg.Dispose()
  $ms = New-Object System.IO.MemoryStream
  $b.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
  $pngs += , $ms.ToArray()
  $b.Dispose()
}

$out = New-Object System.IO.MemoryStream
$bw = New-Object System.IO.BinaryWriter $out
$bw.Write([UInt16]0)          # reserved
$bw.Write([UInt16]1)          # type = icon
$bw.Write([UInt16]$sizes.Count)
$offset = 6 + 16 * $sizes.Count
for ($i = 0; $i -lt $sizes.Count; $i++) {
  $sz = $sizes[$i]
  $bytes = $pngs[$i]
  $b = if ($sz -ge 256) { 0 } else { $sz }
  $bw.Write([Byte]$b)         # width
  $bw.Write([Byte]$b)         # height
  $bw.Write([Byte]0)          # colors
  $bw.Write([Byte]0)          # reserved
  $bw.Write([UInt16]1)        # planes
  $bw.Write([UInt16]32)       # bitcount
  $bw.Write([UInt32]$bytes.Length)
  $bw.Write([UInt32]$offset)
  $offset += $bytes.Length
}
foreach ($bytes in $pngs) { $bw.Write($bytes) }
$bw.Flush()
$raiz = if ($PSScriptRoot) { $PSScriptRoot } else { (Get-Location).Path }
$destino = Join-Path $raiz 'organizador.ico'
[System.IO.File]::WriteAllBytes($destino, $out.ToArray())
Write-Output ("ICO generado: {0} ({1} bytes)" -f $destino, $out.Length)
