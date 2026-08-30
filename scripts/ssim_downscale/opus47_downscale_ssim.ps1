<#
.SYNOPSIS
    智能降采样（简洁多算法输出版）
    - 在脚本顶部直接填数组即可扩展算法
    - 自动命名：原文件名_<算法名>[_ssim].ext（不覆盖原文件）
#>
[CmdletBinding()]
param(
    [Parameter(Position = 0, ValueFromRemainingArguments = $true)]
    [string[]]$Paths,
    [int]$Scale = 50,
    [switch]$KeepLog,
    [int]$EvalThrottle  = 0,
    [int]$FinalThrottle = 0,
    [int]$MpvConcurrency = 1
)

$ErrorActionPreference = 'Stop'
$OutputEncoding        = [System.Text.Encoding]::UTF8
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

# ==============================================================================
# ★★★【在这里填入你需要的算法】★★★
# ==============================================================================

# mpv 的 --dscale 算法（自动挂载 SSimDownscaler，后缀自动补 _ssim）
$MpvDscales = @(
    'catmull_rom',
    'mitchell',
    'bilinear',
    'oversample'
)

# ImageMagick 的 -filter 滤镜名称（后缀为 _滤镜名）
$IMFilters = @(
    'MagicKernelSharp2021',
    'MagicKernelSharp2013'
)

# 用于 SSIM 评分的参考上采样滤镜
$ReferenceUpscaler = 'Lanczos'

# ==============================================================================
# 核心环境与 API 初始化
# ==============================================================================

try {
    $dpiCode = @"
using System;
using System.Runtime.InteropServices;
public class DPIAware {
    [DllImport("user32.dll")]
    public static extern bool SetProcessDpiAwarenessContext(IntPtr dpiFlag);
    public static void MakeAware() { SetProcessDpiAwarenessContext(new IntPtr(-4)); }
}
"@
    if (-not ('DPIAware' -as [type])) { Add-Type -TypeDefinition $dpiCode }
    [DPIAware]::MakeAware()
} catch {
    try {
        $dpiCodeOld = @"
using System;
using System.Runtime.InteropServices;
public class DPIAwareOld {
    [DllImport("shcore.dll")]
    public static extern int SetProcessDpiAwareness(int value);
    public static void MakeAware() { SetProcessDpiAwareness(2); }
}
"@
        if (-not ('DPIAwareOld' -as [type])) { Add-Type -TypeDefinition $dpiCodeOld }
        [DPIAwareOld]::MakeAware()
    } catch {
        Write-Warning "无法设置 DPI 感知，可能导致输出尺寸微小异常。"
    }
}

if ($PSVersionTable.PSVersion.Major -lt 7) {
    Write-Error "本脚本需要 PowerShell 7+（请用 pwsh 运行）。"; exit 1
}

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$Oxipng    = Join-Path $ScriptDir 'oxipng.exe'
$Mpv       = Join-Path $ScriptDir 'mpv.exe'
$Shader    = Join-Path $ScriptDir 'shaders\SSimDownscaler.glsl'
$LuaScript = Join-Path $ScriptDir 'scripts\auto_screenshot.lua'
$MpvReady  = (Test-Path $Mpv) -and (Test-Path $Shader) -and (Test-Path $LuaScript)

$CpuThreads = [Environment]::ProcessorCount
if ($EvalThrottle  -le 0) { $EvalThrottle  = $CpuThreads }
if ($FinalThrottle -le 0) { $FinalThrottle = [Math]::Max(2, [Math]::Floor($CpuThreads / 2)) }

$env:MAGICK_THREAD_LIMIT = '1'
$env:OMP_NUM_THREADS     = '1'

$MpvSemName = "Local\mpv_ssim_final_$PID"
$MpvSem     = [System.Threading.Semaphore]::new($MpvConcurrency, $MpvConcurrency, $MpvSemName)

