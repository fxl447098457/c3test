<#
.SYNOPSIS
    Create a GitCode Pull Request (Merge Request) for the current branch.

.DESCRIPTION
    Called by pr.bat. Creates the PR only -- it never merges, never approves and
    never pushes to the protected base branch. A human reviews and merges it on
    the GitCode web UI.

    API: https://gitcode-api.readthedocs.io/  (base https://api.gitcode.com/api/v5)

.NOTES
    Token resolution order:
      1. environment variable GITCODE_TOKEN
      2. file scripts/.gitcode_token (first line)
      3. git credential manager (protocol=https, host=gitcode.com)
#>
[CmdletBinding()]
param(
    [string]$Repo = 'woeoio/c3.vb6.pro',
    [string]$Base = 'main',
    [string]$Head = '',
    [string]$Title = '',
    [string]$Body = '',
    [switch]$Push,
    [switch]$Open,
    [switch]$DryRun
)

$ErrorActionPreference = 'Stop'
$ApiBase = 'https://api.gitcode.com/api/v5'

if (-not ('System.Net.Http.HttpClient' -as [type])) {
    try { Add-Type -AssemblyName System.Net.Http -ErrorAction Stop } catch { }
}

function Fail([string]$Message) {
    Write-Host "[ERROR] $Message" -ForegroundColor Red
    exit 1
}
function Step([string]$Message) { Write-Host "[STEP ] $Message" -ForegroundColor Cyan }
function Info([string]$Message) { Write-Host "       $Message" -ForegroundColor Gray }
function Warn([string]$Message) { Write-Host "[WARN ] $Message" -ForegroundColor Yellow }

# ---------------------------------------------------------------- token
function Get-Token {
    if ($env:GITCODE_TOKEN) { return $env:GITCODE_TOKEN.Trim() }

    $local = Join-Path $PSScriptRoot '.gitcode_token'
    if (Test-Path -LiteralPath $local) {
        $text = (Get-Content -LiteralPath $local -TotalCount 1 -ErrorAction SilentlyContinue)
        if ($text) { return ([string]$text).Trim() }
    }

    # git credential manager. NOTE: piping multi-line text straight into git fails
    # with "missing protocol field" under PowerShell 5.1, so feed the request
    # through a temporary file with native redirection instead.
    $tmpDir = [System.IO.Path]::GetTempPath()
    $inFile = Join-Path $tmpDir ('c3_cred_' + [guid]::NewGuid().ToString('N') + '.in')
    $outFile = $inFile + '.out'
    $errFile = $inFile + '.err'
    try {
        $bytes = [System.Text.Encoding]::ASCII.GetBytes("protocol=https`nhost=gitcode.com`n`n")
        [System.IO.File]::WriteAllBytes($inFile, $bytes)
        Start-Process -FilePath $script:GitPath -ArgumentList @('credential', 'fill') `
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

# ---------------------------------------------------------------- git helpers
# Run git with output captured as UTF-8 (commit subjects may be non-ASCII).
function Invoke-Git {
    param([Parameter(ValueFromRemainingArguments = $true)][string[]]$GitArgs)
    $outFile = Join-Path ([IO.Path]::GetTempPath()) ('c3_git_' + [guid]::NewGuid().ToString('N') + '.out')
    $errFile = $outFile + '.err'
    try {
        $proc = Start-Process -FilePath $script:GitPath -ArgumentList $GitArgs `
            -RedirectStandardOutput $outFile -RedirectStandardError $errFile `
            -NoNewWindow -Wait -PassThru
        $text = ''
        if (Test-Path -LiteralPath $outFile) {
            $text = [IO.File]::ReadAllText($outFile, [System.Text.Encoding]::UTF8)
        }
        return [pscustomobject]@{ ExitCode = $proc.ExitCode; Out = $text }
    }
    finally {
        Remove-Item -LiteralPath $outFile, $errFile -Force -ErrorAction SilentlyContinue
    }
}
function Test-Ref([string]$Ref) {
    $r = Invoke-Git 'rev-parse' '--verify' '--quiet' "$Ref^{commit}"
    return ($r.ExitCode -eq 0)
}
function Get-Ref([string]$Ref) {
    $r = Invoke-Git 'rev-parse' "$Ref^{commit}"
    if ($r.ExitCode -ne 0) { return '' }
    return $r.Out.Trim()
}

# ---------------------------------------------------------------- json
# GitCode rejects a body containing raw non-ASCII bytes ("Parameter description
# field contains invalid byte sequence"), so escape everything above U+007F.
function ConvertTo-AsciiJson([hashtable]$Object) {
    $json = $Object | ConvertTo-Json -Depth 8 -Compress
    $sb = New-Object System.Text.StringBuilder
    foreach ($ch in $json.ToCharArray()) {
        $code = [int]$ch
        if ($code -gt 127) { [void]$sb.Append(('\u{0:x4}' -f $code)) }
        else { [void]$sb.Append($ch) }
    }
    return $sb.ToString()
}

function Invoke-Api {
    param([string]$Method, [string]$Path, [hashtable]$Payload, [switch]$Soft)
    $headers = @{
        'PRIVATE-TOKEN' = $script:Token
        'User-Agent'    = 'c3-pr-script'
        'Accept'        = 'application/json'
    }
    $uri = $ApiBase + $Path
    try {
        if ($PSBoundParameters.ContainsKey('Payload')) {
            $json = ConvertTo-AsciiJson $Payload
            $bytes = [System.Text.Encoding]::ASCII.GetBytes($json)
            return Invoke-RestMethod -Method $Method -Uri $uri -Headers $headers `
                -ContentType 'application/json' -Body $bytes
        }
        return Invoke-RestMethod -Method $Method -Uri $uri -Headers $headers
    }
    catch {
        $detail = $_.Exception.Message
        if ($_.ErrorDetails.Message) { $detail = $detail + ' / ' + $_.ErrorDetails.Message }
        $code = 0
        if ($_.Exception.Response) { $code = [int]$_.Exception.Response.StatusCode.value__ }
        if ($Soft) {
            Warn ('API {0} {1} failed (HTTP {2}) -> {3}' -f $Method, $Path, $code, $detail)
            return $null
        }
        Fail ('API {0} {1} failed -> {2}' -f $Method, $Path, $detail)
    }
}

