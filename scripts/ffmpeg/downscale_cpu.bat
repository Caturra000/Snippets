@echo off
setlocal enabledelayedexpansion
chcp 65001 >nul
title 视频降采样优化工具 (自动横竖屏 CPU libsvtav1)

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

:: 1.5 探测源视频像素格式与颜色范围，用于修正 swscaler 警告
set "PIX_FMT="
set "COLOR_RANGE="
for /f "tokens=1,2 delims=," %%a in ('ffprobe.exe -v error -select_streams v:0 -show_entries stream^=pix_fmt^,color_range -of csv^=p^=0 "%INPUT_FILE%" 2^>nul') do (
    set "PIX_FMT=%%a"
    set "COLOR_RANGE=%%b"
)
if defined PIX_FMT set "PIX_FMT=!PIX_FMT: =!"
if defined COLOR_RANGE set "COLOR_RANGE=!COLOR_RANGE: =!"

:: 默认按 limited range 处理；若源是 yuvj* 或标记 pc/full，则视为 full range
set "IN_RANGE=tv"
echo !PIX_FMT! | findstr /i "yuvj" >nul && set "IN_RANGE=pc"
if /i "!COLOR_RANGE!"=="pc"   set "IN_RANGE=pc"
if /i "!COLOR_RANGE!"=="full" set "IN_RANGE=pc"

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

:: 6. 开始 FFmpeg 转码
echo.
echo 正在执行 libsvtav1 降采样转码...
echo [优化] 已限制 CPU 线程数并降低进程优先级，转码期间可正常进行其他操作。
echo ----------------------------------------------------

:: 获取系统逻辑处理器总数
for /f %%i in ('powershell -NoProfile -Command "[Environment]::ProcessorCount"') do set "TOTAL_THREADS=%%i"
:: 预留 2 个核心给系统日常使用，至少保留 1 个核心用于转码
set /a "MAX_THREADS=!TOTAL_THREADS! - 2"
if !MAX_THREADS! LSS 1 set "MAX_THREADS=1"
echo [信息] 系统共 !TOTAL_THREADS! 个逻辑核心，限制 FFmpeg 最多使用 !MAX_THREADS! 个核心。

:: 使用 start /wait /belownormal 降低优先级防止卡顿断网，并限制最大线程数
start "FFmpeg_Encoding" /wait /belownormal ^
 ffmpeg.exe -hide_banner -y -i "%INPUT_FILE%" ^
 -vf "scale=!CURR_TARGET_W!:!CURR_TARGET_H!:flags=lanczos+accurate_rnd+full_chroma_int+full_chroma_inp:in_range=!IN_RANGE!:out_range=tv,format=yuv420p10le" ^
 -c:v libsvtav1 -preset 5 -crf 28 -g 300 -pix_fmt yuv420p10le -color_range tv ^
 -svtav1-params "tune=0:enable-overlays=1:film-grain=8:film-grain-denoise=0:lp=!MAX_THREADS!" ^
 -threads !MAX_THREADS! ^
 -map 0 -c:a copy -c:s copy -movflags +faststart "%OUTPUT_FILE%"

if %errorlevel% neq 0 (
    echo.
    echo [转码失败] FFmpeg 返回错误，请检查是否支持 libsvtav1 编码器。
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