# 隐形宿主窗口
$csharpCode = @"
using System;
using System.Runtime.InteropServices;
public class MpvHost {
    [DllImport("user32.dll")]
    public static extern IntPtr CreateWindowEx(uint dwExStyle, string lpClassName, string lpWindowName, uint dwStyle, int x, int y, int nWidth, int nHeight, IntPtr hWndParent, IntPtr hMenu, IntPtr hInstance, IntPtr lpParam);
    [DllImport("user32.dll")]
    public static extern bool SetLayeredWindowAttributes(IntPtr hwnd, uint crKey, byte bAlpha, uint dwFlags);
    [DllImport("user32.dll")]
    public static extern bool ShowWindow(IntPtr hwnd, int nCmdShow);
    [DllImport("user32.dll")]
    public static extern bool DestroyWindow(IntPtr hwnd);

    public static IntPtr Create(int width, int height) {
        uint exStyle = 0x00080000 | 0x00000080 | 0x00000020;
        IntPtr hwnd = CreateWindowEx(exStyle, "STATIC", "MpvHost", 0x80000000, 0, 0, width, height, IntPtr.Zero, IntPtr.Zero, IntPtr.Zero, IntPtr.Zero);
        if (hwnd != IntPtr.Zero) {
            SetLayeredWindowAttributes(hwnd, 0, 0, 2);
            ShowWindow(hwnd, 5);
        }
        return hwnd;
    }
}
"@
if (-not ('MpvHost' -as [type])) { Add-Type -TypeDefinition $csharpCode }

try { $null = & magick -version 2>&1 } catch { Write-Error '未找到 magick.exe'; exit 1 }

$validImages = @($Paths | Where-Object { Test-Path -LiteralPath $_ } |
                 ForEach-Object { (Get-Item -LiteralPath $_).FullName })
if ($validImages.Count -eq 0) { Write-Host '无有效图片输入。'; exit 0 }

Write-Host '────────────────────────────────────────────────' -ForegroundColor DarkGray
Write-Host "降采样: ${Scale}%   参考上采样: $ReferenceUpscaler" -ForegroundColor DarkGray
Write-Host "ImageMagick: $($IMFilters -join ', ')" -ForegroundColor Cyan
Write-Host "MPV dscale : $($MpvDscales -join ', ')" -ForegroundColor Cyan
Write-Host "并行线程: $EvalThrottle (评估) / $FinalThrottle (导出)   mpv并发锁: $MpvConcurrency" -ForegroundColor DarkGray
Write-Host '────────────────────────────────────────────────' -ForegroundColor DarkGray

$imageMeta = @{}
foreach ($img in $validImages) {
    $info = & magick identify -format '%w %h' "$img" 2>&1
    if ($info -match '(\d+)\s+(\d+)') {
        $imageMeta[$img] = @{
            W = [int]$Matches[1]; H = [int]$Matches[2]
            M = (Get-Item -LiteralPath $img).LastWriteTime
        }
    }
}

$tasks = [System.Collections.ArrayList]::new()
foreach ($img in $validImages) {
    $m = $imageMeta[$img]

    $tw = [int](($m.W * $Scale + 50) / 100)
    $th = [int](($m.H * $Scale + 50) / 100)
    if ($tw -lt 1) { $tw = 1 }
    if ($th -lt 1) { $th = 1 }

    $imageMeta[$img].TW = $tw
    $imageMeta[$img].TH = $th

    # 添加 IM 任务
    foreach ($f in $IMFilters) {
        [void]$tasks.Add([pscustomobject]@{
            Image  = $img; W = $m.W; H = $m.H; TW = $tw; TH = $th
            Kind   = 'IM'
            Algo   = $f
            Suffix = $f
            Name   = "IM:$f"
        })
    }
    # 添加 MPV 任务
    if ($MpvReady) {
        foreach ($d in $MpvDscales) {
            [void]$tasks.Add([pscustomobject]@{
                Image  = $img; W = $m.W; H = $m.H; TW = $tw; TH = $th
                Kind   = 'MPV'
                Algo   = $d
                Suffix = "${d}_ssim"
                Name   = "mpv:$d+SSIM"
            })
        }
    }
}
Write-Host "任务总数: $($tasks.Count)  开始并行计算..." -ForegroundColor Cyan

