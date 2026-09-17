<#
.SYNOPSIS
    Create a GitCode Release and upload one attachment.

.DESCRIPTION
    Called by release.bat. Keeps no state: everything needed is passed as
    parameters. API reference: https://gitcode-api.readthedocs.io/

.NOTES
    Token resolution order:
      1. environment variable GITCODE_TOKEN
      2. file scripts/.gitcode_token (first line)
      3. git credential manager (protocol=https, host=gitcode.com)
#>
[CmdletBinding()]
param(
    [string]$Repo = 'woeoio/c3.vb6.pro',
    [string]$Tag = '',
    [string]$Version = '',
    [string]$Zip = '',
    [string]$NotesFile = '',
    [string]$TemplateFile = '',
    [switch]$Force
)

$ErrorActionPreference = 'Stop'
$ApiBase = 'https://api.gitcode.com/api/v5'
$VerifyTries = 10

# Windows PowerShell 5.1 does not load System.Net.Http automatically
if (-not ('System.Net.Http.HttpClient' -as [type])) {
    try { Add-Type -AssemblyName System.Net.Http -ErrorAction Stop } catch { }
}

function Fail([string]$Message) {
    Write-Host "[ERROR] $Message" -ForegroundColor Red
    exit 1
}
function Step([string]$Message) { Write-Host "[STEP ] $Message" -ForegroundColor Cyan }
function Info([string]$Message) { Write-Host "       $Message" -ForegroundColor Gray }

# ---------------------------------------------------------------- token
function Get-Token {
    if ($env:GITCODE_TOKEN) { return $env:GITCODE_TOKEN.Trim() }

    $local = Join-Path $PSScriptRoot '.gitcode_token'
    if (Test-Path -LiteralPath $local) {
        $text = (Get-Content -LiteralPath $local -TotalCount 1 -ErrorAction SilentlyContinue)
        if ($text) { return ([string]$text).Trim() }
    }

    # Fallback: git credential manager. NOTE: piping multi-line text straight into
    # git fails with "missing protocol field" under PowerShell 5.1, so feed the
    # request through a temporary file with native redirection instead.
    $tmpDir = [System.IO.Path]::GetTempPath()
    $inFile = Join-Path $tmpDir ('c3_cred_' + [guid]::NewGuid().ToString('N') + '.in')
    $outFile = Join-Path $tmpDir ('c3_cred_' + [guid]::NewGuid().ToString('N') + '.out')
    $errFile = Join-Path $tmpDir ('c3_cred_' + [guid]::NewGuid().ToString('N') + '.err')
    try {
        $git = (Get-Command git -ErrorAction SilentlyContinue)
        if (-not $git) { return '' }
        $bytes = [System.Text.Encoding]::ASCII.GetBytes("protocol=https`nhost=gitcode.com`n`n")
        [System.IO.File]::WriteAllBytes($inFile, $bytes)
        Start-Process -FilePath $git.Path -ArgumentList @('credential', 'fill') `
            -RedirectStandardInput $inFile -RedirectStandardOutput $outFile -RedirectStandardError $errFile `
            -NoNewWindow -Wait | Out-Null
        if (Test-Path -LiteralPath $outFile) {
            $line = @(Get-Content -LiteralPath $outFile | Where-Object { $_ -like 'password=*' })[0]
            if ($line) { return $line.Substring('password='.Length) }
        }
    }
    catch { }
    finally {
        Remove-Item -LiteralPath $inFile, $outFile, $errFile -Force -ErrorAction SilentlyContinue
    }
    return ''
}

