@echo off
setlocal enabledelayedexpansion
chcp 65001 >nul
title 视频降采样优化工具 (自动横竖屏 CPU libsvtav1)

set "T0=%TIME: =0%"
:: T0 启动时间戳,结束时统一结算总耗时

:: =============================== 设计说明 ===============================
:: 1 线程警告只是噪音:FFmpeg 通用校验上限 16,libsvtav1 包装层不读 thread_count
::   真正决定并行的是 svt 参数 lp 加进程亲和性,threads 压到 16 只是消警告
:: 2 lp means levels 0-6 since SVT 3.0, old ones mean counts plus pin
::   新版传 lp 16 会报警告后忽略,本脚本按线程数折算等级传,新老都合法
:: 3 挑核用系统原生接口枚举每物理核掩码加能效值,混合与非混合通用
::   P_SMT 是 8 大核 16 线程掩码 FFFF,P_PHY 是每核一线程掩码 5555
::   对比这两档即回答 8 还是 16 划算
::   AUTO 策略:逻辑数够小不绑核,大核线程够只用大核,不够大核优先补小核
::   non-hybrid fills one thread per core, HT if short, no bind over 64

:: =================【用户分辨率配置区域】=================
:: 只需填写常规横屏基准分辨率，遇到竖屏会自动翻转
set "TARGET_W=2560"
set "TARGET_H=1440"
:: =======================================================

:: CPU_MODE: AUTO P_SMT P_PHY SMT PHY E ALL
:: P_SMT = P cores with HT
:: P_PHY = P cores one thread each
:: SMT   = P-first with HT
:: PHY   = P-first one thread each
:: E     = E cores only
:: ALL   = no affinity
set "CPU_MODE=AUTO"
set "THREAD_CAP=16"
:: SVT_LP_MODE 可选 AUTO COUNT LEVEL OFF
set "SVT_LP_MODE=AUTO"
set "SVT_LP_LEVEL=AUTO"
set "PRIORITY=/belownormal"

if %TARGET_W% GTR %TARGET_H% (
    set "BASE_LONG=%TARGET_W%"
    set "BASE_SHORT=%TARGET_H%"
) else (
    set "BASE_LONG=%TARGET_H%"
    set "BASE_SHORT=%TARGET_W%"
)

if /i "%~n0"=="ffmpeg" (
    echo [严重错误] 本脚本文件名不能命名为 ffmpeg.bat
    echo 请将本文件重命名为其他名字后再使用。
    pause
    exit /b
)
if /i "%~n0"=="ffprobe" (
    echo [严重错误] 本脚本文件名不能命名为 ffprobe.bat
    pause
    exit /b
)

where ffmpeg.exe >nul 2>&1
if %errorlevel% neq 0 (
    echo [错误] 未在系统 PATH 中找到 ffmpeg.exe
    pause
    exit /b
)
where ffprobe.exe >nul 2>&1
if %errorlevel% neq 0 (
    echo [错误] 未在系统 PATH 中找到 ffprobe.exe
    pause
    exit /b
)

if "%~1"=="" (
    echo ========================================================
    echo  请将需要处理的视频文件直接拖拽到本批处理文件图标上
    echo  当前基准分辨率: 横屏 %BASE_LONG%x%BASE_SHORT% / 竖屏 %BASE_SHORT%x%BASE_LONG%
    echo ========================================================
    pause
    exit /b
)

set "TOPO_PS1=%TEMP%\ffopt_topo_%RANDOM%%RANDOM%.ps1"
set "SVT_LOG=%TEMP%\ffopt_svt_%RANDOM%%RANDOM%.txt"
set "SVT_VER=%TEMP%\ffopt_ver_%RANDOM%%RANDOM%.txt"

call :INIT_CPU
call :INIT_SVT
call :BUILD_SVT_PARAMS

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

