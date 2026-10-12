# Marker PDF drag-and-drop converter with real Surya request progress.
#
# Default:
#   --mode balanced --force_ocr
#
# Script-only options:
#   --nopause       Do not wait for Enter at the end.
#   --recurse       Search subfolders recursively.
#   --smart-ocr     Do not add --force_ocr automatically.
#
# Marker options can be placed after input files:
#   Convert.cmd a.pdf b.pdf --mode balanced --force_ocr
#
# A literal "--" may be used before Marker options:
#   Convert.cmd a.pdf b.pdf -- --mode balanced

$ErrorActionPreference = "Stop"

$R = $PSScriptRoot
. "$R\Env.ps1"
Set-Location $R

$Py       = "$R\.venv\Scripts\python.exe"
$Runner   = "$R\MarkerProgress.py"
$OutDir   = "$R\output"
$LogDir   = "$R\logs"
$JobDir   = "$R\tmp\jobs"
$Prefix   = "@@MARKER_PROGRESS@@"

New-Item -ItemType Directory -Force -Path `
    $OutDir, $LogDir, $JobDir | Out-Null

$NoPause = $false
$Recurse = $false
$SmartOcr = $false
$MarkerPart = $false

$Items = New-Object System.Collections.Generic.List[string]
$MarkerArgs = New-Object System.Collections.Generic.List[string]

function Stop-Here {
    param(
        [string]$Message,
        [int]$Code = 1
    )

    Write-Host ""
    Write-Host $Message -ForegroundColor Yellow

    if (-not $script:NoPause) {
        Write-Host ""
        Read-Host "Press Enter to close"
    }

    exit $Code
}

if (-not (Test-Path -LiteralPath $Py)) {
    Stop-Here "Python was not found: $Py"
}

if (-not (Test-Path -LiteralPath $Runner)) {
    Stop-Here "MarkerProgress.py was not found: $Runner"
}

# ------------------------------------------------------- parse arguments

foreach ($arg in $args) {
    $value = [string]$arg

    # Wrapper-only switches are recognized anywhere.
    if ($value -ieq "--nopause") {
        $NoPause = $true
        continue
    }

    if ($value -ieq "--recurse") {
        $Recurse = $true
        continue
    }

    if ($value -ieq "--smart-ocr") {
        $SmartOcr = $true
        continue
    }

    if ($value -eq "--") {
        $MarkerPart = $true
        continue
    }

    if (
        -not $MarkerPart -and
        (Test-Path -LiteralPath $value)
    ) {
        $resolved = (
            Resolve-Path -LiteralPath $value
        ).Path

        $Items.Add($resolved)
        continue
    }

    # Once the first non-path argument is encountered, the remaining
    # arguments are treated as Marker options/values.
    $MarkerPart = $true
    $MarkerArgs.Add($value)
}

function Test-MarkerOption {
    param([string]$Name)

    foreach ($value in $MarkerArgs) {
        if ($value -ieq $Name) {
            return $true
        }

        if (
            $value.StartsWith(
                $Name + "=",
                [StringComparison]::OrdinalIgnoreCase
            )
        ) {
            return $true
        }
    }

    return $false
}

function Get-MarkerOptionValue {
    param([string]$Name)

    for ($i = 0; $i -lt $MarkerArgs.Count; $i++) {
        $value = $MarkerArgs[$i]

        if (
            $value.StartsWith(
                $Name + "=",
                [StringComparison]::OrdinalIgnoreCase
            )
        ) {
            return $value.Substring($Name.Length + 1)
        }

        if (
            $value -ieq $Name -and
            $i + 1 -lt $MarkerArgs.Count
        ) {
            return $MarkerArgs[$i + 1]
        }
    }

    return $null
}

# Default to the same GPU validation path used by the user.
if (-not (Test-MarkerOption "--mode")) {
    $MarkerArgs.Add("--mode")
    $MarkerArgs.Add("balanced")
}

# TEMPORARY: default page range for testing. Remove this block after testing.
# if (-not (Test-MarkerOption "--page_range")) {
#     $MarkerArgs.Add("--page_range")
#     $MarkerArgs.Add("0-10")
# }

$Mode = Get-MarkerOptionValue "--mode"

if ([string]::IsNullOrWhiteSpace($Mode)) {
    $Mode = "balanced"
}

# No --force_ocr by default. Pass it explicitly on the command line if
# you want forced full-page OCR.
$ForceOcr = Test-MarkerOption "--force_ocr"

# ------------------------------------------------------- collect PDF files

$Files = New-Object System.Collections.Generic.List[string]

foreach ($item in $Items) {
    if (Test-Path -LiteralPath $item -PathType Container) {
        $found = Get-ChildItem `
            -LiteralPath $item `
            -File `
            -Recurse:$Recurse |
            Where-Object {
                $_.Extension -ieq ".pdf"
            } |
            Sort-Object FullName

        foreach ($file in $found) {
            $Files.Add($file.FullName)
        }

        continue
    }

    if (
        [IO.Path]::GetExtension($item) -ieq ".pdf"
    ) {
        $Files.Add($item)
    }
    else {
        Write-Host (
            "Skipped non-PDF file: " + $item
        ) -ForegroundColor DarkYellow
    }
}

