<p align="center">
<img src="logo.png" width="376" height="128" alt="Winlator Logo" />
</p>

# Winlator

[English](README.md) | [简体中文](README_cn.md)

Winlator 是一款 Android 应用程序，让你能够通过 Wine 和 Box86/Box64 运行 Windows (x86_64) 应用程序。

# 安装

1. 从 [GitHub Releases](https://github.com/brunodev85/winlator/releases) 下载并安装 APK (Winlator_10.1.apk)
2. 启动应用程序并等待安装过程完成

----

[![Play on Youtube](https://img.youtube.com/vi/ETYDgKz4jBQ/3.jpg)](https://www.youtube.com/watch?v=ETYDgKz4jBQ)
[![Play on Youtube](https://img.youtube.com/vi/9E4wnKf2OsI/2.jpg)](https://www.youtube.com/watch?v=9E4wnKf2OsI)
[![Play on Youtube](https://img.youtube.com/vi/czEn4uT3Ja8/2.jpg)](https://www.youtube.com/watch?v=czEn4uT3Ja8)
[![Play on Youtube](https://img.youtube.com/vi/eD36nxfT_Z0/2.jpg)](https://www.youtube.com/watch?v=eD36nxfT_Z0)

----

# 实用技巧

- 如果你遇到性能问题，请尝试在 Container Settings -> Advanced Tab 中将 Box64 预设更改为 `Performance`。
- 对于使用 .NET Framework 的应用程序，请尝试安装位于 Start Menu -> System Tools -> Installers 的 `Wine Mono`。
- 如果一些老游戏无法打开，请尝试在 Container Settings -> Environment Variables 中添加环境变量 `MESA_EXTENSION_MAX_YEAR=2003`。
- 尝试使用 Winlator 主屏幕上的快捷方式运行游戏，你可以在那里为每个游戏定义单独的设置。
- 为了正确显示低分辨率游戏，请尝试在快捷方式设置中启用 `Force Fullscreen` 选项。
- 为了提高使用 Unity Engine 游戏的稳定性，请尝试将 Box64 预设更改为 `Stability`，或者在快捷方式设置中添加 exec 参数 `-force-gfx-direct`。

# 信息

该项目自 1.0 版本以来一直在持续开发中，当前的应用源代码已更新至 7.1 版本。为了防止在 Winlator 官方发布之前出现非官方版本，我不经常更新此仓库。

# 致谢与第三方应用

- GLIBC Patches 作者 [Termux Pacman](https://github.com/termux-pacman/glibc-packages)
- Wine ([winehq.org](https://www.winehq.org/))
- Box86/Box64 作者 [ptitseb](https://github.com/ptitSeb)
- Mesa (Turnip/Zink/VirGL) ([mesa3d.org](https://www.mesa3d.org))
- DXVK ([github.com/doitsujin/dxvk](https://github.com/doitsujin/dxvk))
- VKD3D ([gitlab.winehq.org/wine/vkd3d](https://gitlab.winehq.org/wine/vkd3d))
- CNC DDraw ([github.com/FunkyFr3sh/cnc-ddraw](https://github.com/FunkyFr3sh/cnc-ddraw))

特别感谢参与这些项目的所有开发人员。<br>
感谢所有相信这个项目的人。
