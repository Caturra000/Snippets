param(
    [ValidateSet("cpu", "cuda")]
    [string]$Backend = "cuda"
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"
$R = "F:\Models\docling"

New-Item -ItemType Directory -Force -Path $R | Out-Null
foreach ($d in @("input", "models", "output", "logs")) {
    New-Item -ItemType Directory -Force -Path "$R\$d" | Out-Null
}
$Backend | Set-Content "$R\backend.txt" -Encoding ASCII

# ------------------------------------------------ process-local Env.ps1
@'
$R = $PSScriptRoot

$Paths = @{
    UV_PYTHON_INSTALL_DIR = "$R\python"
    UV_PYTHON_CACHE_DIR   = "$R\cache\python"
    UV_PYTHON_BIN_DIR     = "$R\bin"
    UV_CACHE_DIR          = "$R\cache\uv"
    PIP_CACHE_DIR         = "$R\cache\pip"
    HF_HOME               = "$R\cache\huggingface"
    HF_HUB_CACHE          = "$R\cache\huggingface\hub"
    TORCH_HOME            = "$R\cache\torch"
    DOCLING_ARTIFACTS_PATH = "$R\models"
    USERPROFILE           = "$R\home"
    HOME                  = "$R\home"
    APPDATA               = "$R\home\AppData\Roaming"
    LOCALAPPDATA          = "$R\home\AppData\Local"
    XDG_CACHE_HOME        = "$R\cache"
    TEMP                  = "$R\tmp"
    TMP                   = "$R\tmp"
}

foreach ($e in $Paths.GetEnumerator()) {
    [Environment]::SetEnvironmentVariable($e.Key, $e.Value, "Process")
    New-Item -ItemType Directory -Force -Path $e.Value | Out-Null
}

# Mirrors (process-local only).
$env:HF_ENDPOINT             = "https://hf-mirror.com"
$env:HF_HUB_ETAG_TIMEOUT     = "30"
$env:HF_HUB_DOWNLOAD_TIMEOUT = "60"
$env:UV_DEFAULT_INDEX        = "https://pypi.tuna.tsinghua.edu.cn/simple"

$env:UV_PYTHON_INSTALL_BIN      = "0"
$env:UV_PYTHON_INSTALL_REGISTRY = "0"
$env:UV_PYTHON_NO_REGISTRY      = "1"
$env:PYTHONNOUSERSITE           = "1"
$env:PYTHONUTF8                 = "1"

foreach ($name in @("PYTHONPATH", "PYTHONHOME", "VIRTUAL_ENV")) {
    Remove-Item "Env:$name" -ErrorAction SilentlyContinue
}
'@ | Set-Content "$R\Env.ps1" -Encoding UTF8

. "$R\Env.ps1"
Set-Location $R

[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

# ------------------------------------------------ helpers

# GitHub accelerator proxy. Set to "" to always use github.com directly.
$GhProxy = "https://ghfast.top/"

function Get-WithFallback {
    param(
        [string[]]$Urls,
        [string]$OutFile
    )

    $LastError = ""

    foreach ($u in $Urls) {
        Write-Host ""
        Write-Host "Downloading: $u" -ForegroundColor Cyan

        try {
            Invoke-WebRequest `
                -UseBasicParsing `
                -Uri $u `
                -OutFile $OutFile `
                -TimeoutSec 180 `
                -ErrorAction Stop

            return
        }
        catch {
            $LastError = $_.Exception.Message

            Write-Host "Download failed: $u" `
                -ForegroundColor DarkYellow

            Write-Host "Reason: $LastError" `
                -ForegroundColor DarkYellow

            # Remove an incomplete download before trying the next source.
            Remove-Item `
                -LiteralPath $OutFile `
                -Force `
                -ErrorAction SilentlyContinue
        }
    }

    throw "All download sources failed for $OutFile`nLast error: $LastError"
}

function Install-ZipTools($Urls, $Destination, $Name) {
    New-Item -ItemType Directory -Force -Path $Destination | Out-Null
    $Zip = "$R\tmp\$Name.zip"
    $Unpack = "$R\tmp\$Name"

    Get-WithFallback $Urls $Zip
    Expand-Archive -Path $Zip -DestinationPath $Unpack -Force

    Get-ChildItem $Unpack -Recurse -File |
        Where-Object { $_.Extension -in @(".exe", ".dll") } |
        ForEach-Object {
            Copy-Item -LiteralPath $_.FullName -Destination $Destination -Force
        }
}

# ------------------------------------------------ uv + Python 3.12

$UvPath = "astral-sh/uv/releases/latest/download/uv-x86_64-pc-windows-msvc.zip"
$UvUrl = "https://github.com/$UvPath"

# Two independent strings; no "+" / comma precedence ambiguity.
$UvUrls = @(
    "${GhProxy}${UvUrl}"
    $UvUrl
)

Write-Host "UV download sources: $($UvUrls.Count)"

Install-ZipTools `
    -Urls $UvUrls `
    -Destination "$R\bin" `
    -Name "uv"

$Uv = "$R\bin\uv.exe"
$Py = "$R\.venv\Scripts\python.exe"

# Mirror for python-build-standalone; fall back to direct download.
$env:UV_PYTHON_INSTALL_MIRROR =
    "$GhProxy" +
    "https://github.com/astral-sh/python-build-standalone/releases/download"

& $Uv --no-config python install 3.12 --no-bin
if ($LASTEXITCODE -ne 0) {
    Remove-Item Env:UV_PYTHON_INSTALL_MIRROR -ErrorAction SilentlyContinue
    & $Uv --no-config python install 3.12 --no-bin
    if ($LASTEXITCODE -ne 0) { throw "Python installation failed." }
}

if (-not (Test-Path $Py)) {
    & $Uv --no-config venv --python 3.12 --managed-python "$R\.venv"
    if ($LASTEXITCODE -ne 0) { throw "Virtual environment creation failed." }
}

# ------------------------------------------------ Docling + PyTorch
# RTX 5070 Ti (Blackwell, sm_120) requires the cu128 (or newer) wheels.
# Torch wheels come from download.pytorch.org; there is no reliable domestic
# mirror for cu128. If it is too slow, try replacing --torch-backend with:
#   --index https://mirrors.aliyun.com/pytorch-wheels/cu128
$TorchBackend = if ($Backend -eq "cuda") { "cu128" } else { "cpu" }

& $Uv --no-config pip install --python $Py --torch-backend $TorchBackend `
    "docling==2.137.0"
if ($LASTEXITCODE -ne 0) { throw "Docling installation failed." }

# ------------------------------------------------ verify GPU
& $Py -c @"
import torch
ok = torch.cuda.is_available()
print('torch', torch.__version__, '| cuda available:', ok)
if ok:
    print('device:', torch.cuda.get_device_name(0))
"@
if ($LASTEXITCODE -ne 0) { throw "PyTorch verification failed." }

if ($Backend -eq "cuda") {
    & $Py -c "import torch, sys; sys.exit(0 if torch.cuda.is_available() else 1)"
    if ($LASTEXITCODE -ne 0) {
        throw "CUDA backend requested but torch.cuda.is_available() is False."
    }
}

# ------------------------------------------------ prefetch models (hf-mirror)
# Downloads layout / tableformer / picture classifier / code formula / OCR
# models into $R\models so conversion never touches the network.
& "$R\.venv\Scripts\docling-tools.exe" models download -o "$R\models"
if ($LASTEXITCODE -ne 0) { throw "Model prefetch failed." }

# ------------------------------------------------ launcher
@'
$CliArgs = @($args)

. "$PSScriptRoot\Env.ps1"
Set-Location $PSScriptRoot

$env:HF_HUB_OFFLINE = "1"

& "$PSScriptRoot\.venv\Scripts\docling.exe" @CliArgs `
    --artifacts-path "$PSScriptRoot\models" `
    --output "$PSScriptRoot\output"
exit $LASTEXITCODE
'@ | Set-Content "$R\Run.ps1" -Encoding UTF8

& $Uv --no-config pip freeze --python $Py |
    Set-Content "$R\requirements-installed.txt" -Encoding UTF8

& "$R\.venv\Scripts\docling.exe" --version
if ($LASTEXITCODE -ne 0) { throw "Docling CLI verification failed." }

Write-Host "Installation complete. Use F:\Models\docling\Convert.cmd" `
    -ForegroundColor Green