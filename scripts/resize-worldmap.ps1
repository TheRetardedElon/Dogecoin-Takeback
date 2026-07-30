$src = 'C:\Users\Jeramiah\Downloads\worldmap-yellow.png'
$dst = 'C:\dogedev\src\qt\res\images\worldmap-yellow.png'
New-Item -ItemType Directory -Force -Path (Split-Path $dst) | Out-Null
Add-Type -AssemblyName System.Drawing
$img = [System.Drawing.Image]::FromFile($src)
$w = 1600
$h = [int]($img.Height * ($w / [double]$img.Width))
$bmp = New-Object System.Drawing.Bitmap $w, $h
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g.DrawImage($img, 0, 0, $w, $h)
$bmp.Save($dst, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose()
$bmp.Dispose()
$img.Dispose()
Get-Item $dst | Format-List FullName, Length
