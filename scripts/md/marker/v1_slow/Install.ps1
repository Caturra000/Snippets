param(
    [ValidateSet("cpu", "cuda12", "cuda13", "vulkan")]
    [string]$Backend = "cpu"
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"
$R = "F:\Models\Marker"

New-Item -ItemType Directory -Force -Path $R | Out-Null
foreach ($d in @("input", "models\surya")) {
    New-Item -ItemType Directory -Force -Path "$R\$d" | Out-Null
}
$Backend | Set-Content "$R\backend.txt" -Encoding ASCII

# Generate process-local environment configuration.
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
    HF_XET_CACHE          = "$R\cache\huggingface\xet"
    HF_ASSETS_CACHE       = "$R\cache\huggingface\assets"
    TORCH_HOME            = "$R\cache\torch"
    MODEL_CACHE_DIR       = "$R\models\datalab"
    USERPROFILE           = "$R\home"
    HOME                  = "$R\home"
    APPDATA               = "$R\home\AppData\Roaming"
    LOCALAPPDATA          = "$R\home\AppData\Local"
    XDG_CACHE_HOME        = "$R\cache"
    TEMP                  = "$R\tmp"
    TMP                   = "$R\tmp"
    OUTPUT_DIR            = "$R\output"
}

foreach ($e in $Paths.GetEnumerator()) {
    [Environment]::SetEnvironmentVariable($e.Key, $e.Value, "Process")
    New-Item -ItemType Directory -Force -Path $e.Value | Out-Null
}

# 使用国内 Hugging Face 镜像
$env:HF_ENDPOINT = "https://hf-mirror.com"
$env:HF_HUB_DISABLE_XET = "1"
$env:HF_HUB_ETAG_TIMEOUT = "30"
$env:HF_HUB_DOWNLOAD_TIMEOUT = "60"

$env:UV_PYTHON_INSTALL_BIN      = "0"
$env:UV_PYTHON_INSTALL_REGISTRY = "0"
$env:UV_PYTHON_NO_REGISTRY      = "1"
$env:PYTHONNOUSERSITE           = "1"
$env:PYTHONUTF8                = "1"

foreach ($name in @("PYTHONPATH", "PYTHONHOME", "VIRTUAL_ENV")) {
    Remove-Item "Env:$name" -ErrorAction SilentlyContinue
}

$env:TORCH_DEVICE            = "cpu"
$env:FAST_DETECTOR_DEVICE    = "cpu"
$env:SURYA_INFERENCE_BACKEND = "llamacpp"
$env:SURYA_INFERENCE_PARALLEL = "1"
$env:SURYA_INFERENCE_CTX_SIZE = "16384"

# Avoid inheriting an unrelated inference endpoint.
Remove-Item Env:SURYA_INFERENCE_URL -ErrorAction SilentlyContinue

$Backend = (Get-Content "$R\backend.txt").Trim()
$env:LLAMA_CPP_BINARY = "$R\llama\$Backend\llama-server.exe"
$env:LLAMA_CPP_NGL = if ($Backend -eq "cpu") { "0" } else { "99" }

$env:SURYA_GGUF_LOCAL_MODEL_PATH =
    "$R\models\surya\surya-2.gguf"
$env:SURYA_GGUF_LOCAL_MMPROJ_PATH =
    "$R\models\surya\surya-2-mmproj.gguf"

# Save extracted PDF images (used by Markdown/HTML) as PNG instead of JPEG.
# Marker 2.0.0 reads this Pydantic setting from the environment; the name
# has no MARKER_ prefix.
$env:OUTPUT_IMAGE_FORMAT = "PNG"
'@ | Set-Content "$R\Env.ps1" -Encoding UTF8

. "$R\Env.ps1"
Set-Location $R

[Net.ServicePointManager]::SecurityProtocol =
    [Net.SecurityProtocolType]::Tls12

# Extract portable executables and DLLs into the destination.
function Install-ZipTools($Url, $Destination, $Name) {
    New-Item -ItemType Directory -Force -Path $Destination | Out-Null
    $Zip = "$R\tmp\$Name.zip"
    $Unpack = "$R\tmp\$Name"

    Invoke-WebRequest -UseBasicParsing -Uri $Url -OutFile $Zip
    Expand-Archive -Path $Zip -DestinationPath $Unpack -Force

    Get-ChildItem $Unpack -Recurse -File |
        Where-Object { $_.Extension -in @(".exe", ".dll") } |
        ForEach-Object {
            Copy-Item -LiteralPath $_.FullName -Destination $Destination -Force
        }
}

Install-ZipTools `
    "https://github.com/astral-sh/uv/releases/latest/download/uv-x86_64-pc-windows-msvc.zip" `
    "$R\bin" "uv"

$Uv = "$R\bin\uv.exe"
$Py = "$R\.venv\Scripts\python.exe"

& $Uv --no-config python install 3.12 --no-bin
if ($LASTEXITCODE -ne 0) { throw "Python installation failed." }

if (-not (Test-Path $Py)) {
    & $Uv --no-config venv --python 3.12 --managed-python "$R\.venv"
    if ($LASTEXITCODE -ne 0) { throw "Virtual environment creation failed." }
}

& $Uv --no-config pip install --python $Py --torch-backend cpu `
    "marker-pdf==2.0.0" "surya-ocr==0.22.1"
if ($LASTEXITCODE -ne 0) { throw "Marker dependency installation failed." }

$Flavor = (@{
    cpu    = "win-cpu-x64"
    cuda12 = "win-cuda-12.4-x64"
    cuda13 = "win-cuda-13.4-x64"
    vulkan = "win-vulkan-x64"
})[$Backend]

$Tag = "b11552"
$Base = "https://github.com/ggml-org/llama.cpp/releases/download/$Tag"
$LlamaDir = "$R\llama\$Backend"

Install-ZipTools "$Base/llama-$Tag-bin-$Flavor.zip" `
    $LlamaDir "llama-$Backend"

if ($Backend -like "cuda*") {
    Install-ZipTools "$Base/cudart-llama-bin-$Flavor.zip" `
        $LlamaDir "cudart-$Backend"
}

& $env:LLAMA_CPP_BINARY --version
if ($LASTEXITCODE -ne 0) { throw "llama-server could not start." }

# Download the official VLM and vision projector.
@'
import os
os.environ.setdefault("HF_ENDPOINT", "https://hf-mirror.com")

from pathlib import Path
from huggingface_hub import hf_hub_download
from surya.settings import settings

destination = str(Path(settings.SURYA_GGUF_LOCAL_MODEL_PATH).parent)
for filename in (settings.SURYA_GGUF_MODEL_FILE,
                 settings.SURYA_GGUF_MMPROJ_FILE):
    print(hf_hub_download(
        repo_id=settings.SURYA_GGUF_REPO,
        filename=filename,
        local_dir=destination,
    ))
'@ | & $Py -
if ($LASTEXITCODE -ne 0) { throw "VLM download failed." }

# Generate a launcher for both single files and folders.
@'
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
'@ | Set-Content "$R\Run.ps1" -Encoding UTF8

& $Uv --no-config pip freeze --python $Py |
    Set-Content "$R\requirements-installed.txt" -Encoding UTF8

& $Py -c "import importlib.metadata as m; print('Marker:', m.version('marker-pdf')); print('Surya:', m.version('surya-ocr'))"
if ($LASTEXITCODE -ne 0) { throw "Package verification failed." }

Write-Host "Installation complete. Use F:\Models\Marker\Run.ps1"