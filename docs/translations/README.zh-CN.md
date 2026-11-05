<div align="center">

# Osteon

**适用于 Windows 的免费开源 2D 骨骼动画编辑器。**

[English](../../README.md) · [हिन्दी](README.hi.md) · [Español](README.es.md) · 简体中文 · [日本語](README.ja.md) · [Português (BR)](README.pt-BR.md)

</div>

## Osteon 是什么？

Osteon 是一款原生桌面编辑器：在美术素材上创建骨骼层级，把图片或网格绑定到骨骼上，再通过关键帧让 2D 角色动起来。它只是一个小巧的原生可执行文件（C++20、Direct3D 11、Dear ImGui），无需浏览器、额外运行库或账号。

> **受 Spine 启发，而非复制。** Osteon 受 Esoteric Software 的 [Spine](https://esotericsoftware.com) 启发，但**并不打算复刻整个软件**。它是拥有独立代码和独立方向的项目。在合适的情况下，它会读写公开文档化的 Spine JSON 格式。

## 下载

请在 **[Releases 页面](https://github.com/Divyash93/Osteon/releases/latest)** 获取最新版：`OsteonSetup.exe`（安装程序，无需管理员权限）或 `Osteon-0.1.0-win64.zip`（便携版）。系统要求：Windows 10/11（x64）和支持 Direct3D 11 的显卡。目前为早期 alpha 版本（0.1.0）。

## 功能

- **绑定：** 骨骼、插槽、皮肤、绘制顺序，Pose/Create 工具，以及旋转/平移/缩放/切变操作手柄。
- **美术：** 区域、网格、链接网格、边界框、裁剪、路径；支持导入 **PSD**、单张图片或多张图片。
- **权重：** Bind、Auto、Smooth、Prune，笔刷与直接绘制。
- **约束：** IK、变换、路径与物理。
- **动画：** 摄影表、曲线编辑器、自动关键帧、预览。
- **输出：** `.ost`、Spine JSON、PNG 序列、GIF、视频与纹理图集。
- 自动保存与崩溃恢复；所有编辑均可撤销/重做。

## 运行时（即将推出）

用于在游戏中播放动画的官方运行时**即将推出**：Unity、Godot、Unreal Engine、Web（PixiJS、Phaser、WebGL）以及 C/C++ 引擎。目前尚未开发。

## 语言

本 README 提供六种语言版本。编辑器界面目前为英文；**更多界面语言即将支持。**

## 许可证与法律声明

Osteon 采用 **MIT 许可证**（[LICENSE](../../LICENSE)）。第三方代码保留各自的许可证（[THIRD_PARTY_NOTICES.md](../../THIRD_PARTY_NOTICES.md)）。Osteon 是独立项目，与 Esoteric Software 无隶属、背书或赞助关系；“Spine”是 Esoteric Software LLC 的商标。