set "PIX_FMT="
set "COLOR_RANGE="
for /f "tokens=1,2 delims=," %%a in ('ffprobe.exe -v error -select_streams v:0 -show_entries stream^=pix_fmt^,color_range -of csv^=p^=0 "%INPUT_FILE%" 2^>nul') do (
    set "PIX_FMT=%%a"
    set "COLOR_RANGE=%%b"
)
if defined PIX_FMT set "PIX_FMT=!PIX_FMT: =!"
if defined COLOR_RANGE set "COLOR_RANGE=!COLOR_RANGE: =!"

set "IN_RANGE=tv"
echo !PIX_FMT! | findstr /i "yuvj" >nul && set "IN_RANGE=pc"
if /i "!COLOR_RANGE!"=="pc"   set "IN_RANGE=pc"
if /i "!COLOR_RANGE!"=="full" set "IN_RANGE=pc"

for /f "tokens=1 delims=," %%r in ('ffprobe.exe -v error -select_streams v:0 -show_entries stream_tags^=rotate:stream_side_data^=rotation -of csv^=p^=0 "%INPUT_FILE%" 2^>nul') do (
    if not "%%r"=="" set "ROT=%%r"
)
if defined ROT set "ROT=%ROT: =%"

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

echo 画面方向    : !ORIENTATION_TEXT! 旋转角度 !ROT!
echo 原画面分辨率: !ORIG_W!x!ORIG_H!
echo 目标分辨率  : !CURR_TARGET_W!x!CURR_TARGET_H!

set "WARN_FLAG=0"
if !CURR_TARGET_W! GTR !ORIG_W! set "WARN_FLAG=1"
if !CURR_TARGET_H! GTR !ORIG_H! set "WARN_FLAG=1"

if "!WARN_FLAG!"=="1" (
    echo.
    echo ----------------------------------------------------
    echo [警告] 目标分辨率大于原片尺寸，将发生放大或插值
    echo ----------------------------------------------------
    choice /c YN /m "是否仍然强制继续处理 Y=继续 N=跳过本文件"
    if errorlevel 2 (
        echo [已跳过] 用户取消处理该文件。
        shift
        goto LOOP
    )
)

set /a "PROD1=!ORIG_W! * !CURR_TARGET_H!"
set /a "PROD2=!ORIG_H! * !CURR_TARGET_W!"
set /a "DIFF=PROD1 - PROD2"
if !DIFF! LSS 0 set /a "DIFF=-DIFF"
set /a "TOLERANCE=PROD1 / 50"

if !DIFF! GTR !TOLERANCE! (
    echo.
    echo ----------------------------------------------------
    echo [警告] 目标分辨率的宽高比与原视频不一致，画面可能被拉伸
    echo ----------------------------------------------------
    choice /c YN /m "是否仍然强制继续处理 Y=继续 N=跳过本文件"
    if errorlevel 2 (
        echo [已跳过] 用户取消处理该文件。
        shift
        goto LOOP
    )
)

echo.
echo 正在执行 libsvtav1 降采样转码...
echo [优化] 已限制 CPU 线程数并降低进程优先级，转码期间可正常进行其他操作。
echo [SCHED] mode !T_MODE_USED! threads !T_THREADS! cpus !T_CPULIST!
echo ----------------------------------------------------

start "FFmpeg_Encoding" /wait %PRIORITY% !AFF_ARG! ^
 ffmpeg.exe -hide_banner -y -filter_threads !FF_THREADS! -i "%INPUT_FILE%" ^
 -vf "scale=!CURR_TARGET_W!:!CURR_TARGET_H!:flags=lanczos+accurate_rnd+full_chroma_int+full_chroma_inp:param0=2:in_range=!IN_RANGE!:out_range=tv,format=yuv420p10le" ^
 -c:v libsvtav1 -preset 5 -crf 28 -g 300 -pix_fmt yuv420p10le -color_range tv ^
 -svtav1-params "tune=0:enable-overlays=1:film-grain=8:film-grain-denoise=0!SVT_EXTRA!" ^
 -threads !FF_THREADS! ^
 -map 0 -c:a copy -c:s copy -movflags +faststart "%OUTPUT_FILE%"

