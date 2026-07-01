param([string]$ImagePath)

if (-not $ImagePath -or -not (Test-Path $ImagePath)) { exit }

# ── 工具路径 ──────────────────────────────────────
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$Oxipng    = Join-Path $ScriptDir 'oxipng.exe'
$Mpv       = Join-Path $ScriptDir 'mpv.exe'
$Shader    = Join-Path $ScriptDir 'shaders\SSimDownscaler.glsl'
$LuaScript = Join-Path $ScriptDir 'scripts\auto_screenshot.lua'

function Invoke-Oxipng {
    if (Test-Path $Oxipng) {
        & $Oxipng -o 3 --strip safe $ImagePath 2>$null | Out-Null
    }
}

# ── 临时关闭用 ────────────────────────────────────
# Invoke-Oxipng
# exit

# ── 检测分辨率 ────────────────────────────────────
$size = & magick identify -ping -format "%wx%h" "${ImagePath}[0]" 2>$null
if (-not $size) {
    $isBroken = $true
}

$w, $h = ($size | Select-Object -First 1) -split 'x'
$is4K = ($w -eq '3840' -and $h -eq '2160')

# ── DPI 修正 + 隐形宿主窗口（防止 mpv 闪烁） ──────
if (-not ('MpvHost' -as [type])) {
    Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;

public class DpiFix {
    [DllImport("user32.dll", SetLastError=true)]
    private static extern bool SetProcessDpiAwarenessContext(IntPtr dpiContext);

    [DllImport("user32.dll", SetLastError=true)]
    private static extern bool SetProcessDPIAware();

    public static void Enable() {
        try {
            if (SetProcessDpiAwarenessContext(new IntPtr(-4))) return;
        } catch {}
        try {
            if (SetProcessDpiAwarenessContext(new IntPtr(-2))) return;
        } catch {}
        try {
            SetProcessDPIAware();
        } catch {}
    }
}

public class MpvHost {
    [DllImport("user32.dll")]
    public static extern IntPtr CreateWindowEx(
        uint exStyle, string cls, string title, uint style,
        int x, int y, int w, int h,
        IntPtr parent, IntPtr menu, IntPtr inst, IntPtr param);

    [DllImport("user32.dll")]
    public static extern bool SetLayeredWindowAttributes(IntPtr hwnd, uint key, byte alpha, uint flags);

    [DllImport("user32.dll")]
    public static extern bool ShowWindow(IntPtr hwnd, int cmd);

    [DllImport("user32.dll")]
    public static extern bool DestroyWindow(IntPtr hwnd);

    public static IntPtr Create(int w, int h) {
        uint exStyle = 0x00080000 | 0x00000080 | 0x00000020 | 0x08000000;
        uint style = 0x80000000;

        IntPtr p = CreateWindowEx(
            exStyle,
            "STATIC",
            "",
            style,
            0, 0, w, h,
            IntPtr.Zero,
            IntPtr.Zero,
            IntPtr.Zero,
            IntPtr.Zero
        );

        if (p != IntPtr.Zero) {
            SetLayeredWindowAttributes(p, 0, 0, 2);
            ShowWindow(p, 4);
        }

        return p;
    }
}
"@
}