# Find an already-open PR from <head> into <base>. NOTE: GitCode expects the
# plain branch name in the head filter -- the GitHub style "owner:branch" form
# silently matches nothing.
function Find-OpenPr {
    param([string]$HeadBranch, [string]$BaseBranch)
    $list = Invoke-Api -Method GET -Soft `
        -Path ('/repos/{0}/pulls?state=open&head={1}&base={2}' -f $Repo, [Uri]::EscapeDataString($HeadBranch), [Uri]::EscapeDataString($BaseBranch))
    if ($list) { return @($list)[0] }
    return $null
}

# ---------------------------------------------------------------- preflight
$gitCmd = (Get-Command git -ErrorAction SilentlyContinue)
if (-not $gitCmd) { Fail 'git not found in PATH' }
$script:GitPath = $gitCmd.Path

if (-not $Head) {
    $r = Invoke-Git 'rev-parse' '--abbrev-ref' 'HEAD'
    if ($r.ExitCode -ne 0) { Fail 'cannot detect current branch' }
    $Head = $r.Out.Trim()
}
if (-not $Head -or $Head -eq 'HEAD') { Fail 'detached HEAD, pass -Head explicitly' }
if ($Head -eq $Base) { Fail ("head and base are both [{0}]" -f $Base) }

# Base ref: prefer the local branch, fall back to the remote one.
if (Test-Ref $Base) { $baseRef = $Base }
elseif (Test-Ref "origin/$Base") { $baseRef = "origin/$Base" }
else { Fail ("base branch [{0}] not found locally or on origin" -f $Base) }

if (-not (Test-Ref $Head)) { Fail ("head branch [{0}] not found" -f $Head) }

# Dirty working tree: not fatal, the changes simply will not be in the PR.
$dirty = Invoke-Git 'status' '--porcelain'
if ($dirty.Out.Trim()) {
    Warn 'working tree has uncommitted changes, they will NOT be part of the PR'
}

# Remote sync: the PR is built from origin/<head>, so it must be up to date.
$remoteHead = "origin/$Head"
$needsPush = $false
if (Test-Ref $remoteHead) {
    if ((Get-Ref $remoteHead) -ne (Get-Ref $Head)) { $needsPush = $true }
}
else { $needsPush = $true }

if ($needsPush) {
    if (-not $Push) {
        Fail ("origin/$Head is not in sync with local $Head -> rerun with -Push (or push it manually)")
    }
    Step ("push $Head to origin")
    & $script:GitPath push -u origin $Head
    if ($LASTEXITCODE -ne 0) { Fail 'git push failed' }
}

# ---------------------------------------------------------------- PR content
$log = Invoke-Git 'log' '--no-color' '--pretty=format:%h%x09%s' "$baseRef..$Head"
$commits = @()
foreach ($line in ($log.Out -split "`r?`n")) {
    if ($line.Trim()) { $commits += $line.Trim() }
}
if ($commits.Count -eq 0) { Fail ("no commits in {0} that are not already in {1}" -f $Head, $Base) }

if (-not $Title) {
    if ($commits.Count -eq 1) { $Title = ($commits[0] -split "`t", 2)[1] }
    else { $Title = ("merge {0} into {1}" -f $Head, $Base) }
}
if (-not $Body) {
    $lines = @()
    $lines += ("Merge {0} into {1}." -f $Head, $Base)
    $lines += ''
    $lines += ("{0} commit(s):" -f $commits.Count)
    foreach ($c in $commits) {
        $parts = $c -split "`t", 2
        $lines += ('- {0} {1}' -f $parts[0], $parts[1])
    }
    $Body = ($lines -join "`n")
}

