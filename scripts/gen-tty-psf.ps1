param(
    [string]$OutputPath = "ramdisk/system/tty.psf",
    [int]$Width = 12,
    [int]$Height = 24,
    [int]$GlyphCount = 256,
    [int]$FontSize = 20,
    [int]$OffsetX = 0,
    [int]$OffsetY = 0,
    [int]$AutoCenter = 1
)

Add-Type -AssemblyName System.Drawing

if ($Width -le 0 -or $Height -le 0) {
    throw "Width and Height must be positive"
}

if ($GlyphCount -le 0) {
    throw "GlyphCount must be positive"
}

$fontCandidates = @("Cascadia Mono", "Consolas", "Lucida Console")
$font = $null

foreach ($name in $fontCandidates) {
    try {
        $candidate = New-Object System.Drawing.Font($name, $FontSize, [System.Drawing.FontStyle]::Regular, [System.Drawing.GraphicsUnit]::Pixel)
        if ($candidate.Name -eq $name) {
            $font = $candidate
            break
        }
        $candidate.Dispose()
    } catch {
        $font = $null
    }
}

if ($null -eq $font) {
    throw "No usable monospace font found for PSF generation"
}

function Set-U32LE {
    param(
        [byte[]]$Buffer,
        [int]$Offset,
        [uint32]$Value
    )

    $Buffer[$Offset + 0] = [byte]($Value -band 0xFF)
    $Buffer[$Offset + 1] = [byte](($Value -shr 8) -band 0xFF)
    $Buffer[$Offset + 2] = [byte](($Value -shr 16) -band 0xFF)
    $Buffer[$Offset + 3] = [byte](($Value -shr 24) -band 0xFF)
}

function Pixel-On {
    param([System.Drawing.Color]$Pixel)
    return (($Pixel.R + $Pixel.G + $Pixel.B) -ge 384)
}

function Get-BBox {
    param([System.Drawing.Bitmap]$Bmp)

    $w = $Bmp.Width
    $h = $Bmp.Height
    $minX = $w
    $minY = $h
    $maxX = -1
    $maxY = -1

    for ($y = 0; $y -lt $h; $y++) {
        for ($x = 0; $x -lt $w; $x++) {
            if (Pixel-On ($Bmp.GetPixel($x, $y))) {
                if ($x -lt $minX) { $minX = $x }
                if ($y -lt $minY) { $minY = $y }
                if ($x -gt $maxX) { $maxX = $x }
                if ($y -gt $maxY) { $maxY = $y }
            }
        }
    }

    if ($maxX -lt 0 -or $maxY -lt 0) {
        return @{ Empty = $true; MinX = 0; MinY = 0; MaxX = -1; MaxY = -1 }
    }

    return @{ Empty = $false; MinX = $minX; MinY = $minY; MaxX = $maxX; MaxY = $maxY }
}

$headerSize = 32
$bytesPerRow = (($Width + 7) -shr 3)
$bytesPerGlyph = $bytesPerRow * $Height
$totalSize = $headerSize + ($GlyphCount * $bytesPerGlyph)

$bytes = New-Object byte[] $totalSize

# PSF2 header
Set-U32LE -Buffer $bytes -Offset 0  -Value ([Convert]::ToUInt32("864AB572", 16))
Set-U32LE -Buffer $bytes -Offset 4  -Value 0
Set-U32LE -Buffer $bytes -Offset 8  -Value ([uint32]$headerSize)
Set-U32LE -Buffer $bytes -Offset 12 -Value 0
Set-U32LE -Buffer $bytes -Offset 16 -Value ([uint32]$GlyphCount)
Set-U32LE -Buffer $bytes -Offset 20 -Value ([uint32]$bytesPerGlyph)
Set-U32LE -Buffer $bytes -Offset 24 -Value ([uint32]$Height)
Set-U32LE -Buffer $bytes -Offset 28 -Value ([uint32]$Width)

for ($i = $headerSize; $i -lt $totalSize; $i++) {
    $bytes[$i] = 0x00
}

