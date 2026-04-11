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

function Render-NormalizedGlyph {
    param(
        [char]$Char,
        [System.Drawing.Font]$Font,
        [int]$GlyphWidth,
        [int]$GlyphHeight,
        [int]$ShiftX,
        [int]$ShiftY,
        [int]$DoCenter
    )

    $workW = [Math]::Max($GlyphWidth * 4, 64)
    $workH = [Math]::Max($GlyphHeight * 4, 64)

    $src = New-Object System.Drawing.Bitmap $workW, $workH
    $g = [System.Drawing.Graphics]::FromImage($src)
    $g.Clear([System.Drawing.Color]::Black)
    $g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::SingleBitPerPixelGridFit

    $fmt = New-Object System.Drawing.StringFormat
    $fmt.FormatFlags = [System.Drawing.StringFormatFlags]::NoClip
    $fmt.Alignment = [System.Drawing.StringAlignment]::Near
    $fmt.LineAlignment = [System.Drawing.StringAlignment]::Near

    $size = $g.MeasureString([string]$Char, $Font, 1000, $fmt)
    $drawX = [int][Math]::Floor(($workW - $size.Width) / 2.0)
    $drawY = [int][Math]::Floor(($workH - $size.Height) / 2.0)

    $g.DrawString([string]$Char, $Font, [System.Drawing.Brushes]::White, $drawX, $drawY, $fmt)
    $g.Dispose()
    $fmt.Dispose()

    $minX = $workW
    $minY = $workH
    $maxX = -1
    $maxY = -1

    for ($y = 0; $y -lt $workH; $y++) {
        for ($x = 0; $x -lt $workW; $x++) {
            if (Pixel-On ($src.GetPixel($x, $y))) {
                if ($x -lt $minX) { $minX = $x }
                if ($y -lt $minY) { $minY = $y }
                if ($x -gt $maxX) { $maxX = $x }
                if ($y -gt $maxY) { $maxY = $y }
            }
        }
    }

    $dst = New-Object System.Drawing.Bitmap $GlyphWidth, $GlyphHeight
    for ($y = 0; $y -lt $GlyphHeight; $y++) {
        for ($x = 0; $x -lt $GlyphWidth; $x++) {
            $dst.SetPixel($x, $y, [System.Drawing.Color]::Black)
        }
    }

    if ($maxX -ge $minX -and $maxY -ge $minY) {
        $contentW = $maxX - $minX + 1
        $contentH = $maxY - $minY + 1

        if ($DoCenter -ne 0) {
            $baseX = [int][Math]::Floor(($GlyphWidth - $contentW) / 2.0)
            $baseY = [int][Math]::Floor(($GlyphHeight - $contentH) / 2.0)
        } else {
            $baseX = 0
            $baseY = 0
        }

        $targetX = $baseX + $ShiftX
        $targetY = $baseY + $ShiftY

        for ($y = $minY; $y -le $maxY; $y++) {
            for ($x = $minX; $x -le $maxX; $x++) {
                if (-not (Pixel-On ($src.GetPixel($x, $y)))) {
                    continue
                }

                $nx = $targetX + ($x - $minX)
                $ny = $targetY + ($y - $minY)

                if ($nx -ge 0 -and $nx -lt $GlyphWidth -and $ny -ge 0 -and $ny -lt $GlyphHeight) {
                    $dst.SetPixel($nx, $ny, [System.Drawing.Color]::White)
                }
            }
        }
    }

    $src.Dispose()
    return $dst
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

for ($code = 32; $code -le [Math]::Min(126, $GlyphCount - 1); $code++) {
    $ch = [char]$code
    $glyphBmp = Render-NormalizedGlyph -Char $ch -Font $font -GlyphWidth $Width -GlyphHeight $Height -ShiftX $OffsetX -ShiftY $OffsetY -DoCenter $AutoCenter

    $glyphOffset = $headerSize + ($code * $bytesPerGlyph)

    for ($y = 0; $y -lt $Height; $y++) {
        $rowOffset = $glyphOffset + ($y * $bytesPerRow)

        for ($x = 0; $x -lt $Width; $x++) {
            $p = $glyphBmp.GetPixel($x, $y)
            if (Pixel-On $p) {
                $byteIndex = ($x -shr 3)
                $bitIndex = 7 - ($x -band 7)
                $target = $rowOffset + $byteIndex
                $bytes[$target] = [byte]($bytes[$target] -bor (1 -shl $bitIndex))
            }
        }
    }

    $glyphBmp.Dispose()
}

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
Write-Output "Generated PSF2: $OutputPath (w=$Width, h=$Height, glyphs=$GlyphCount, bytes=$($bytes.Length), center=$AutoCenter)"