$Files = @(
    $Files |
    Select-Object -Unique
)

if ($Files.Count -eq 0) {
    Write-Host ""
    Write-Host "Marker drag-and-drop converter"
    Write-Host "------------------------------"
    Write-Host "Drop one or more PDF files onto Convert.cmd."
    Write-Host ""
    Write-Host "Defaults:"
    Write-Host "  --mode balanced"
    Write-Host ""
    Write-Host "Force full-page OCR:"
    Write-Host '  Convert.cmd "D:\test.pdf" --force_ocr'
    Write-Host ""
    Write-Host "Use Marker options:"
    Write-Host '  Convert.cmd "D:\test.pdf" --mode balanced'
    Write-Host ""

    Stop-Here "No PDF files were supplied."
}

# ------------------------------------------------------- page counts

$Pages = @{}

try {
    $CounterPath = Join-Path `
        $JobDir `
        ("pagecount-" + [Guid]::NewGuid().ToString("N") + ".py")

    @'
import json
import sys

import pypdfium2 as pdfium

result = {}

for path in sys.argv[1:]:
    count = 0

    try:
        document = pdfium.PdfDocument(path)
        count = len(document)
        document.close()
    except Exception:
        count = 0

    result[path] = count

print(json.dumps(result, ensure_ascii=False))
'@ | Set-Content `
        -LiteralPath $CounterPath `
        -Encoding UTF8

    $PageJson = & $Py $CounterPath @Files 2>$null

    if ($LASTEXITCODE -eq 0 -and $PageJson) {
        $PageObject = (
            ($PageJson | Out-String) |
            ConvertFrom-Json
        )

        foreach (
            $property in
            $PageObject.PSObject.Properties
        ) {
            $Pages[$property.Name] = [int]$property.Value
        }
    }

    Remove-Item `
        -LiteralPath $CounterPath `
        -Force `
        -ErrorAction SilentlyContinue
}
catch {
    # Page counts are optional. Conversion can continue without them.
}

# ------------------------------------------------------- create job

$JobPath = Join-Path `
    $JobDir `
    ("marker-" + [Guid]::NewGuid().ToString("N") + ".json")

$Job = [ordered]@{
    files       = @($Files)
    marker_args = @($MarkerArgs)
    output_dir  = $OutDir
}

$Job |
    ConvertTo-Json -Depth 6 |
    Set-Content `
        -LiteralPath $JobPath `
        -Encoding UTF8

# ------------------------------------------------------- logging

$BatchStamp = (Get-Date).ToString("yyyyMMdd-HHmmss")
$BatchLog = Join-Path `
    $LogDir `
    ($BatchStamp + "-batch.log")

$Utf8NoBom = New-Object System.Text.UTF8Encoding($false)

$BatchWriter = New-Object System.IO.StreamWriter(
    $BatchLog,
    $false,
    $Utf8NoBom
)
$BatchWriter.AutoFlush = $true

$CurrentWriter = $null
$CurrentLog = $null

# ------------------------------------------------------- display state

$CurrentIndex = 0
$CurrentTotal = $Files.Count
$CurrentFile = ""
$CurrentName = "Preparing Marker"
$CurrentPages = 0
$CurrentStage = "Starting"
$StageDone = 0
$StageTotal = 0
$CurrentPct = 0.0
$CurrentStarted = Get-Date
$SpinnerIndex = 0
$LastRender = [DateTime]::MinValue

function Get-ConsoleWidth {
    try {
        $width = [Console]::WindowWidth - 1

        if ($width -lt 50) {
            return 100
        }

        return $width
    }
    catch {
        return 120
    }
}

function Get-Bar {
    param(
        [double]$Percent,
        [int]$Width = 25
    )

    $bounded = [Math]::Max(
        0.0,
        [Math]::Min(100.0, $Percent)
    )

    $filled = [int][Math]::Floor(
        $bounded * $Width / 100.0
    )

    return (
        ("#" * $filled) +
        ("-" * ($Width - $filled))
    )
}