if !errorlevel! neq 0 (
    echo.
    echo [转码失败] FFmpeg 返回错误，请检查是否支持 libsvtav1 编码器。
    if exist "%OUTPUT_FILE%" del "%OUTPUT_FILE%"
    shift
    goto LOOP
)

powershell -NoProfile -Command ^
    "$inPath = [System.Environment]::GetEnvironmentVariable('INPUT_FILE');" ^
    "$outPath = [System.Environment]::GetEnvironmentVariable('OUTPUT_FILE');" ^
    "$in = Get-Item -LiteralPath $inPath;" ^
    "$out = Get-Item -LiteralPath $outPath;" ^
    "$out.LastWriteTime = $in.LastWriteTime;" ^
    "Write-Host '[信息] 已成功同步文件的修改时间。' -ForegroundColor Cyan;" ^
    "if ($out.Length -gt $in.Length) {" ^
    "   $diff = [math]::Round(($out.Length - $in.Length) / 1MB, 2);" ^
    "   Write-Host ('[警告] 输出文件体积变大了 ' + $diff + ' MB') -ForegroundColor Yellow;" ^
    "} else {" ^
    "   $saved = [math]::Round(($in.Length - $out.Length) / 1MB, 2);" ^
    "   Write-Host ('[完成] 优化成功，节省了 ' + $saved + ' MB') -ForegroundColor Green;" ^
    "}"

shift
goto LOOP

:END
set "T1=%TIME: =0%"
set /a "TS0=(1%T0:~0,2%-100)*3600+(1%T0:~3,2%-100)*60+(1%T0:~6,2%-100)"
set /a "TS1=(1%T1:~0,2%-100)*3600+(1%T1:~3,2%-100)*60+(1%T1:~6,2%-100)"
set /a "TC0=1%T0:~9,2%-100"
set /a "TC1=1%T1:~9,2%-100"
set /a "ELAPSED=(TS1-TS0)*100+TC1-TC0"
if !ELAPSED! LSS 0 set /a "ELAPSED+=8640000"
set /a "EH=ELAPSED/360000"
set /a "EM=ELAPSED%%360000/6000"
set /a "ES=ELAPSED%%6000/100"
if !EM! LSS 10 set "EM=0!EM!"
if !ES! LSS 10 set "ES=0!ES!"
echo 本次共耗时: !EH!小时!EM!分!ES!秒
if exist "%TOPO_PS1%" del "%TOPO_PS1%" >nul 2>&1
if exist "%SVT_LOG%" del "%SVT_LOG%" >nul 2>&1
if exist "%SVT_VER%" del "%SVT_VER%" >nul 2>&1
echo.
echo ========================================================
echo 全部任务已处理完毕
echo ========================================================
pause
exit /b

:INIT_CPU
if exist "%TOPO_PS1%" del "%TOPO_PS1%" >nul 2>&1
call :WRITE_TOPO_PS1
set "T_ERROR="
set "T_LP_TOTAL="
set "T_CORE_TOTAL=0"
set "T_HYBRID=0"
set "T_PCORE_COUNT=0"
set "T_PCORE_LP=0"
set "T_ECORE_COUNT=0"
set "T_ECORE_LP=0"
set "T_MULTIGROUP=0"
set "T_MODE_USED="
set "T_THREADS="
set "T_AFFINITY=NONE"
set "T_CPULIST=ALL"
for /f "usebackq tokens=1,2 delims==" %%a in (`powershell -NoProfile -ExecutionPolicy Bypass -File "%TOPO_PS1%" %THREAD_CAP% %CPU_MODE%`) do set "T_%%a=%%b"
if defined T_ERROR goto :CPU_FALLBACK
if not defined T_THREADS goto :CPU_FALLBACK
if not defined T_LP_TOTAL goto :CPU_FALLBACK
goto :CPU_DONE

