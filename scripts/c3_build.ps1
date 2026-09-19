#!/usr/bin/env powershell
# c3_build.ps1 - build a VB6 project with the current C3.exe (MSVC env set natively)
# Usage: c3_build.ps1 -Vbp <path> -OutDir <outdir> [-Extra "--emit-c"]
param(
    [Parameter(Mandatory=$true)][string]$Vbp,
    [Parameter(Mandatory=$true)][string]$OutDir,
    [string]$Extra = ""
)
$msvc   = "D:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Tools\MSVC\14.29.30133"
$kits   = "D:\Windows Kits\10"
$kitinc = "$kits\Include\10.0.19041.0"
$kitlib = "$kits\Lib\10.0.19041.0"
$env:INCLUDE = "$msvc\include;$kitinc\um;$kitinc\ucrt;$kitinc\shared;$kitinc\winrt;$kitinc\cppwinrt"
$env:LIB     = "$msvc\lib\x64;$kitlib\um\x64;$kitlib\ucrt\x64"
$env:PATH    = "$msvc\bin\Hostx64\x64;$kits\bin\10.0.19041.0\x64;$env:PATH"

Set-Location D:\c3.vb6.pro
Write-Host "== building $Vbp =="
$argList = @($Vbp, "--output-dir", $OutDir)
if ($Extra -ne "") { $argList += ($Extra -split ' ') }
& .build\C3.exe @argList 2>&1 | ForEach-Object { "$_" }
Write-Host "== C3 exit=$LASTEXITCODE =="
