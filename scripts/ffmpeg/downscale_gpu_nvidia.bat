@echo off
setlocal enabledelayedexpansion
chcp 65001 >nul
title 视频降采样优化工具 (自动横竖屏 NVENC AV1)

:: =================【用户分辨率配置区域】=================
:: 只需填写常规横屏基准分辨率，遇到竖屏会自动翻转
set "TARGET_W=2560"
set "TARGET_H=1440"
:: =======================================================

:: 计算长边与短边基准
if %TARGET_W% GTR %TARGET_H% (
    set "BASE_LONG=%TARGET_W%"
    set "BASE_SHORT=%TARGET_H%"
) else (
    set "BASE_LONG=%TARGET_H%"
    set "BASE_SHORT=%TARGET_W%"
)

:: 0. 脚本自身命名检查
if /i "%~n0"=="ffmpeg" (
    echo [严重错误] 本脚本文件名不能命名为 ffmpeg.bat ！
    echo 请将本文件重命名为其他名字（例如：视频优化.bat）后再使用。
    pause
    exit /b
)
if /i "%~n0"=="ffprobe" (
    echo [严重错误] 本脚本文件名不能命名为 ffprobe.bat ！
    pause
    exit /b
)

:: 检查 ffmpeg.exe 和 ffprobe.exe
where ffmpeg.exe >nul 2>&1
if %errorlevel% neq 0 (
    echo [错误] 未在系统 PATH 中找到 ffmpeg.exe，请确保已安装并配置环境变量。
    pause
    exit /b
)
where ffprobe.exe >nul 2>&1
if %errorlevel% neq 0 (
    echo [错误] 未在系统 PATH 中找到 ffprobe.exe，请确保已安装并配置环境变量。
    pause
    exit /b
)

:: 检查是否有拖入文件
if "%~1"=="" (
    echo ========================================================
    echo  请将需要处理的视频文件【直接拖拽】到本批处理文件图标上！
    echo  当前基准分辨率: 横屏 %BASE_LONG%x%BASE_SHORT% / 竖屏 %BASE_SHORT%x%BASE_LONG%
    echo ========================================================
    pause
    exit /b
)

:: 遍历所有拖入的文件
:LOOP
if "%~1"=="" goto END

set "INPUT_FILE=%~f1"
set "OUTPUT_FILE=%~dpn1_optimized%~x1"
set "ORIG_W="
set "ORIG_H="
set "ROT=0"

echo.
echo ========================================================
echo 正在分析文件: "%~nx1"
echo ========================================================

:: 1. 获取原视频原始宽高
for /f "tokens=1,2 delims=," %%a in ('ffprobe.exe -v error -select_streams v:0 -show_entries stream^=width^,height -of csv^=p^=0 "%INPUT_FILE%" 2^>nul') do (
    set "ORIG_W=%%a"
    set "ORIG_H=%%b"
)

if defined ORIG_W set "ORIG_W=%ORIG_W: =%"
if defined ORIG_H set "ORIG_H=%ORIG_H: =%"

if "%ORIG_W%"=="" (
    echo [跳过] 无法获取视频分辨率，可能不是有效的视频文件。
    shift
    goto LOOP
)

:: 2. 读取旋转角度元数据 (rotate / rotation)
for /f "tokens=1 delims=," %%r in ('ffprobe.exe -v error -select_streams v:0 -show_entries stream_tags^=rotate:stream_side_data^=rotation -of csv^=p^=0 "%INPUT_FILE%" 2^>nul') do (
    if not "%%r"=="" set "ROT=%%r"
)
if defined ROT set "ROT=%ROT: =%"

:: 若视频带有 90度/270度 旋转标签，则交换显示宽高
set "SWAP=0"
if "!ROT!"=="90" set "SWAP=1"
if "!ROT!"=="-90" set "SWAP=1"
if "!ROT!"=="270" set "SWAP=1"
if "!ROT!"=="-270" set "SWAP=1"

if "!SWAP!"=="1" (
    set "TMP_W=!ORIG_W!"
    set "ORIG_W=!ORIG_H!"
    set "ORIG_H=!TMP_W!"
)

:: 3. 判断横屏还是竖屏，并分配目标分辨率
if !ORIG_H! GTR !ORIG_W! (
    set "IS_PORTRAIT=1"
    set "ORIENTATION_TEXT=竖屏"
    set "CURR_TARGET_W=%BASE_SHORT%"
    set "CURR_TARGET_H=%BASE_LONG%"
) else (
    set "IS_PORTRAIT=0"
    set "ORIENTATION_TEXT=横屏"
    set "CURR_TARGET_W=%BASE_LONG%"
    set "CURR_TARGET_H=%BASE_SHORT%"
)

