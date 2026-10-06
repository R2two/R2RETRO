param(
    [string]$Source = (Join-Path $PSScriptRoot '../assets/logo.png'),
    [string]$Destination = (Join-Path $PSScriptRoot '../pkg/icon0.png')
)

# Packaging conversion only: preserve the supplied artwork and its aspect ratio.
# Run on Windows; the generated PNG is checked in for Linux/PS4 builds.
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$sourcePath = (Resolve-Path -LiteralPath $Source).Path
$destinationPath = [System.IO.Path]::GetFullPath($Destination)
if ($sourcePath -eq $destinationPath) { throw 'Keep the original artwork separate from the icon.' }
$original = [System.Drawing.Image]::FromFile($sourcePath)
$bitmap = $null
$graphics = $null
$attributes = $null
try {
    $size = 512
    $scale = [Math]::Min($size / $original.Width, $size / $original.Height)
    $width = [int][Math]::Round($original.Width * $scale)
    $height = [int][Math]::Round($original.Height * $scale)
    $x = [int][Math]::Floor(($size - $width) / 2)
    $y = [int][Math]::Floor(($size - $height) / 2)
    $bitmap = [System.Drawing.Bitmap]::new($size, $size, [System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $graphics.Clear([System.Drawing.Color]::FromArgb(3, 9, 28))
    $graphics.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
    $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $attributes = [System.Drawing.Imaging.ImageAttributes]::new()
    $attributes.SetWrapMode([System.Drawing.Drawing2D.WrapMode]::TileFlipXY)
    $rectangle = [System.Drawing.Rectangle]::new($x, $y, $width, $height)
    $graphics.DrawImage($original, $rectangle, 0, 0, $original.Width, $original.Height,
        [System.Drawing.GraphicsUnit]::Pixel, $attributes)
    [System.IO.Directory]::CreateDirectory([System.IO.Path]::GetDirectoryName($destinationPath)) | Out-Null
    $bitmap.Save($destinationPath, [System.Drawing.Imaging.ImageFormat]::Png)
    Write-Output "Icon: $destinationPath (512 x 512 RGB; artwork $width x $height)"
} finally {
    if ($attributes) { $attributes.Dispose() }
    if ($graphics) { $graphics.Dispose() }
    if ($bitmap) { $bitmap.Dispose() }
    $original.Dispose()
}
