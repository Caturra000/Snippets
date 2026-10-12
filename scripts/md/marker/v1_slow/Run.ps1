$CliArgs = @($args)
$Tool = "marker_single.exe"

if ($CliArgs.Count -gt 0 -and (Test-Path -LiteralPath $CliArgs[0])) {
    if (Test-Path -LiteralPath $CliArgs[0] -PathType Container) {
        $Tool = "marker.exe"
    }
    $CliArgs[0] = (Resolve-Path -LiteralPath $CliArgs[0]).Path
}

. "$PSScriptRoot\Env.ps1"
Set-Location $PSScriptRoot

& "$PSScriptRoot\.venv\Scripts\$Tool" @CliArgs `
    --output_dir "$PSScriptRoot\output"
exit $LASTEXITCODE
