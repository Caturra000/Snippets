# Docling drag-and-drop converter.
#
# Defaults: --to md, --device <backend.txt>, --table-mode accurate,
#           --image-export-mode referenced, offline artifacts.
#
# Script-only options:
#   --nopause    Do not wait for Enter at the end.
#   --recurse    Search dropped folders recursively.
#
# Docling options may follow the input files, e.g.:
#   Convert.cmd a.pdf --force-ocr --ocr-lang "ch_sim,en"
#   Convert.cmd a.pdf -- --to html

$ErrorActionPreference = "Stop"

$R = $PSScriptRoot
. "$R\Env.ps1"
Set-Location $R

# Console/native-process encoding; current process/console only.
$Utf8Console = [System.Text.UTF8Encoding]::new($false)

[Console]::InputEncoding  = $Utf8Console
[Console]::OutputEncoding = $Utf8Console
$OutputEncoding          = $Utf8Console

$env:PYTHONUTF8        = "1"
$env:PYTHONIOENCODING  = "utf-8"
$env:PYTHONUNBUFFERED  = "1"

$Py     = "$R\.venv\Scripts\python.exe"
$Runner = "$R\DoclingRunner.py"

$env:HF_HUB_OFFLINE = "1"

$Docling = "$R\.venv\Scripts\docling.exe"
$OutDir  = "$R\output"
$LogDir  = "$R\logs"

New-Item -ItemType Directory -Force -Path $OutDir, $LogDir | Out-Null

$NoPause = $false
$Recurse = $false
$Passthrough = $false

$Items = New-Object System.Collections.Generic.List[string]
$DoclingArgs = New-Object System.Collections.Generic.List[string]

function Stop-Here {
    param([string]$Message, [int]$Code = 1)
    Write-Host ""
    Write-Host $Message -ForegroundColor Yellow
    if (-not $script:NoPause) {
        Write-Host ""
        Read-Host "Press Enter to close"
    }
    exit $Code
}

if (-not (Test-Path -LiteralPath $Docling)) {
    Stop-Here "Docling was not found: $Docling (run Install.ps1 first)"
}

# ---------------------------------------------------- parse arguments
foreach ($arg in $args) {
    $value = [string]$arg

    if ($value -ieq "--nopause") { $NoPause = $true; continue }
    if ($value -ieq "--recurse") { $Recurse = $true; continue }
    if ($value -eq "--")         { $Passthrough = $true; continue }

    if (-not $Passthrough -and (Test-Path -LiteralPath $value)) {
        $Items.Add((Resolve-Path -LiteralPath $value).Path)
        continue
    }

    $Passthrough = $true
    $DoclingArgs.Add($value)
}

function Test-DoclingOption {
    param([string]$Name)
    foreach ($value in $DoclingArgs) {
        if ($value -ieq $Name) { return $true }
        if ($value.StartsWith($Name + "=",
            [StringComparison]::OrdinalIgnoreCase)) { return $true }
    }
    return $false
}

# ---------------------------------------------------- collect input files
$Extensions = @(
    ".pdf", ".docx", ".pptx", ".xlsx", ".html", ".htm", ".md", ".csv",
    ".png", ".jpg", ".jpeg", ".tif", ".tiff", ".bmp", ".webp"
)

$Files = New-Object System.Collections.Generic.List[string]

foreach ($item in $Items) {
    if (Test-Path -LiteralPath $item -PathType Container) {
        Get-ChildItem -LiteralPath $item -File -Recurse:$Recurse |
            Where-Object { $_.Extension.ToLower() -in $Extensions } |
            Sort-Object FullName |
            ForEach-Object { $Files.Add($_.FullName) }
        continue
    }

    if ([IO.Path]::GetExtension($item).ToLower() -in $Extensions) {
        $Files.Add($item)
    }
    else {
        Write-Host "Skipped unsupported file: $item" -ForegroundColor DarkYellow
    }
}

$Files = @($Files | Select-Object -Unique)

if ($Files.Count -eq 0) {
    Write-Host ""
    Write-Host "Docling drag-and-drop converter"
    Write-Host "-------------------------------"
    Write-Host "Drop files or folders onto Convert.cmd."
    Write-Host ""
    Write-Host "Examples:"
    Write-Host '  Convert.cmd "D:\test.pdf"'
    Write-Host '  Convert.cmd "D:\scans" --recurse --force-ocr'
    Write-Host '  Convert.cmd "D:\test.pdf" --ocr-lang "ch_sim,en"'
    Stop-Here "No supported input files were supplied."
}

# ---------------------------------------------------- defaults
$Backend = "cpu"
if (Test-Path -LiteralPath "$R\backend.txt") {
    $Backend = (Get-Content -LiteralPath "$R\backend.txt" -Raw).Trim()
}
$Device = if ($Backend -eq "cuda") { "cuda" } else { "cpu" }

if (-not (Test-DoclingOption "--to")) {
    $DoclingArgs.Add("--to"); $DoclingArgs.Add("md")
}
if (-not (Test-DoclingOption "--device")) {
    $DoclingArgs.Add("--device"); $DoclingArgs.Add($Device)
}
if (-not (Test-DoclingOption "--table-mode")) {
    $DoclingArgs.Add("--table-mode"); $DoclingArgs.Add("accurate")
}
if (-not (Test-DoclingOption "--image-export-mode")) {
    $DoclingArgs.Add("--image-export-mode"); $DoclingArgs.Add("referenced")
}
if (-not (Test-DoclingOption "--artifacts-path")) {
    $DoclingArgs.Add("--artifacts-path"); $DoclingArgs.Add("$R\models")
}

