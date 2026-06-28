用于 4k 截图降采样到 1080p 的 SSIM 斗蛐蛐脚本，把截图拖到 bat 文件里即可执行。

部分文件要求自行补全，二进制我不放进仓库里：

```
├── mpv.exe
├── opus47_downscale_ssim.bat
├── opus47_downscale_ssim.ps1
├── oxipng.exe
├── scripts
│   └── auto_screenshot.lua
└── shaders
    └── SSimDownscaler.glsl
```

另外我觉得直接使用 MKS2021 应该算是足够好的了，因为 SSIM 指标不是万能的。