echo 画面方向    : !ORIENTATION_TEXT! (旋转角度: !ROT!°)
echo 原画面分辨率: !ORIG_W!x!ORIG_H!
echo 目标分辨率  : !CURR_TARGET_W!x!CURR_TARGET_H!

:: 4. 检查是否放大 (Upscaling 检查)
set "WARN_FLAG=0"
if !CURR_TARGET_W! GTR !ORIG_W! set "WARN_FLAG=1"
if !CURR_TARGET_H! GTR !ORIG_H! set "WARN_FLAG=1"

if "!WARN_FLAG!"=="1" (
    echo.
    echo ----------------------------------------------------
    echo [警告] 目标分辨率大于原片尺寸（将发生放大/插值）！
    echo ----------------------------------------------------
    choice /c YN /m "是否仍然强制继续处理？(Y=继续, N=跳过本文件)"
    if errorlevel 2 (
        echo [已跳过] 用户取消处理该文件。
        shift
        goto LOOP
    )
)

:: 5. 检查画面比例是否一致
set /a "PROD1=!ORIG_W! * !CURR_TARGET_H!"
set /a "PROD2=!ORIG_H! * !CURR_TARGET_W!"
set /a "DIFF=PROD1 - PROD2"
if !DIFF! LSS 0 set /a "DIFF=-DIFF"
set /a "TOLERANCE=PROD1 / 50"

if !DIFF! GTR !TOLERANCE! (
    echo.
    echo ----------------------------------------------------
    echo [警告] 目标分辨率的宽高比与原视频不一致，画面可能会被拉伸或变形！
    echo ----------------------------------------------------
    choice /c YN /m "是否仍然强制继续处理？(Y=继续, N=跳过本文件)"
    if errorlevel 2 (
        echo [已跳过] 用户取消处理该文件。
        shift
        goto LOOP
    )
)

:: 6. 开始 FFmpeg 转码（最新定制参数）
echo.
echo 正在执行 NVENC AV1 降采样转码，请稍候...
echo ----------------------------------------------------

ffmpeg.exe -hide_banner -y -hwaccel cuda -i "%INPUT_FILE%" -vf "format=yuv420p10le,scale=!CURR_TARGET_W!:!CURR_TARGET_H!:flags=lanczos+accurate_rnd+full_chroma_int,format=p010le" -c:v av1_nvenc -preset p7 -tune uhq -multipass fullres -rc vbr -cq 23 -b:v 0 -spatial-aq 1 -aq-strength 8 -bf 4 -b_ref_mode each -g 240 -tile-columns 0 -tile-rows 0 -split_encode_mode disabled -c:a copy -movflags +faststart "%OUTPUT_FILE%"

if %errorlevel% neq 0 (
    echo.
    echo [转码失败] FFmpeg 返回错误，请检查显卡驱动或显卡是否支持当前参数。
    if exist "%OUTPUT_FILE%" del "%OUTPUT_FILE%"
    shift
    goto LOOP
)

:: 7. 同步修改时间 + 检查文件体积
powershell -NoProfile -Command ^
    "$inPath = [System.Environment]::GetEnvironmentVariable('INPUT_FILE');" ^
    "$outPath = [System.Environment]::GetEnvironmentVariable('OUTPUT_FILE');" ^
    "$in = Get-Item -LiteralPath $inPath;" ^
    "$out = Get-Item -LiteralPath $outPath;" ^
    "$out.LastWriteTime = $in.LastWriteTime;" ^
    "Write-Host '[信息] 已成功同步文件的修改时间。' -ForegroundColor Cyan;" ^
    "if ($out.Length -gt $in.Length) {" ^
    "   $diff = [math]::Round(($out.Length - $in.Length) / 1MB, 2);" ^
    "   Write-Host ('[警告] 输出文件体积变大了 ' + $diff + ' MB！') -ForegroundColor Yellow;" ^
    "} else {" ^
    "   $saved = [math]::Round(($in.Length - $out.Length) / 1MB, 2);" ^
    "   Write-Host ('[完成] 优化成功，节省了 ' + $saved + ' MB。') -ForegroundColor Green;" ^
    "}"

shift
goto LOOP

:END
echo.
echo ========================================================
echo 全部任务已处理完毕！
echo ========================================================
pause
