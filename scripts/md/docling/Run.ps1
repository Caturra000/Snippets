$CliArgs = @($args)

. "$PSScriptRoot\Env.ps1"
Set-Location $PSScriptRoot

$env:HF_HUB_OFFLINE = "1"

& "$PSScriptRoot\.venv\Scripts\docling.exe" @CliArgs `
    --artifacts-path "$PSScriptRoot\models" `
    --output "$PSScriptRoot\output"
exit $LASTEXITCODE