# ==============================================================================
# 第一阶段：并行降采样与 SSIM 评分
# ==============================================================================
$allResults = $tasks | ForEach-Object -ThrottleLimit $EvalThrottle -Parallel {
    $t = $_
    $uid   = [guid]::NewGuid().ToString('N').Substring(0,12)
    $down  = [IO.Path]::Combine([IO.Path]::GetTempPath(), "down_${uid}.png")
    $recon = [IO.Path]::Combine([IO.Path]::GetTempPath(), "recon_${uid}.png")

    $res = [pscustomobject]@{
        Image  = $t.Image
        Name   = $t.Name
        Suffix = $t.Suffix
        Path   = $null
        SSIM   = [double]-1
        Error  = $null
    }

    try {
        if ($t.Kind -eq 'MPV') {
            $sem = [System.Threading.Semaphore]::OpenExisting($using:MpvSemName)
            $null = $sem.WaitOne()

            $hwnd = [IntPtr]::Zero
            try {
                $mpvArgs = [System.Collections.ArrayList]::new()
                [void]$mpvArgs.AddRange(@(
                    "`"$($t.Image)`""
                    '--no-config', '--idle=no', '--force-window=yes'
                    '--vo=gpu-next', '--gpu-api=vulkan'
                    '--no-hidpi-window-scale', '--osd-level=0'
                    '--pause=yes', '--hr-seek=yes', '--keep-open=yes'
                    '--deband=no', '--dither-depth=no'
                    '--correct-downscaling=yes', '--linear-downscaling=no', '--sigmoid-upscaling=no'
                    "--dscale=$($t.Algo)"
                    "--glsl-shader=`"$($using:Shader)`""
                    '--screenshot-format=png'
                    '--screenshot-png-compression=0', '--screenshot-png-filter=0'
                    '--screenshot-high-bit-depth=no'
                    "--script=`"$($using:LuaScript)`""
                    "--script-opts=output_path=`"$down`""
                    '--msg-level=all=warn'
                ))

                if ('MpvHost' -as [type]) {
                    $hwnd = [MpvHost]::Create([int]$t.TW, [int]$t.TH)
                }
                if ($hwnd -ne [IntPtr]::Zero) {
                    [void]$mpvArgs.Add("--wid=$($hwnd.ToInt64())")
                }

                $proc = Start-Process -FilePath $using:Mpv -ArgumentList $mpvArgs `
                                      -Wait -PassThru -WindowStyle Hidden
            }
            finally {
                if ($hwnd -ne [IntPtr]::Zero) {
                    [void][MpvHost]::DestroyWindow($hwnd)
                }
                $sem.Release() | Out-Null
                $sem.Dispose()
            }

            if (-not (Test-Path -LiteralPath $down)) {
                throw "mpv 未生成输出文件 (ExitCode=$($proc.ExitCode))"
            }
        } else {
            # ImageMagick 降采样（保持线性光 RGB 空间高精度下采样）
            & magick "$($t.Image)" `
                -alpha off -colorspace RGB `
                -filter $t.Algo -resize "$($t.TW)x$($t.TH)!" `
                -colorspace sRGB -set colorspace sRGB `
                -define png:exclude-chunk=bKGD,cHRM,tIME,date `
                "$down"
            if ($LASTEXITCODE -ne 0) { throw "IM downscale failed" }
        }

        # 统一上采样回原始尺寸用于 SSIM 评测
        & magick "$down" `
            -alpha off -colorspace RGB `
            -filter $using:ReferenceUpscaler -resize "$($t.W)x$($t.H)!" `
            -colorspace sRGB -set colorspace sRGB `
            "$recon"
        if ($LASTEXITCODE -ne 0) { throw "IM upscale failed" }

        $prevEAP = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
        $ssimOut = & magick compare -metric SSIM "$($t.Image)" "$recon" NULL: 2>&1
        $ErrorActionPreference = $prevEAP

        $txt = ($ssimOut | Out-String).Trim()
        if     ($txt -match '\(([0-9]*\.?[0-9]+(?:[eE][+-]?[0-9]+)?)\)') { $v = [double]$Matches[1] }
        elseif ($txt -match  '([0-9]*\.?[0-9]+(?:[eE][+-]?[0-9]+)?)')   { $v = [double]$Matches[1] }
        else { throw "SSIM 解析失败: $txt" }

        $res.Path = $down
        $res.SSIM = if ($v -lt 0.5) { 1.0 - $v } else { $v }
    }
    catch {
        Remove-Item $down -EA SilentlyContinue
        $res.Error = "$_"
    }
    finally {
        Remove-Item $recon -EA SilentlyContinue
    }

    $tag = if ($res.Error) { "失败: $($res.Error)" } else { '{0:N6}' -f $res.SSIM }
    $shortName = if ($res.Image) { [IO.Path]::GetFileName($res.Image) } else { '(未知)' }
    $line = '    [完成] {0,-35} {1,-26} SSIM: {2}' -f $shortName, $res.Name, $tag
    [Console]::WriteLine($line)

    $res
}

# ==============================================================================
# 第二阶段：输出所有带后缀的算法文件并后处理
# ==============================================================================
$grouped = $allResults | Group-Object Image

$grouped | ForEach-Object -ThrottleLimit $FinalThrottle -Parallel {
    $imgPath = $_.Name
    $meta    = ($using:imageMeta)[$imgPath]
    $oxipng  = $using:Oxipng
    $keepLog = $using:KeepLog

    $dir      = [IO.Path]::GetDirectoryName($imgPath)
    $baseName = [IO.Path]::GetFileNameWithoutExtension($imgPath)
    $ext      = [IO.Path]::GetExtension($imgPath)

    $log = [System.Collections.Generic.List[string]]::new()
    $log.Add("==> 导出: $([IO.Path]::GetFileName($imgPath)) (目标尺寸: $($meta.TW)x$($meta.TH))")

    $ok = @($_.Group | Where-Object { -not $_.Error -and $_.Path })
    if ($ok.Count -eq 0) {
        $log.Add("    所有算法均失败，跳过。")
        [Console]::WriteLine(($log -join "`n")); return
    }

    foreach ($item in $ok) {
        $outFile = [IO.Path]::Combine($dir, "${baseName}_$($item.Suffix)${ext}")
        try {
            # 8-bit 输出
            & magick "$($item.Path)" -depth 8 -define png:exclude-chunk=bKGD,cHRM,tIME,date "$outFile"
            if ($LASTEXITCODE -ne 0) { throw "写入目标文件失败" }

            # oxipng 压缩
            if ((Test-Path $oxipng) -and ($ext.ToLower() -eq '.png')) {
                & $oxipng -o 3 --strip safe "$outFile" | Out-Null
            }

            # 还原时间戳
            (Get-Item -LiteralPath $outFile).LastWriteTime = $meta.M
            $log.Add(('    + [生成] {0,-24} -> {1} (SSIM: {2:N6})' -f $item.Name, [IO.Path]::GetFileName($outFile), $item.SSIM))
        } catch {
            $log.Add("    ! [错误] 导出 $($item.Name) 失败: $_")
        } finally {
            Remove-Item $item.Path -EA SilentlyContinue
        }
    }

    if ($keepLog) {
        $logPath = [IO.Path]::Combine($dir, "${baseName}.ssim.log")
        $ok | Sort-Object SSIM -Descending |
            ForEach-Object { '{0,-28} (_{1})  SSIM = {2:N6}' -f $_.Name, $_.Suffix, $_.SSIM } |
            Set-Content -LiteralPath $logPath -Encoding UTF8
    }

    [Console]::WriteLine(($log -join "`n"))
}

if ($MpvSem) { $MpvSem.Dispose() }
Write-Host "`n全部处理完成。" -ForegroundColor Green