:CPU_FALLBACK
set "T_MODE_USED=FALLBACK"
set "T_AFFINITY=NONE"
set "T_CPULIST=ALL"
set "T_LP_TOTAL="
for /f %%i in ('powershell -NoProfile -Command "[Environment]::ProcessorCount"') do set "T_LP_TOTAL=%%i"
if not defined T_LP_TOTAL set "T_LP_TOTAL=%NUMBER_OF_PROCESSORS%"
set /a "T_THREADS=T_LP_TOTAL"
if !T_THREADS! GTR %THREAD_CAP% set "T_THREADS=%THREAD_CAP%"
if !T_THREADS! LSS 1 set "T_THREADS=1"

:CPU_DONE
set "MODE_TEXT=未知模式"
if /i "!T_MODE_USED!"=="ALL"      set "MODE_TEXT=不限制核心"
if /i "!T_MODE_USED!"=="P_SMT"    set "MODE_TEXT=仅大核心并包含超线程"
if /i "!T_MODE_USED!"=="P_PHY"    set "MODE_TEXT=仅大核心每核一个线程"
if /i "!T_MODE_USED!"=="SMT"      set "MODE_TEXT=大核优先并包含超线程"
if /i "!T_MODE_USED!"=="PHY"      set "MODE_TEXT=大核优先每核一个线程"
if /i "!T_MODE_USED!"=="E"        set "MODE_TEXT=仅小核心"
if /i "!T_MODE_USED!"=="FALLBACK" set "MODE_TEXT=拓扑探测失败并回退"

set "AFF_ARG="
if /i not "!T_AFFINITY!"=="NONE" set "AFF_ARG=/affinity !T_AFFINITY!"

set "FF_THREADS=!T_THREADS!"
if !FF_THREADS! GTR 16 set "FF_THREADS=16"
if !FF_THREADS! LSS 1 set "FF_THREADS=1"

set "LP_LEVEL=5"
if !T_THREADS! LEQ 15 set "LP_LEVEL=4"
if !T_THREADS! LEQ 7 set "LP_LEVEL=3"
if !T_THREADS! LEQ 3 set "LP_LEVEL=2"
if !T_THREADS! LEQ 1 set "LP_LEVEL=1"
if /i not "%SVT_LP_LEVEL%"=="AUTO" set "LP_LEVEL=%SVT_LP_LEVEL%"

echo ========================================================
echo [CPU] 逻辑处理器 !T_LP_TOTAL! 个  物理核心 !T_CORE_TOTAL! 个
echo [CPU] P cores !T_PCORE_COUNT! (!T_PCORE_LP!T)  E cores !T_ECORE_COUNT! (!T_ECORE_LP!T)
echo [CPU] 调度模式 !T_MODE_USED! - !MODE_TEXT!
echo [CPU] 实际线程 !T_THREADS!  亲和掩码 !T_AFFINITY!  绑定 CPU !T_CPULIST!
goto :eof

:INIT_SVT
:: 用黑场试跑直接测语义,横幅在重定向下可能缺失所以不用它
set "SVT_HAS_LEVEL=0"
set "PROBE_RC=1"
if exist "%SVT_LOG%" del "%SVT_LOG%" >nul 2>&1
ffmpeg.exe -hide_banner -loglevel info -y -f lavfi -i color=c=black:s=64x64:r=5:d=1 -c:v libsvtav1 -preset 12 -svtav1-params "tune=0:lp=!T_THREADS!" -f null NUL >"%SVT_LOG%" 2>&1
if errorlevel 1 (
    set "PROBE_RC=1"
) else (
    set "PROBE_RC=0"
)
findstr /i /c:"supports levels" "%SVT_LOG%" >nul 2>&1
if errorlevel 1 (
    set "SVT_HAS_LEVEL=0"
) else (
    set "SVT_HAS_LEVEL=1"
)
if "!PROBE_RC!"=="1" echo [SVT] probe failed, no lp
if "!PROBE_RC!"=="1" goto :SVT_PROBE_TAIL
if "!SVT_HAS_LEVEL!"=="1" echo [SVT] probe ok, level mode
if "!SVT_HAS_LEVEL!"=="0" echo [SVT] probe ok, count mode
:SVT_PROBE_TAIL
echo ========================================================
goto :eof