function Clear-StatusLine {
    $width = Get-ConsoleWidth

    try {
        [Console]::Write(
            "`r" + (" " * $width) + "`r"
        )
    }
    catch {
        Write-Host ""
    }
}

function Show-Status {
    $now = Get-Date

    if (
        ($now - $script:LastRender).TotalMilliseconds -lt 100
    ) {
        return
    }

    $script:LastRender = $now

    $spinnerChars = @("|", "/", "-", "\")
    $spinner = $spinnerChars[
        $script:SpinnerIndex % $spinnerChars.Count
    ]
    $script:SpinnerIndex++

    $elapsed = $now - $script:CurrentStarted
    $timeText = "{0:00}:{1:00}" -f `
        [int]$elapsed.TotalMinutes, `
        $elapsed.Seconds

    $pageTag = ""

    if ($script:CurrentPages -gt 0) {
        $pageTag = " " + $script:CurrentPages + "p"
    }

    $stagePart = $script:CurrentStage

    if ($script:StageTotal -gt 0) {
        $stagePart += (
            " " +
            $script:StageDone +
            "/" +
            $script:StageTotal
        )
    }

    $name = $script:CurrentName

    if ($name.Length -gt 42) {
        $name = $name.Substring(0, 39) + "..."
    }

    $indexText = "[{0}/{1}]" -f `
        $script:CurrentIndex, `
        $script:CurrentTotal

    if ($script:CurrentIndex -eq 0) {
        $indexText = "[0/$($script:CurrentTotal)]"
    }

    $text = (
        "{0} {1}{2} [{3}] {4,3}% | {5} {6} | {7}" -f
        $indexText,
        $name,
        $pageTag,
        (Get-Bar $script:CurrentPct 25),
        [int]$script:CurrentPct,
        $stagePart,
        $spinner,
        $timeText
    )

    $width = Get-ConsoleWidth

    if ($text.Length -gt $width) {
        $text = $text.Substring(0, $width)
    }

    try {
        [Console]::Write(
            "`r" + $text.PadRight($width)
        )
    }
    catch {
        Write-Host $text
    }
}

function Get-PhaseName {
    param([string]$Phase)

    switch ($Phase) {
        "layout" {
            return "Layout pages"
        }

        "high_accuracy_bbox" {
            return "OCR pages"
        }

        "block" {
            return "OCR blocks"
        }

        "table_rec" {
            return "Tables"
        }

        "mixed" {
            return "VLM requests"
        }

        default {
            return $Phase
        }
    }
}

function Update-FilePercent {
    param(
        [string]$Phase,
        [int]$Done,
        [int]$Total
    )

    if ($Total -le 0) {
        return
    }

    $ratio = [Math]::Max(
        0.0,
        [Math]::Min(
            1.0,
            [double]$Done / [double]$Total
        )
    )

    $candidate = 0.0

    if (
        $script:ForceOcr -and
        $script:Mode -ieq "balanced"
    ) {
        # In the normal balanced + force_ocr path, the full-page OCR
        # request batch corresponds approximately one request per page.
        switch ($Phase) {
            "high_accuracy_bbox" {
                $candidate = 99.0 * $ratio
            }

            "layout" {
                # Normally only seen as a fallback after an OCR failure.
                $candidate = 95.0 + 4.0 * $ratio
            }

            "block" {
                $candidate = 95.0 + 4.0 * $ratio
            }

            "table_rec" {
                $candidate = 95.0 + 4.0 * $ratio
            }

            default {
                $candidate = 99.0 * $ratio
            }
        }
    }
    elseif ($script:Mode -ieq "balanced") {
        # Without force_ocr, the layout page count is known first, but the
        # number of pages later selected for OCR is not known in advance.
        switch ($Phase) {
            "layout" {
                $candidate = 70.0 * $ratio
            }

            "high_accuracy_bbox" {
                $candidate = 70.0 + 29.0 * $ratio
            }

            "block" {
                $candidate = 70.0 + 29.0 * $ratio
            }

            default {
                $candidate = 90.0 * $ratio
            }
        }
    }
    else {
        # Fast mode has additional local-model stages that are not all VLM
        # requests, so this remains an estimate.
        $candidate = 95.0 * $ratio
    }

    if ($candidate -gt $script:CurrentPct) {
        $script:CurrentPct = [Math]::Min(
            99.0,
            $candidate
        )
    }
}

# ------------------------------------------------------- header

$Backend = "unknown"

if (Test-Path -LiteralPath "$R\backend.txt") {
    $Backend = (
        Get-Content `
            -LiteralPath "$R\backend.txt" `
            -Raw
    ).Trim()
}

Write-Host ""
Write-Host (
    "Marker drag converter | backend=" +
    $Backend +
    " | NGL=" +
    $env:LLAMA_CPP_NGL +
    " | mode=" +
    $Mode +
    " | force_ocr=" +
    $ForceOcr +
    " | files=" +
    $Files.Count
)

Write-Host ("Output: " + $OutDir)
Write-Host ("Batch log: " + $BatchLog)
Write-Host ""

if ($Backend -eq "cpu") {
    Write-Host (
        "WARNING: backend.txt is set to cpu."
    ) -ForegroundColor Yellow
}

# ------------------------------------------------------- start runner

$Psi = New-Object System.Diagnostics.ProcessStartInfo
$Psi.FileName = $Py
$Psi.Arguments = (
    '"' + $Runner + '" "' + $JobPath + '"'
)
$Psi.WorkingDirectory = $R
$Psi.UseShellExecute = $false
$Psi.CreateNoWindow = $true
$Psi.RedirectStandardOutput = $true
$Psi.StandardOutputEncoding = [Text.Encoding]::UTF8

$Process = $null
$Ok = 0
$Fail = 0
$RunnerFatal = $null
$Started = Get-Date

try {
    $Process = [Diagnostics.Process]::Start($Psi)
    $Reader = $Process.StandardOutput

    while ($true) {
        $ReadTask = $Reader.ReadLineAsync()

        while (-not $ReadTask.Wait(200)) {
            Show-Status
        }

        $Line = $ReadTask.Result

        if ($null -eq $Line) {
            break
        }

        $BatchWriter.WriteLine($Line)

        if ($Line.StartsWith($Prefix)) {
            $JsonText = $Line.Substring($Prefix.Length)

            try {
                $Event = $JsonText | ConvertFrom-Json
            }
            catch {
                if ($CurrentWriter) {
                    $CurrentWriter.WriteLine($Line)
                }

                continue
            }

            switch ([string]$Event.event) {
                "runner_start" {
                    $CurrentStage = (
                        "Marker " +
                        $Event.marker_version +
                        ", Surya " +
                        $Event.surya_version
                    )
                }

                "models_start" {
                    $CurrentStage = "Creating model clients"
                }

                "models_ready" {
                    $CurrentStage = "Model clients ready"
                }

                "file_start" {
                    if ($CurrentWriter) {
                        $CurrentWriter.Flush()
                        $CurrentWriter.Close()
                        $CurrentWriter = $null
                    }

                    $CurrentIndex = [int]$Event.index
                    $CurrentTotal = [int]$Event.total
                    $CurrentFile = [string]$Event.file
                    $CurrentName = [IO.Path]::GetFileName(
                        $CurrentFile
                    )
                    $CurrentPages = 0

                    if ($Pages.ContainsKey($CurrentFile)) {
                        $CurrentPages = [int]$Pages[
                            $CurrentFile
                        ]
                    }

                    $CurrentStage = "Preparing document"
                    $StageDone = 0
                    $StageTotal = 0
                    $CurrentPct = 0.0
                    $CurrentStarted = Get-Date

                    $safeStem = (
                        [IO.Path]::GetFileNameWithoutExtension(
                            $CurrentFile
                        ) -replace '[^\w\.\-]', '_'
                    )

                    $CurrentLog = Join-Path `
                        $LogDir `
                        (
                            $BatchStamp +
                            "-" +
                            $CurrentIndex +
                            "-" +
                            $safeStem +
                            ".log"
                        )

                    $CurrentWriter = New-Object `
                        System.IO.StreamWriter(
                            $CurrentLog,
                            $false,
                            $Utf8NoBom
                        )

                    $CurrentWriter.AutoFlush = $true
                    $CurrentWriter.WriteLine($Line)
                }

                "batch_start" {
                    $StageDone = 0
                    $StageTotal = [int]$Event.total
                    $CurrentStage = Get-PhaseName `
                        ([string]$Event.phase)

                    if ($CurrentWriter) {
                        $CurrentWriter.WriteLine($Line)
                    }
                }

                "batch_progress" {
                    $StageDone = [int]$Event.done
                    $StageTotal = [int]$Event.total
                    $CurrentStage = Get-PhaseName `
                        ([string]$Event.phase)

                    Update-FilePercent `
                        ([string]$Event.phase) `
                        $StageDone `
                        $StageTotal

                    if ($CurrentWriter) {
                        $CurrentWriter.WriteLine($Line)
                    }
                }

                "batch_done" {
                    $StageDone = [int]$Event.done
                    $StageTotal = [int]$Event.total

                    if ($CurrentWriter) {
                        $CurrentWriter.WriteLine($Line)
                    }
                }

                "file_done" {
                    if ($CurrentWriter) {
                        $CurrentWriter.WriteLine($Line)
                    }

                    $CurrentPct = 100.0
                    $CurrentStage = "Finished"
                    Show-Status
                    Clear-StatusLine

                    $seconds = [double]$Event.seconds
                    $duration = [TimeSpan]::FromSeconds(
                        $seconds
                    )

                    $timeText = "{0:00}:{1:00}" -f `
                        [int]$duration.TotalMinutes, `
                        $duration.Seconds

                    if (
                        [string]$Event.status -eq "OK"
                    ) {
                        $Ok++

                        Write-Host (
                            "  [OK]   " +
                            $CurrentName +
                            "  " +
                            $timeText +
                            "  -> " +
                            [string]$Event.output_folder
                        ) -ForegroundColor Green
                    }
                    else {
                        $Fail++

                        Write-Host (
                            "  [FAIL] " +
                            $CurrentName +
                            "  " +
                            $timeText
                        ) -ForegroundColor Red

                        if ($Event.error) {
                            Write-Host (
                                "         " +
                                [string]$Event.error
                            ) -ForegroundColor DarkGray
                        }

                        Write-Host (
                            "         log: " +
                            $CurrentLog
                        ) -ForegroundColor DarkGray
                    }

                    if ($CurrentWriter) {
                        $CurrentWriter.Flush()
                        $CurrentWriter.Close()
                        $CurrentWriter = $null
                    }
                }

                "runner_done" {
                    # Individual file_done events already updated counters.
                }

                "runner_fatal" {
                    $RunnerFatal = [string]$Event.error
                }

                default {
                    if ($CurrentWriter) {
                        $CurrentWriter.WriteLine($Line)
                    }
                }
            }

            Show-Status
            continue
        }

        # Normal Marker/Surya log output.
        if ($CurrentWriter) {
            $CurrentWriter.WriteLine($Line)
        }

        if (
            $Line -match "Spawning llamacpp server"
        ) {
            $CurrentStage = "Starting llama.cpp GPU server"
        }
        elseif (
            $Line -match "Spawning:"
        ) {
            $CurrentStage = "Loading VLM on GPU"
        }
        elseif (
            $Line -match "server ready"
        ) {
            $CurrentStage = "GPU server ready"
        }
        elseif (
            $Line -match "Attaching to existing"
        ) {
            $CurrentStage = "Attached to GPU server"
        }

        Show-Status
    }

    $Process.WaitForExit()
    $ExitCode = $Process.ExitCode
}
catch {
    $RunnerFatal = $_.Exception.Message
    $ExitCode = 2
}
finally {
    Clear-StatusLine

    if ($CurrentWriter) {
        $CurrentWriter.Flush()
        $CurrentWriter.Close()
        $CurrentWriter = $null
    }

    if ($BatchWriter) {
        $BatchWriter.Flush()
        $BatchWriter.Close()
    }

    Remove-Item `
        -LiteralPath $JobPath `
        -Force `
        -ErrorAction SilentlyContinue

    if (
        $Process -and
        -not $Process.HasExited
    ) {
        try {
            $Process.Kill()
        }
        catch {
        }
    }
}

# ------------------------------------------------------- summary

$TotalElapsed = (Get-Date) - $Started

Write-Host ""
Write-Host (
    "Done. OK=" +
    $Ok +
    "  FAIL=" +
    $Fail +
    "  total " +
    (
        "{0:00}:{1:00}:{2:00}" -f
        [int]$TotalElapsed.TotalHours,
        $TotalElapsed.Minutes,
        $TotalElapsed.Seconds
    )
)

Write-Host ("Output: " + $OutDir)
Write-Host ("Batch log: " + $BatchLog)

if ($RunnerFatal) {
    Write-Host ""
    Write-Host (
        "Runner error: " + $RunnerFatal
    ) -ForegroundColor Red
}

if ($ExitCode -ne 0 -and $Fail -eq 0) {
    Write-Host ""
    Write-Host (
        "The runner exited with code " +
        $ExitCode +
        ". Check the batch log."
    ) -ForegroundColor Red
}

if (-not $NoPause) {
    Write-Host ""
    Read-Host "Press Enter to close"
}

if (
    $ExitCode -ne 0 -or
    $Fail -gt 0
) {
    exit 1
}

exit 0