Info ("repo  {0}" -f $Repo)
Info ("head  {0}" -f $Head)
Info ("base  {0}" -f $Base)
Info ("title {0}" -f $Title)

if ($DryRun) {
    Step 'dry-run, nothing was sent to GitCode'
    Info '--- body ---'
    Write-Host $Body -ForegroundColor Gray
    exit 0
}

# ---------------------------------------------------------------- create
$script:Token = Get-Token
if (-not $script:Token) {
    Fail 'no GitCode token found (set GITCODE_TOKEN, write scripts/.gitcode_token, or store it with git credential manager)'
}

$existing = Find-OpenPr -HeadBranch $Head -BaseBranch $Base
if ($existing) {
    Write-Host ''
    Write-Host ('PR already open -> {0}' -f $existing.web_url) -ForegroundColor Green
    Info ('#{0} {1}' -f $existing.iid, $existing.title)
    if ($Open) { Start-Process $existing.web_url | Out-Null }
    exit 0
}

Step 'create pull request'
$payload = @{
    title = $Title
    head  = $Head
    base  = $Base
    body  = $Body
}
$pr = Invoke-Api -Method POST -Path ('/repos/{0}/pulls' -f $Repo) -Payload $payload -Soft
if (-not $pr) {
    # Race / stale cache: the server may already have an open PR for this pair.
    # Re-list and surface the URL instead of pretending we created one.
    $existing = Find-OpenPr -HeadBranch $Head -BaseBranch $Base
    if ($existing) {
        Write-Host ''
        Write-Host ('PR already open -> {0}' -f $existing.web_url) -ForegroundColor Green
        Info ('#{0} {1}' -f $existing.iid, $existing.title)
        if ($Open) { Start-Process $existing.web_url | Out-Null }
        exit 0
    }
    Fail 'PR creation failed and no existing open PR was found'
}

Write-Host ''
Write-Host ('PR created -> {0}' -f $pr.web_url) -ForegroundColor Green
Info ('number   #{0}' -f $pr.iid)
Info ('state    {0}' -f $pr.state)
Info ('changes  {0} file(s), +{1} -{2}' -f $pr.changes_count, $pr.added_lines, $pr.removed_lines)
Info 'not merged on purpose -> review it on GitCode and merge manually'

if ($Open -and $pr.web_url) { Start-Process $pr.web_url | Out-Null }
exit 0