:BUILD_SVT_PARAMS
set "SVT_EXTRA="
set "SVT_LP_APPLIED=OFF"
if /i "%SVT_LP_MODE%"=="OFF" goto :SVT_PARAMS_DONE
if /i "%SVT_LP_MODE%"=="COUNT" (
    set "SVT_EXTRA=:lp=!T_THREADS!"
    set "SVT_LP_APPLIED=COUNT"
    goto :SVT_PARAMS_DONE
)
if /i "%SVT_LP_MODE%"=="LEVEL" (
    set "SVT_EXTRA=:lp=!LP_LEVEL!"
    set "SVT_LP_APPLIED=LEVEL"
    goto :SVT_PARAMS_DONE
)
if !PROBE_RC! NEQ 0 (
    set "SVT_LP_APPLIED=OFF"
    goto :SVT_PARAMS_DONE
)
if "!SVT_HAS_LEVEL!"=="1" (
    set "SVT_EXTRA=:lp=!LP_LEVEL!"
    set "SVT_LP_APPLIED=LEVEL"
    goto :SVT_PARAMS_DONE
)
set "SVT_EXTRA=:lp=!T_THREADS!"
set "SVT_LP_APPLIED=COUNT"

:SVT_PARAMS_DONE
if /i "!SVT_LP_APPLIED!"=="LEVEL" echo [SVT] lp 按并行度等级传递 等级 !LP_LEVEL!
if /i "!SVT_LP_APPLIED!"=="COUNT" echo [SVT] lp 按逻辑核心数传递 数量 !T_THREADS!
if /i "!SVT_LP_APPLIED!"=="OFF"   echo [SVT] 不传递 lp 参数
goto :eof

