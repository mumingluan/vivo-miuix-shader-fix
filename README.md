# vivo Miuix Shader Fix

修复 vivo 上 LSPosed Miuix 寄生管理器在深色模式中的 RuntimeShader 复制问题，涉及圆角裁剪和玻璃效果。

通过 Zygisk 仅在 `com.android.shell`、UID appId 2000 的进程中加载。RuntimeShader 的复制返回原对象，其他 Shader 继续调用原实现。LSPosed 的 APK、证书和许可证不作修改。

## 使用

需要 ARM64、Zygisk 和 root 模块管理器。安装 ZIP，按 Zygisk 要求重新加载后，再启动寄生管理器。禁用模块后，同样需要结束已注入进程或重启来撤销内存中的 hook。

主要环境为 iQOO 15 / OriginOS 6。四份固件的 ART 与 libcore 样本做过离线检查；未知布局会拒绝初始化，不能保证未来所有 ART 版本都兼容。

启动时通过反射识别 Shader 复制方法，并处理 Shader 与 vivo 深色模式控制类中的 Java 方法，不绑定方法名、固件版本或 ART 地址。整套 DEX 的调用分析只用于离线对照，不在管理器启动时执行。ART 布局使用两个类的实例 JNI 方法交叉检查，避免静态方法的类初始化跳板造成误判。

修复在寄生管理器 Application 就绪后初始化，避开 Zygisk 特化阶段尚未稳定的 JNI 方法状态。启动等待最长 5 秒，完成或超时后退出；普通系统 Shell 不安装渲染钩子。

## 构建

Windows、Python 3、JDK 21、Android SDK build-tools 36.0.0、NDK 29、CMake 3.28+、Ninja。设置 `JAVA_HOME`、`ANDROID_HOME`，运行 `python build.py`。

`vendor/` 包含构建所需的 LSPlant、DexBuilder、parallel-hashmap、Dobby 与 Zygisk API。修改后的 LSPlant 同时保留独立 fork；来源、版本与许可证见 [THIRD_PARTY.md](THIRD_PARTY.md)。静态链接依赖的源码和构建入口均保留，允许自行修改并重新链接。

## 许可证

原有代码采用 GPL-3.0-only，Copyright (C) 2026 Axuan。第三方代码遵循各自许可证，见 [THIRD_PARTY.md](THIRD_PARTY.md)。

版本 `v1.3.2`，包名 / 模块 ID：`vivo_miuix_shader_fix`。
