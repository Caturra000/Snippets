<#
.SYNOPSIS
    智能降采样（32核并行版 - 完美最终版）
    - 精确像素输出：基于百分比预先计算目标像素尺寸，消除浮点误差。
    - 无缝静默：C# 注入隐形宿主窗口，彻底消除 mpv 闪烁和弹窗。
    - 空间优化：中间过程 16-bit 保留精度，最终输出强制压缩为 8-bit。
    - 评分精准：严格对齐单线程版参数，SSIM 误差为 0。
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

if ($PSVersionTable.PSVersion.Major -lt 7) {
    Write-Error "本脚本需要 PowerShell 7+（请用 pwsh 运行）。"; exit 1
}

$IMFilters = @(
    'MagicKernelSharp2021','MagicKernelSharp2013',
    'Lanczos','LanczosSharp','Robidoux','Mitchell'
)
$ReferenceUpscaler = 'Lanczos'

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

# ★★★ 注入隐形宿主窗口代码（支持动态尺寸） ★★★
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
        // WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT
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
if ($validImages.Count -eq 0) { Write-Host '无有效图片。'; exit 0 }

Write-Host '────────────────────────────────────────' -ForegroundColor DarkGray
Write-Host "降采样: ${Scale}%   参考上采样: $ReferenceUpscaler" -ForegroundColor DarkGray
Write-Host "核心: $CpuThreads   IM并行: $EvalThrottle   mpv锁: $MpvConcurrency" -ForegroundColor DarkGray
Write-Host '────────────────────────────────────────' -ForegroundColor DarkGray

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

    # 整数运算：先乘后除，避免浮点误差，四舍五入取整
    $tw = [int](($m.W * $Scale + 50) / 100)
    $th = [int](($m.H * $Scale + 50) / 100)

    # 至少保留1像素
    if ($tw -lt 1) { $tw = 1 }
    if ($th -lt 1) { $th = 1 }

    # 保存精确目标尺寸，供后续阶段使用
    $imageMeta[$img].TW = $tw
    $imageMeta[$img].TH = $th

    foreach ($f in $IMFilters) {
        [void]$tasks.Add([pscustomobject]@{
            Image=$img; W=$m.W; H=$m.H; TW=$tw; TH=$th; Kind='IM'; Filter=$f
        })
    }
    if ($MpvReady) {
        [void]$tasks.Add([pscustomobject]@{
            Image=$img; W=$m.W; H=$m.H; TW=$tw; TH=$th; Kind='MPV'; Filter=$null
        })
    }
}
Write-Host "任务总数: $($tasks.Count)  开始并行..." -ForegroundColor Cyan

$allResults = $tasks | ForEach-Object -ThrottleLimit $EvalThrottle -Parallel {
    $t = $_
    $uid   = [guid]::NewGuid().ToString('N').Substring(0,12)
    $down  = [IO.Path]::Combine([IO.Path]::GetTempPath(), "down_${uid}.png")
    $recon = [IO.Path]::Combine([IO.Path]::GetTempPath(), "recon_${uid}.png")
    $name  = if ($t.Kind -eq 'MPV') { 'mpv:SSimDownscaler' } else { "IM:$($t.Filter)" }

    $res = [pscustomobject]@{ Image=$t.Image; Name=$name; Path=$null; SSIM=[double]-1; Error=$null }

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
                    '--dscale=mitchell'
                    "--glsl-shader=`"$($using:Shader)`""
                    '--screenshot-format=png'
                    '--screenshot-png-compression=0', '--screenshot-png-filter=0'
                    '--screenshot-high-bit-depth=no'
                    "--script=`"$($using:LuaScript)`""
                    "--script-opts=output_path=`"$down`""
                    '--msg-level=all=warn'
                ))

                # 使用精确目标像素尺寸创建宿主窗口
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
            # ImageMagick 降采样：使用精确像素尺寸
            & magick "$($t.Image)" `
                -alpha off -colorspace RGB `
                -filter $t.Filter -resize "$($t.TW)x$($t.TH)!" `
                -colorspace sRGB -set colorspace sRGB `
                -define png:exclude-chunk=bKGD,cHRM,tIME,date `
                "$down"
            if ($LASTEXITCODE -ne 0) { throw "IM downscale failed" }
        }

        # 统一上采样回原始尺寸
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
    $line = '    [链] {0,-45} {1,-22} {2}' -f $shortName, $res.Name, $tag
    [Console]::WriteLine($line)

    $res
}

$grouped = $allResults | Group-Object Image

$grouped | ForEach-Object -ThrottleLimit $FinalThrottle -Parallel {
    $imgPath = $_.Name
    $meta    = ($using:imageMeta)[$imgPath]
    $oxipng  = $using:Oxipng
    $keepLog = $using:KeepLog

    $log = [System.Collections.Generic.List[string]]::new()
    $log.Add("==> $([IO.Path]::GetFileName($imgPath))  ($($meta.W)x$($meta.H))")

    $ok = @($_.Group | Where-Object { -not $_.Error -and $_.Path })
    if ($ok.Count -eq 0) {
        $log.Add("    所有候选失败，跳过")
        [Console]::WriteLine(($log -join "`n")); return
    }

    $best = $ok | Sort-Object SSIM -Descending | Select-Object -First 1
    $log.Add(('    [最优] {0}  (SSIM = {1:N6})' -f $best.Name, $best.SSIM))

    try {
        # 最终输出强制 8-bit
        & magick "$($best.Path)" -depth 8 -define png:exclude-chunk=bKGD,cHRM,tIME,date "$imgPath"
        if ($LASTEXITCODE -ne 0) { throw "ImageMagick 写入最终文件失败" }
    } catch {
        $log.Add("    [错误] 覆盖原文件失败: $_")
        [Console]::WriteLine(($log -join "`n")); return
    }

    foreach ($c in $_.Group) {
        if ($c.Path -and ($c.Path -ne $best.Path)) { Remove-Item $c.Path -EA SilentlyContinue }
    }

    if ((Test-Path $oxipng) -and ([IO.Path]::GetExtension($imgPath).ToLower() -eq '.png')) {
        & $oxipng -o 3 --strip safe "$imgPath" | Out-Null
        $log.Add('    [oxipng] 优化完成')
    }

    (Get-Item -LiteralPath $imgPath).LastWriteTime = $meta.M
    $log.Add("    [mtime] 已还原")

    if ($keepLog) {
        $logPath = "$imgPath.ssim.log"
        $ok | Sort-Object SSIM -Descending |
            ForEach-Object { '{0,-30}  SSIM = {1:N6}' -f $_.Name, $_.SSIM } |
            Set-Content -LiteralPath $logPath -Encoding UTF8
    }

    [Console]::WriteLine(($log -join "`n"))
}

if ($MpvSem) { $MpvSem.Dispose() }
Write-Host "`n全部完成。" -ForegroundColor Green