:WRITE_TOPO_PS1
>"%TOPO_PS1%" echo trap { Write-Output 'ERROR=1'; exit 1 }
>>"%TOPO_PS1%" echo $ErrorActionPreference = 'Stop'
>>"%TOPO_PS1%" echo $cap = 16
>>"%TOPO_PS1%" echo if ($args.Count -ge 1) { $cap = [int]$args[0] }
>>"%TOPO_PS1%" echo if ($cap -lt 1) { $cap = 1 }
>>"%TOPO_PS1%" echo $mode = 'AUTO'
>>"%TOPO_PS1%" echo if ($args.Count -ge 2) { $mode = [string]$args[1] }
>>"%TOPO_PS1%" echo $mode = $mode.ToUpper()
>>"%TOPO_PS1%" echo $code = @'
>>"%TOPO_PS1%" echo using System;
>>"%TOPO_PS1%" echo using System.Runtime.InteropServices;
>>"%TOPO_PS1%" echo public static class FfoTopo {
>>"%TOPO_PS1%" echo [DllImport("kernel32.dll", SetLastError = true)]
>>"%TOPO_PS1%" echo public static extern bool GetLogicalProcessorInformationEx(int rel, byte[] buf, ref int len);
>>"%TOPO_PS1%" echo }
>>"%TOPO_PS1%" echo '@
>>"%TOPO_PS1%" echo if (-not ('FfoTopo' -as [type])) { Add-Type -TypeDefinition $code }
>>"%TOPO_PS1%" echo $len = 0
>>"%TOPO_PS1%" echo [void][FfoTopo]::GetLogicalProcessorInformationEx(0, $null, [ref]$len)
>>"%TOPO_PS1%" echo if ($len -le 0) { Write-Output 'ERROR=1'; exit 1 }
>>"%TOPO_PS1%" echo $buf = New-Object byte[] $len
>>"%TOPO_PS1%" echo $ok = [FfoTopo]::GetLogicalProcessorInformationEx(0, $buf, [ref]$len)
>>"%TOPO_PS1%" echo if (-not $ok) { Write-Output 'ERROR=1'; exit 1 }
>>"%TOPO_PS1%" echo $cores = New-Object System.Collections.ArrayList
>>"%TOPO_PS1%" echo $multi = 0
>>"%TOPO_PS1%" echo $off = 0
>>"%TOPO_PS1%" echo while ($off -lt $len) {
>>"%TOPO_PS1%" echo $size = [BitConverter]::ToInt32($buf, $off + 4)
>>"%TOPO_PS1%" echo if ($size -le 0) { break }
>>"%TOPO_PS1%" echo if ([BitConverter]::ToInt32($buf, $off) -eq 0) {
>>"%TOPO_PS1%" echo $eff = [int]$buf[$off + 9]
>>"%TOPO_PS1%" echo $grp = [int][BitConverter]::ToUInt16($buf, $off + 40)
>>"%TOPO_PS1%" echo $mask = [BitConverter]::ToUInt64($buf, $off + 32)
>>"%TOPO_PS1%" echo if ($grp -ne 0) { $multi = 1 }
>>"%TOPO_PS1%" echo $bits = New-Object System.Collections.ArrayList
>>"%TOPO_PS1%" echo if ($mask -gt [uint64]9223372036854775807) { $multi = 1 }
>>"%TOPO_PS1%" echo else {
>>"%TOPO_PS1%" echo $bin = [Convert]::ToString([int64]$mask, 2)
>>"%TOPO_PS1%" echo for ($i = 0; $i -lt $bin.Length; $i++) {
>>"%TOPO_PS1%" echo if ($bin.Substring($bin.Length - 1 - $i, 1) -eq '1') { [void]$bits.Add($i) }
>>"%TOPO_PS1%" echo }
>>"%TOPO_PS1%" echo }
>>"%TOPO_PS1%" echo $item = New-Object psobject
>>"%TOPO_PS1%" echo Add-Member -InputObject $item -MemberType NoteProperty -Name Eff -Value $eff
>>"%TOPO_PS1%" echo Add-Member -InputObject $item -MemberType NoteProperty -Name Bits -Value $bits
>>"%TOPO_PS1%" echo [void]$cores.Add($item)
>>"%TOPO_PS1%" echo }
>>"%TOPO_PS1%" echo $off = $off + $size
>>"%TOPO_PS1%" echo }
>>"%TOPO_PS1%" echo $coreTotal = $cores.Count
>>"%TOPO_PS1%" echo if ($coreTotal -eq 0) { Write-Output 'ERROR=1'; exit 1 }
>>"%TOPO_PS1%" echo $lpAll = 0
>>"%TOPO_PS1%" echo foreach ($c in $cores) { $lpAll = $lpAll + $c.Bits.Count }
>>"%TOPO_PS1%" echo if ($multi -eq 1) { $lpAll = [int][Environment]::ProcessorCount }
>>"%TOPO_PS1%" echo if ($lpAll -lt 1) { $lpAll = 1 }
>>"%TOPO_PS1%" echo $effs = New-Object System.Collections.ArrayList
>>"%TOPO_PS1%" echo foreach ($c in $cores) { if (-not $effs.Contains($c.Eff)) { [void]$effs.Add($c.Eff) } }
>>"%TOPO_PS1%" echo for ($i = 0; $i -lt $effs.Count; $i++) {
>>"%TOPO_PS1%" echo for ($j = $i + 1; $j -lt $effs.Count; $j++) {
>>"%TOPO_PS1%" echo if ($effs[$j] -gt $effs[$i]) { $tmp = $effs[$i]; $effs[$i] = $effs[$j]; $effs[$j] = $tmp }
>>"%TOPO_PS1%" echo }
>>"%TOPO_PS1%" echo }
>>"%TOPO_PS1%" echo $hybrid = 0
>>"%TOPO_PS1%" echo if ($effs.Count -gt 1) { $hybrid = 1 }
>>"%TOPO_PS1%" echo $perf = New-Object System.Collections.ArrayList
>>"%TOPO_PS1%" echo $slow = New-Object System.Collections.ArrayList
>>"%TOPO_PS1%" echo $ordered = New-Object System.Collections.ArrayList
>>"%TOPO_PS1%" echo foreach ($e in $effs) { foreach ($c in $cores) { if ($c.Eff -eq $e) { [void]$ordered.Add($c) } } }
>>"%TOPO_PS1%" echo foreach ($c in $cores) { if ($c.Eff -eq $effs[0]) { [void]$perf.Add($c) } }
>>"%TOPO_PS1%" echo foreach ($c in $cores) { if ($c.Eff -eq $effs[$effs.Count - 1]) { [void]$slow.Add($c) } }
>>"%TOPO_PS1%" echo $perfLp = 0
>>"%TOPO_PS1%" echo foreach ($c in $perf) { $perfLp = $perfLp + $c.Bits.Count }
>>"%TOPO_PS1%" echo $slowLp = 0
>>"%TOPO_PS1%" echo foreach ($c in $slow) { $slowLp = $slowLp + $c.Bits.Count }
>>"%TOPO_PS1%" echo $eCount = 0
>>"%TOPO_PS1%" echo $eLp = 0
>>"%TOPO_PS1%" echo if ($hybrid -eq 1) { $eCount = $slow.Count; $eLp = $slowLp }
>>"%TOPO_PS1%" echo if ($mode -eq 'AUTO') {
>>"%TOPO_PS1%" echo if ($lpAll -le $cap) { $mode = 'ALL' }
>>"%TOPO_PS1%" echo elseif ($hybrid -eq 1) {
>>"%TOPO_PS1%" echo if ($perfLp -ge $cap) { $mode = 'P_SMT' } else { $mode = 'SMT' }
>>"%TOPO_PS1%" echo }
>>"%TOPO_PS1%" echo else {
>>"%TOPO_PS1%" echo if ($coreTotal -ge $cap) { $mode = 'PHY' } else { $mode = 'SMT' }
>>"%TOPO_PS1%" echo }
>>"%TOPO_PS1%" echo }
>>"%TOPO_PS1%" echo if (($mode -eq 'E') -and ($hybrid -eq 0)) { $mode = 'SMT' }
>>"%TOPO_PS1%" echo if ($perf.Count -lt 1) { $mode = 'ALL' }
>>"%TOPO_PS1%" echo if ($multi -eq 1) { $mode = 'ALL' }
>>"%TOPO_PS1%" echo $list = $ordered
>>"%TOPO_PS1%" echo $one = 0
>>"%TOPO_PS1%" echo if ($mode -eq 'P_SMT') { $list = $perf }
>>"%TOPO_PS1%" echo if ($mode -eq 'P_PHY') { $list = $perf; $one = 1 }
>>"%TOPO_PS1%" echo if ($mode -eq 'PHY') { $one = 1 }
>>"%TOPO_PS1%" echo if ($mode -eq 'E') { $list = $slow }
>>"%TOPO_PS1%" echo $sel = New-Object System.Collections.ArrayList
>>"%TOPO_PS1%" echo if ($mode -ne 'ALL') {
>>"%TOPO_PS1%" echo foreach ($c in $list) {
>>"%TOPO_PS1%" echo if ($sel.Count -ge $cap) { break }
>>"%TOPO_PS1%" echo if ($c.Bits.Count -lt 1) { continue }
>>"%TOPO_PS1%" echo if ($one -eq 1) { [void]$sel.Add($c.Bits[0]) }
>>"%TOPO_PS1%" echo else { foreach ($b in $c.Bits) { if ($sel.Count -lt $cap) { [void]$sel.Add($b) } } }
>>"%TOPO_PS1%" echo }
>>"%TOPO_PS1%" echo }
>>"%TOPO_PS1%" echo $threads = $sel.Count
>>"%TOPO_PS1%" echo if ($mode -eq 'ALL') {
>>"%TOPO_PS1%" echo $threads = $lpAll
>>"%TOPO_PS1%" echo if ($threads -gt $cap) { $threads = $cap }
>>"%TOPO_PS1%" echo }
>>"%TOPO_PS1%" echo if ($threads -lt 1) { $threads = 1 }
>>"%TOPO_PS1%" echo $acc = [uint64]0
>>"%TOPO_PS1%" echo foreach ($b in $sel) {
>>"%TOPO_PS1%" echo $v = [uint64]1
>>"%TOPO_PS1%" echo for ($k = 0; $k -lt $b; $k++) { $v = $v * [uint64]2 }
>>"%TOPO_PS1%" echo $acc = $acc + $v
>>"%TOPO_PS1%" echo }
>>"%TOPO_PS1%" echo $hex = 'NONE'
>>"%TOPO_PS1%" echo if (($mode -ne 'ALL') -and ($sel.Count -gt 0)) { $hex = '{0:X}' -f [uint64]$acc }
>>"%TOPO_PS1%" echo $arr = @()
>>"%TOPO_PS1%" echo foreach ($b in $sel) { $arr = $arr + [int]$b }
>>"%TOPO_PS1%" echo for ($i = 0; $i -lt $arr.Count; $i++) {
>>"%TOPO_PS1%" echo for ($j = $i + 1; $j -lt $arr.Count; $j++) {
>>"%TOPO_PS1%" echo if ($arr[$j] -lt $arr[$i]) { $tmp = $arr[$i]; $arr[$i] = $arr[$j]; $arr[$j] = $tmp }
>>"%TOPO_PS1%" echo }
>>"%TOPO_PS1%" echo }
>>"%TOPO_PS1%" echo $txt = 'ALL'
>>"%TOPO_PS1%" echo if ($arr.Count -gt 0) {
>>"%TOPO_PS1%" echo $txt = ''
>>"%TOPO_PS1%" echo $k = 0
>>"%TOPO_PS1%" echo while ($k -lt $arr.Count) {
>>"%TOPO_PS1%" echo $s = $arr[$k]
>>"%TOPO_PS1%" echo $t2 = $s
>>"%TOPO_PS1%" echo while ((($k + 1) -lt $arr.Count) -and ($arr[$k + 1] -eq ($t2 + 1))) { $k = $k + 1; $t2 = $arr[$k] }
>>"%TOPO_PS1%" echo if ($txt -ne '') { $txt = $txt + ',' }
>>"%TOPO_PS1%" echo if ($s -eq $t2) { $txt = $txt + [string]$s }
>>"%TOPO_PS1%" echo else { $txt = $txt + [string]$s + '-' + [string]$t2 }
>>"%TOPO_PS1%" echo $k = $k + 1
>>"%TOPO_PS1%" echo }
>>"%TOPO_PS1%" echo }
>>"%TOPO_PS1%" echo Write-Output ('LP_TOTAL=' + $lpAll)
>>"%TOPO_PS1%" echo Write-Output ('CORE_TOTAL=' + $coreTotal)
>>"%TOPO_PS1%" echo Write-Output ('HYBRID=' + $hybrid)
>>"%TOPO_PS1%" echo Write-Output ('PCORE_COUNT=' + $perf.Count)
>>"%TOPO_PS1%" echo Write-Output ('PCORE_LP=' + $perfLp)
>>"%TOPO_PS1%" echo Write-Output ('ECORE_COUNT=' + $eCount)
>>"%TOPO_PS1%" echo Write-Output ('ECORE_LP=' + $eLp)
>>"%TOPO_PS1%" echo Write-Output ('MULTIGROUP=' + $multi)
>>"%TOPO_PS1%" echo Write-Output ('MODE_USED=' + $mode)
>>"%TOPO_PS1%" echo Write-Output ('THREADS=' + $threads)
>>"%TOPO_PS1%" echo Write-Output ('AFFINITY=' + $hex)
>>"%TOPO_PS1%" echo Write-Output ('CPULIST=' + $txt)
goto :eof
