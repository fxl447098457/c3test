# pack.ps1 - Zip the publish directory with UTF-8 (EFS) entry names
#
# Why not tar.exe: Windows built-in tar (libarchive) writes non-ASCII file
# names into the zip without the UTF-8 flag (EFS bit 11), so Explorer /
# 7-Zip / WinRAR decode them with the local codepage and produce mojibake.
# .NET ZipArchive (entryNameEncoding = default) stores ASCII names as ASCII
# and any non-ASCII name as UTF-8 with the EFS flag set, which is the
# interoperable way.
#
# Usage:
#   powershell -NoProfile -ExecutionPolicy Bypass -File pack.ps1 -Source <dir> -Out <zip>
#
# Self checks before reporting success:
#   1. every source file has an entry whose name round-trips exactly
#   2. every non-ASCII entry name carries the EFS flag (raw central-dir scan)

param(
    [Parameter(Mandatory = $true)][string]$Source,
    [Parameter(Mandatory = $true)][string]$Out
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

$src = (Resolve-Path -LiteralPath $Source).Path.TrimEnd('\')
$files = @(Get-ChildItem -LiteralPath $src -Recurse -File)
if ($files.Count -eq 0) { throw "no files found under: $src" }

if (Test-Path -LiteralPath $Out) { Remove-Item -LiteralPath $Out -Force }

$zip = [System.IO.Compression.ZipFile]::Open($Out, [System.IO.Compression.ZipArchiveMode]::Create)
try {
    foreach ($f in $files) {
        $rel = $f.FullName.Substring($src.Length + 1).Replace('\', '/')
        $null = [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile(
            $zip, $f.FullName, $rel, [System.IO.Compression.CompressionLevel]::Optimal)
    }
}
finally { $zip.Dispose() }

# ---- check 1: entry names round-trip ----
$check = [System.IO.Compression.ZipFile]::OpenRead($Out)
try {
    $zipSet = New-Object 'System.Collections.Generic.HashSet[string]'
    foreach ($e in $check.Entries) { $null = $zipSet.Add($e.FullName) }
    foreach ($f in $files) {
        $rel = $f.FullName.Substring($src.Length + 1).Replace('\', '/')
        if (-not $zipSet.Contains($rel)) { throw "entry missing in zip: $rel" }
    }
    if ($zipSet.Count -ne $files.Count) {
        throw "entry count mismatch: zip=$($zipSet.Count) src=$($files.Count)"
    }
}
finally { $check.Dispose() }

# ---- check 2: EFS flag set for non-ASCII names (raw central directory) ----
$fs = [System.IO.File]::OpenRead($Out)
$br = New-Object System.IO.BinaryReader($fs)
try {
    while ($fs.Position -lt ($fs.Length - 4)) {
        $sig = $br.ReadUInt32()
        if ($sig -ne 0x02014b50) { $fs.Position -= 3; continue }
        $null = $br.ReadBytes(4)      # version made / version needed
        $flags = $br.ReadUInt16()
        $null = $br.ReadBytes(18)     # method .. usize
        $nameLen = $br.ReadUInt16()
        $extraLen = $br.ReadUInt16()
        $cmtLen = $br.ReadUInt16()
        $null = $br.ReadBytes(12)     # disk .. external attrs
        $nameBytes = $br.ReadBytes($nameLen)
        $nonAscii = $false
        foreach ($b in $nameBytes) { if ($b -gt 0x7F) { $nonAscii = $true; break } }
        if ($nonAscii -and (($flags -band 0x800) -eq 0)) {
            throw ("EFS flag missing for non-ASCII entry: " +
                [System.Text.Encoding]::UTF8.GetString($nameBytes))
        }
        $null = $br.ReadBytes($extraLen + $cmtLen)
    }
}
finally { $fs.Close() }

Write-Output ("pack OK, " + $files.Count + " entries -> " + $Out)