$workW = [Math]::Max($Width * 4, 96)
$workH = [Math]::Max($Height * 4, 96)
$drawX = [Math]::Max(16, [Math]::Floor($workW / 4))
$drawY = [Math]::Max(16, [Math]::Floor($workH / 4))

$fmt = New-Object System.Drawing.StringFormat
$fmt.FormatFlags = [System.Drawing.StringFormatFlags]::NoClip
$fmt.Alignment = [System.Drawing.StringAlignment]::Near
$fmt.LineAlignment = [System.Drawing.StringAlignment]::Near

$rendered = @{}
$globalMinX = $workW
$globalMinY = $workH
$globalMaxX = -1
$globalMaxY = -1

for ($code = 32; $code -le [Math]::Min(126, $GlyphCount - 1); $code++) {
    $ch = [char]$code

    $bmp = New-Object System.Drawing.Bitmap $workW, $workH
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.Clear([System.Drawing.Color]::Black)
    $g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::SingleBitPerPixelGridFit
    $g.DrawString([string]$ch, $font, [System.Drawing.Brushes]::White, $drawX, $drawY, $fmt)
    $g.Dispose()

    $bbox = Get-BBox -Bmp $bmp

    if ($bbox.Empty -eq $false) {
        if ($bbox.MinX -lt $globalMinX) { $globalMinX = $bbox.MinX }
        if ($bbox.MinY -lt $globalMinY) { $globalMinY = $bbox.MinY }
        if ($bbox.MaxX -gt $globalMaxX) { $globalMaxX = $bbox.MaxX }
        if ($bbox.MaxY -gt $globalMaxY) { $globalMaxY = $bbox.MaxY }
    }

    $rendered[$code] = $bmp
}

if ($globalMaxX -lt $globalMinX -or $globalMaxY -lt $globalMinY) {
    $fmt.Dispose()
    $font.Dispose()
    throw "No visible glyph pixels found during render"
}

$globalW = $globalMaxX - $globalMinX + 1
$globalH = $globalMaxY - $globalMinY + 1

$cropX = $globalMinX
$cropY = $globalMinY

if ($AutoCenter -ne 0 -and $globalW -lt $Width) {
    $cropX = $cropX - [Math]::Floor(($Width - $globalW) / 2)
}

# Keep top anchored to preserve english punctuation/baseline relationships.
$cropX = $cropX + $OffsetX
$cropY = $cropY + $OffsetY

for ($code = 32; $code -le [Math]::Min(126, $GlyphCount - 1); $code++) {
    $src = $rendered[$code]
    $glyphOffset = $headerSize + ($code * $bytesPerGlyph)

    for ($y = 0; $y -lt $Height; $y++) {
        $srcY = $cropY + $y
        if ($srcY -lt 0 -or $srcY -ge $workH) {
            continue
        }

        $rowOffset = $glyphOffset + ($y * $bytesPerRow)

        for ($x = 0; $x -lt $Width; $x++) {
            $srcX = $cropX + $x
            if ($srcX -lt 0 -or $srcX -ge $workW) {
                continue
            }

            if (Pixel-On ($src.GetPixel($srcX, $srcY))) {
                $byteIndex = ($x -shr 3)
                $bitIndex = 7 - ($x -band 7)
                $target = $rowOffset + $byteIndex
                $bytes[$target] = [byte]($bytes[$target] -bor (1 -shl $bitIndex))
            }
        }
    }
}

foreach ($bmp in $rendered.Values) {
    $bmp.Dispose()
}

$fmt.Dispose()
$font.Dispose()

if ([System.IO.Path]::IsPathRooted($OutputPath)) {
    $fullOutput = $OutputPath
} else {
    $fullOutput = Join-Path (Resolve-Path '.') $OutputPath
}

$dir = Split-Path -Parent $fullOutput
if (-not [string]::IsNullOrWhiteSpace($dir)) {
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
}

[System.IO.File]::WriteAllBytes($fullOutput, $bytes)
Write-Output "Generated PSF2: $OutputPath (w=$Width, h=$Height, glyphs=$GlyphCount, bytes=$($bytes.Length), global_bbox=${globalW}x${globalH})"