# Enable useful Docling diagnostics unless verbosity was supplied explicitly.
$HasShortVerbose = $false

foreach ($value in $DoclingArgs) {
    if ($value -match '^-v+$') {
        $HasShortVerbose = $true
        break
    }
}

if (
    -not (Test-DoclingOption "--verbose") -and
    -not $HasShortVerbose
) {
    $DoclingArgs.Add("-v")
}

# The wrapper always owns the output directory.
$DoclingArgs.Add("--output"); $DoclingArgs.Add($OutDir)

# ---------------------------------------------------- run
$Stamp = (Get-Date).ToString("yyyyMMdd-HHmmss")
$Log = Join-Path $LogDir "$Stamp-batch.log"

Write-Host ""
Write-Host ("Docling converter | device=$Device | files=" + $Files.Count)
Write-Host ("Options: " + ($DoclingArgs -join " "))
Write-Host ("Output:  " + $OutDir)
Write-Host ("Log:     " + $Log)
Write-Host ""

if ($Backend -eq "cpu") {
    Write-Host "WARNING: backend.txt is set to cpu." -ForegroundColor Yellow
}

$Started = Get-Date
$ExitCode = 2
$RunError = $null

$Utf8NoBom = New-Object -TypeName System.Text.UTF8Encoding -ArgumentList @($false)
$Writer = $null

# Keep Stop for the wrapper itself, but not while receiving native stderr.
$SavedErrorActionPreference = $ErrorActionPreference

try {
    # Create the log before starting Docling. This guarantees that even
    # startup failures leave a log file behind.
    $Writer = New-Object -TypeName System.IO.StreamWriter `
        -ArgumentList @($Log, $false, $Utf8NoBom)
    $Writer.AutoFlush = $true

    $Writer.WriteLine("Started: " + (Get-Date).ToString("yyyy-MM-dd HH:mm:ss"))
    $Writer.WriteLine("PowerShell: " + $PSVersionTable.PSVersion.ToString())
    $Writer.WriteLine("Docling: " + $Docling)
    $Writer.WriteLine("Backend: " + $Backend)
    $Writer.WriteLine("Device: " + $Device)
    $Writer.WriteLine("Files:")

    foreach ($file in $Files) {
        $Writer.WriteLine("  " + $file)
    }

    $Writer.WriteLine("Options: " + ($DoclingArgs -join " "))
    $Writer.WriteLine("")

    # The Python runner merges stderr into stdout internally.
    # Do not add PowerShell "2>&1" here.
    $ErrorActionPreference = "Stop"
    $LASTEXITCODE = $null

    if (-not (Test-Path -LiteralPath $Py)) {
        throw "Python was not found: $Py"
    }

    if (-not (Test-Path -LiteralPath $Runner)) {
        throw "Runner was not found: $Runner"
    }

    $Writer.WriteLine("Python runner: " + $Runner)

    & $Py -u $Runner @Files @DoclingArgs |
        ForEach-Object {
            $Text = [string]$_
            $Writer.WriteLine($Text)
            Write-Host $Text
        }

    if ($null -eq $LASTEXITCODE) {
        $ExitCode = 2
        $RunError = (
            "Docling did not provide an exit code. " +
            "The executable may have failed to start."
        )
    }
    else {
        $ExitCode = [int]$LASTEXITCODE
    }
}
catch {
    $ExitCode = 2
    $RunError = ($_ | Out-String).TrimEnd()

    if ($Writer) {
        $Writer.WriteLine("")
        $Writer.WriteLine("Wrapper exception:")
        $Writer.WriteLine($RunError)
    }

    Write-Host ""
    Write-Host "Wrapper exception:" -ForegroundColor Red
    Write-Host $RunError -ForegroundColor DarkRed
}
finally {
    $ErrorActionPreference = $SavedErrorActionPreference

    if ($Writer) {
        try {
            $Writer.WriteLine("")
            $Writer.WriteLine("Exit code: " + $ExitCode)
            $Writer.WriteLine("Finished: " + (Get-Date).ToString("yyyy-MM-dd HH:mm:ss"))
            $Writer.Flush()
            $Writer.Close()
        }
        catch {
        }

        $Writer = $null
    }
}

$Elapsed = (Get-Date) - $Started

Write-Host ""

if ($ExitCode -eq 0) {
    Write-Host (
        "Done in {0:00}:{1:00}:{2:00}" -f
        [int]$Elapsed.TotalHours,
        $Elapsed.Minutes,
        $Elapsed.Seconds
    ) -ForegroundColor Green
}
else {
    Write-Host (
        "Docling exited with code " +
        $ExitCode +
        "."
    ) -ForegroundColor Red

    if ($RunError) {
        Write-Host ""
        Write-Host $RunError -ForegroundColor DarkRed
    }

    Write-Host (
        "See log: " +
        $Log
    ) -ForegroundColor Yellow
}

Write-Host (
    "Output: " +
    $OutDir
)

Write-Host (
    "Log: " +
    $Log
)

if (-not $NoPause) {
    Write-Host ""
    Read-Host "Press Enter to close"
}

exit $ExitCode