# ── PNG chunk 修复器 ──────────────────────────────
if (-not ('PngMeta' -as [type])) {
    Add-Type -TypeDefinition @"
using System;
using System.IO;
using System.Text;

public class PngMeta {
    private static readonly byte[] Sig = new byte[] { 137, 80, 78, 71, 13, 10, 26, 10 };
    private static readonly uint[] CrcTable = MakeCrcTable();

    private static uint[] MakeCrcTable() {
        uint[] table = new uint[256];

        for (uint n = 0; n < 256; n++) {
            uint c = n;
            for (int k = 0; k < 8; k++) {
                if ((c & 1) != 0) {
                    c = 0xEDB88320u ^ (c >> 1);
                }
                else {
                    c = c >> 1;
                }
            }
            table[n] = c;
        }

        return table;
    }

    private static uint ReadBE32(byte[] b, int o) {
        return
            ((uint)b[o] << 24) |
            ((uint)b[o + 1] << 16) |
            ((uint)b[o + 2] << 8) |
            ((uint)b[o + 3]);
    }

    private static void WriteBE32(Stream s, uint v) {
        s.WriteByte((byte)((v >> 24) & 0xFF));
        s.WriteByte((byte)((v >> 16) & 0xFF));
        s.WriteByte((byte)((v >> 8) & 0xFF));
        s.WriteByte((byte)(v & 0xFF));
    }

    private static void WriteBE32To(byte[] b, int o, uint v) {
        b[o]     = (byte)((v >> 24) & 0xFF);
        b[o + 1] = (byte)((v >> 16) & 0xFF);
        b[o + 2] = (byte)((v >> 8) & 0xFF);
        b[o + 3] = (byte)(v & 0xFF);
    }

    private static uint UpdateCrc(uint c, byte[] data, int offset, int count) {
        for (int i = offset; i < offset + count; i++) {
            c = CrcTable[(int)((c ^ data[i]) & 0xFF)] ^ (c >> 8);
        }
        return c;
    }

    private static void WriteChunk(Stream s, string type, byte[] data) {
        byte[] t = Encoding.ASCII.GetBytes(type);
        if (t.Length != 4) {
            throw new ArgumentException("PNG chunk type must be 4 bytes.");
        }

        WriteBE32(s, (uint)data.Length);
        s.Write(t, 0, 4);

        if (data.Length > 0) {
            s.Write(data, 0, data.Length);
        }

        uint c = 0xFFFFFFFFu;
        c = UpdateCrc(c, t, 0, 4);
        c = UpdateCrc(c, data, 0, data.Length);
        c ^= 0xFFFFFFFFu;

        WriteBE32(s, c);
    }

    public static void Fix(string path, uint xppm, uint yppm, byte unit, byte srgbIntent) {
        if (xppm == 0 || yppm == 0) {
            throw new ArgumentException("pHYs x/y must be non-zero for this workflow.");
        }

        if (unit != 0 && unit != 1) {
            throw new ArgumentException("pHYs unit must be 0 or 1.");
        }

        if (srgbIntent > 3) {
            throw new ArgumentException("sRGB rendering intent must be 0..3.");
        }

        byte[] input = File.ReadAllBytes(path);

        if (input.Length < 33) {
            throw new InvalidDataException("File is too small to be a valid PNG.");
        }

        for (int i = 0; i < Sig.Length; i++) {
            if (input[i] != Sig[i]) {
                throw new InvalidDataException("Not a PNG file.");
            }
        }

        using (MemoryStream output = new MemoryStream(input.Length + 64)) {
            output.Write(Sig, 0, Sig.Length);

            int pos = 8;
            bool inserted = false;
            bool sawIEND = false;

            while (pos + 12 <= input.Length) {
                int chunkStart = pos;

                uint lenU = ReadBE32(input, pos);
                if (lenU > Int32.MaxValue) {
                    throw new InvalidDataException("PNG chunk too large.");
                }

                int len = (int)lenU;
                pos += 4;

                if (pos + 4 + len + 4 > input.Length) {
                    throw new InvalidDataException("Truncated PNG chunk.");
                }

                string type = Encoding.ASCII.GetString(input, pos, 4);
                pos += 4;

                int dataStart = pos;
                pos += len;

                int crcStart = pos;
                pos += 4;

                if (type == "pHYs" || type == "sRGB") {
                    continue;
                }

                output.Write(input, chunkStart, pos - chunkStart);

                if (type == "IHDR" && !inserted) {
                    WriteChunk(output, "sRGB", new byte[] { srgbIntent });

                    byte[] phys = new byte[9];
                    WriteBE32To(phys, 0, xppm);
                    WriteBE32To(phys, 4, yppm);
                    phys[8] = unit;

                    WriteChunk(output, "pHYs", phys);

                    inserted = true;
                }

                if (type == "IEND") {
                    sawIEND = true;
                    break;
                }
            }

            if (!inserted) {
                throw new InvalidDataException("IHDR not found.");
            }

            if (!sawIEND) {
                throw new InvalidDataException("IEND not found.");
            }

            File.WriteAllBytes(path, output.ToArray());
        }
    }
}
"@
}

function Repair-PngMetadata {
    param([Parameter(Mandatory)][string]$Path)

    [PngMeta]::Fix($Path, [uint32]3780, [uint32]3780, [byte]1, [byte]0)
}

if (-not $is4K -or $isBroken) {
    Repair-PngMetadata $ImagePath
    Invoke-Oxipng
    exit
}

[DpiFix]::Enable()

$tmp  = Join-Path (Split-Path $ImagePath -Parent) ([IO.Path]::GetRandomFileName() + '.png')
$hwnd = [IntPtr]::Zero

try {
    $hwnd = [MpvHost]::Create(1920, 1080)

    $mpvArgs = @(
        "`"$ImagePath`""
        '--no-config', '--idle=no', '--force-window=yes'
        '--vo=gpu-next', '--gpu-api=vulkan'
        '--no-hidpi-window-scale', '--osd-level=0'
        '--pause=yes', '--hr-seek=yes', '--keep-open=yes'
        '--deband=no', '--dither-depth=no'
        '--correct-downscaling=yes', '--linear-downscaling=no', '--sigmoid-upscaling=no'
        '--dscale=catmull_rom'
        "--glsl-shader=`"$Shader`""
        '--screenshot-format=png'
        '--screenshot-png-compression=0', '--screenshot-png-filter=0'
        '--screenshot-high-bit-depth=no'
        '--screenshot-tag-colorspace=yes'
        "--script=`"$LuaScript`""
        "--script-opts=output_path=`"$tmp`""
        '--msg-level=all=warn'
    )

    if ($hwnd -ne [IntPtr]::Zero) {
        $mpvArgs += "--wid=$($hwnd.ToInt64())"
    }

    Start-Process -FilePath $Mpv -ArgumentList $mpvArgs -Wait -WindowStyle Hidden
}
finally {
    if ($hwnd -ne [IntPtr]::Zero) {
        [MpvHost]::DestroyWindow($hwnd) | Out-Null
    }
}

if (Test-Path $tmp) {
    $outSize = & magick identify -ping -format "%wx%h" "${tmp}[0]" 2>$null
    $outSize = ($outSize | Select-Object -First 1)

    if ($outSize -eq '1920x1080') {
        try {
            Repair-PngMetadata $tmp

            Move-Item $tmp $ImagePath -Force
            Invoke-Oxipng
        }
        catch {
            Write-Warning "PNG metadata repair failed: $($_.Exception.Message). Keeping original 4K file."
            Remove-Item $tmp -Force -ErrorAction SilentlyContinue
            Invoke-Oxipng
        }
    }
    else {
        Write-Warning "Downscale output size mismatch: expected 1920x1080, got $outSize. Keeping original 4K file."
        Remove-Item $tmp -Force -ErrorAction SilentlyContinue
        Invoke-Oxipng
    }
}
else {
    Invoke-Oxipng
}