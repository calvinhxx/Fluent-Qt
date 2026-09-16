[CmdletBinding()]
param(
    [string] $IconPath,
    [string] $OutputDirectory
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

if ([string]::IsNullOrWhiteSpace($IconPath)) {
    $IconPath = Join-Path $PSScriptRoot '..\app\assets\app-icon.png'
}
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $PSScriptRoot '..\app\assets'
}

$resolvedIconPath = (Resolve-Path -LiteralPath $IconPath).Path
$resolvedOutputDirectory = (Resolve-Path -LiteralPath $OutputDirectory).Path

function New-GradientBitmap {
    param(
        [int] $Width,
        [int] $Height,
        [Drawing.Color] $TopColor,
        [Drawing.Color] $BottomColor
    )

    $bitmap = [Drawing.Bitmap]::new(
        $Width,
        $Height,
        [Drawing.Imaging.PixelFormat]::Format24bppRgb)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    $graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $graphics.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $brush = [Drawing.Drawing2D.LinearGradientBrush]::new(
        [Drawing.Rectangle]::new(0, 0, $Width, $Height),
        $TopColor,
        $BottomColor,
        90.0)
    $graphics.FillRectangle($brush, 0, 0, $Width, $Height)
    $brush.Dispose()

    return @($bitmap, $graphics)
}

function Save-Bitmap24 {
    param(
        [Drawing.Bitmap] $Bitmap,
        [string] $Path
    )

    $Bitmap.Save($Path, [Drawing.Imaging.ImageFormat]::Bmp)
}

$icon = [Drawing.Image]::FromFile($resolvedIconPath)
try {
    $welcomeParts = New-GradientBitmap `
        164 314 `
        ([Drawing.ColorTranslator]::FromHtml('#F7FAFF')) `
        ([Drawing.ColorTranslator]::FromHtml('#EEF0FF'))
    $welcome = $welcomeParts[0]
    $welcomeGraphics = $welcomeParts[1]
    try {
        $blueOrb = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(184, 217, 233, 255))
        $violetOrb = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(196, 226, 219, 255))
        $cyanOrb = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(130, 190, 218, 255))
        $borderPen = [Drawing.Pen]::new([Drawing.ColorTranslator]::FromHtml('#D8E2EE'))
        try {
            $welcomeGraphics.FillEllipse($blueOrb, -86, 168, 224, 224)
            $welcomeGraphics.FillEllipse($violetOrb, 48, 214, 166, 166)
            $welcomeGraphics.FillEllipse($cyanOrb, 76, -54, 146, 146)
            $welcomeGraphics.DrawImage($icon, [Drawing.Rectangle]::new(34, 38, 96, 96))
            $welcomeGraphics.DrawLine($borderPen, 163, 0, 163, 314)
        } finally {
            $blueOrb.Dispose()
            $violetOrb.Dispose()
            $cyanOrb.Dispose()
            $borderPen.Dispose()
        }
        Save-Bitmap24 $welcome (Join-Path $resolvedOutputDirectory 'installer-welcome.bmp')
    } finally {
        $welcomeGraphics.Dispose()
        $welcome.Dispose()
    }

    $headerParts = New-GradientBitmap `
        150 57 `
        ([Drawing.ColorTranslator]::FromHtml('#F9FBFF')) `
        ([Drawing.ColorTranslator]::FromHtml('#F1F2FF'))
    $header = $headerParts[0]
    $headerGraphics = $headerParts[1]
    try {
        $blueOrb = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(150, 214, 231, 255))
        $violetOrb = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(145, 218, 211, 255))
        $borderPen = [Drawing.Pen]::new([Drawing.ColorTranslator]::FromHtml('#D8E2EE'))
        try {
            $headerGraphics.FillEllipse($blueOrb, 62, -46, 112, 112)
            $headerGraphics.FillEllipse($violetOrb, 112, 15, 80, 80)
            $headerGraphics.DrawImage($icon, [Drawing.Rectangle]::new(101, 8, 40, 40))
            $headerGraphics.DrawLine($borderPen, 0, 56, 150, 56)
        } finally {
            $blueOrb.Dispose()
            $violetOrb.Dispose()
            $borderPen.Dispose()
        }
        Save-Bitmap24 $header (Join-Path $resolvedOutputDirectory 'installer-header.bmp')
    } finally {
        $headerGraphics.Dispose()
        $header.Dispose()
    }
} finally {
    $icon.Dispose()
}
