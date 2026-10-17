# vivo Miuix Shader Fix

修复 vivo 上 LSPosed Miuix 寄生管理器在深色模式中的 RuntimeShader 复制问题，涉及圆角裁剪和玻璃效果。

通过 Zygisk 仅在 `com.android.shell`、UID appId 2000 的进程中加载。RuntimeShader 的复制返回原对象，其他 Shader 继续调用原实现。LSPosed 的 APK、证书和许可证不作修改。

## 使用

需要 ARM64、Zygisk 和 root 模块管理器。安装 ZIP，按 Zygisk 要求重新加载后，再启动寄生管理器。禁用模块后，同样需要结束已注入进程或重启来撤销内存中的 hook。

主要环境为 iQOO 15 / OriginOS 6。四份固件的 ART 与 libcore 样本做过离线检查；未知布局会拒绝初始化，不能保证未来所有 ART 版本都兼容。

## 构建

Windows、Python 3、JDK 21、Android SDK build-tools 36.0.0、NDK 29、CMake 3.28+、Ninja。设置 `JAVA_HOME`、`ANDROID_HOME`，运行 `python build.py`。

`vendor/` 包含构建所需的 LSPlant、DexBuilder、parallel-hashmap、Dobby 与 Zygisk API。修改后的 LSPlant 同时保留独立 fork；来源、版本与许可证见 [THIRD_PARTY.md](THIRD_PARTY.md)。静态链接依赖的源码和构建入口均保留，允许自行修改并重新链接。

## 许可证

原有代码采用 GPL-3.0-only，Copyright (C) 2026 Axuan。第三方代码遵循各自许可证，见 [THIRD_PARTY.md](THIRD_PARTY.md)。

版本 `v1.3.0`，包名 / 模块 ID：`vivo_miuix_shader_fix`。