# ---------------------------------------------------------------- request
function Invoke-Api {
    param(
        [string]$Method,
        [string]$Path,
        [hashtable]$Body
    )
    $uri = $ApiBase + $Path
    $headers = @{
        'PRIVATE-TOKEN' = $script:Token
        'User-Agent'    = 'c3-release-script'
        'Accept'        = 'application/json'
    }
    try {
        if ($PSBoundParameters.ContainsKey('Body')) {
            $json = $Body | ConvertTo-Json -Depth 8 -Compress
            $bytes = [System.Text.Encoding]::UTF8.GetBytes($json)
            return Invoke-RestMethod -Method $Method -Uri $uri -Headers $headers `
                -ContentType 'application/json; charset=utf-8' -Body $bytes
        }
        return Invoke-RestMethod -Method $Method -Uri $uri -Headers $headers
    }
    catch {
        $detail = $_.Exception.Message
        if ($_.ErrorDetails.Message) { $detail = $detail + ' / ' + $_.ErrorDetails.Message }
        Fail ('API {0} {1} failed -> {2}' -f $Method, $Path, $detail)
    }
}

# GET that returns $null on 404 instead of aborting. Used to probe existence.
function Get-Maybe {
    param([string]$Path)
    $uri = $ApiBase + $Path
    $headers = @{
        'PRIVATE-TOKEN' = $script:Token
        'User-Agent'    = 'c3-release-script'
        'Accept'        = 'application/json'
    }
    try {
        return Invoke-RestMethod -Method GET -Uri $uri -Headers $headers
    }
    catch {
        $code = 0
        if ($_.Exception.Response) { $code = [int]$_.Exception.Response.StatusCode.value__ }
        if ($code -eq 404) { return $null }
        Fail ('API GET {0} failed -> {1}' -f $Path, $_.Exception.Message)
    }
}

# ---------------------------------------------------------------- notes
function Get-NotesText {
    param([string]$Version, [string]$Tag, [string]$ZipName)

    if ($NotesFile -and (Test-Path -LiteralPath $NotesFile)) {
        return [System.IO.File]::ReadAllText((Resolve-Path -LiteralPath $NotesFile), [System.Text.Encoding]::UTF8)
    }
    if ($TemplateFile -and (Test-Path -LiteralPath $TemplateFile)) {
        $text = [System.IO.File]::ReadAllText((Resolve-Path -LiteralPath $TemplateFile), [System.Text.Encoding]::UTF8)
        return $text.Replace('{VERSION}', $Version).Replace('{TAG}', $Tag).Replace('{ZIPNAME}', $ZipName)
    }
    return ('C3 v{0}' -f $Version)
}

# ---------------------------------------------------------------- validate
if (-not $Tag) { Fail 'missing -Tag' }
if (-not (Test-Path -LiteralPath $Zip)) { Fail ('zip not found: {0}' -f $Zip) }

$fileName = [System.IO.Path]::GetFileName($Zip)
$escapedTag = [Uri]::EscapeDataString($Tag)
$escapedName = [Uri]::EscapeDataString($fileName)
$sizeMB = [math]::Round(((Get-Item -LiteralPath $Zip).Length / 1MB), 2)

# ---------------------------------------------------------------- token
$script:Token = Get-Token
if (-not $script:Token) {
    Fail 'no GitCode token found (set GITCODE_TOKEN, write scripts/.gitcode_token, or store it with git credential manager)'
}

# ---------------------------------------------------------------- release
$existing = Get-Maybe -Path ('/repos/{0}/releases/tags/{1}' -f $Repo, $escapedTag)
if ($existing) {
    if (-not $Force) { Fail ('release [{0}] already exists, rerun release.bat with --force to overwrite' -f $Tag) }
    Step ('delete existing release [{0}]' -f $Tag)
    Invoke-Api -Method DELETE -Path ('/repos/{0}/releases/{1}' -f $Repo, $escapedTag) | Out-Null
}

Step ('create release [{0}]' -f $Tag)
$body = @{
    tag_name       = $Tag
    name           = ('C3 {0}' -f $Version)
    body           = (Get-NotesText -Version $Version -Tag $Tag -ZipName $fileName)
    release_status = 'latest'
}
$release = Invoke-Api -Method POST -Path ('/repos/{0}/releases' -f $Repo) -Body $body
if (-not $release) { Fail 'release creation returned nothing' }
Info ('release id: {0}' -f $release.id)

# ---------------------------------------------------------------- upload
Step ('request upload url for {0} ({1} MB)' -f $fileName, $sizeMB)
$uploadPath = '/repos/{0}/releases/{1}/upload_url?file_name={2}' -f $Repo, $escapedTag, $escapedName
$upload = Invoke-Api -Method GET -Path $uploadPath
if (-not $upload.url) { Fail 'server returned an empty upload url' }

$meta = @{}
foreach ($prop in $upload.headers.PSObject.Properties) {
    $meta[$prop.Name] = [string]$prop.Value
}

Step ('upload attachment ({0} MB) ...' -f $sizeMB)
$client = New-Object System.Net.Http.HttpClient
$client.Timeout = [TimeSpan]::FromMinutes(30)
$stream = [System.IO.File]::Open($Zip, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::Read)
try {
    $content = New-Object System.Net.Http.StreamContent -ArgumentList $stream
    # Use the exact Content-Type from the pre-signed signature (changing it
    # makes OBS reject with 401), then forward every header the server gave us.
    if ($meta.ContainsKey('Content-Type')) {
        $content.Headers.ContentType = [System.Net.Http.Headers.MediaTypeHeaderValue]::Parse($meta['Content-Type'])
    }
    foreach ($key in $meta.Keys) {
        if ($key -ieq 'Content-Type') { continue }
        [void]$content.Headers.TryAddWithoutValidation($key, $meta[$key])
    }
    $response = $client.PutAsync($upload.url, $content).GetAwaiter().GetResult()
    $status = [int]$response.StatusCode
    $text = ''
    if ($response.Content) { $text = $response.Content.ReadAsStringAsync().GetAwaiter().GetResult() }
    if ($status -lt 200 -or $status -ge 300) { Fail ('upload failed, HTTP {0}: {1}' -f $status, $text) }
}
finally {
    $stream.Close()
}
Info ('storage replied HTTP {0}' -f $status)

# ---------------------------------------------------------------- verify
Step 'verify attachment'
for ($i = 1; $i -le $VerifyTries; $i++) {
    Start-Sleep -Seconds 3
    $current = Invoke-Api -Method GET -Path ('/repos/{0}/releases/tags/{1}' -f $Repo, $escapedTag)
    $asset = $null
    if ($current.assets) {
        $asset = @($current.assets | Where-Object { $_.name -eq $fileName })[0]
    }
    if ($asset) {
        Write-Host ''
        Write-Host ('Published: https://gitcode.com/{0}/releases' -f $Repo) -ForegroundColor Green
        Info ('tag    : {0}' -f $Tag)
        Info ('asset  : {0} ({1} MB)' -f $asset.name, [math]::Round(($asset.size / 1MB), 2))
        exit 0
    }
}
Fail ('attachment was uploaded but did not appear on the release after {0} checks' -f $VerifyTries)
