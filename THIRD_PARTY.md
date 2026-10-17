# 第三方来源

| 依赖 | 来源 | 许可证 | 使用方式 |
| --- | --- | --- | --- |
| LSPlant | [上游](https://github.com/LSPosed/LSPlant)，基线 `d8b5d1dbb664abc606644036822e4bb64547edf6`；[本项目修改 fork](https://github.com/mumingluan/LSPlant/tree/vivo-runtime-layout) | LGPL-3.0 | 静态链接，完整修改源码在 vendor/LSPlant |
| DexBuilder | [LSPosed/DexBuilder](https://github.com/LSPosed/DexBuilder)，`ac7fb2230954ee311808bad469b0db501f31bfb8` | LGPL-3.0，部分 slicer 文件为 Apache-2.0 | 原源码随 LSPlant 保留 |
| parallel-hashmap | [greg7mdp/parallel-hashmap](https://github.com/greg7mdp/parallel-hashmap) | Apache-2.0，含原文件中的其他许可说明 | 原源码与许可证随 DexBuilder 保留 |
| Dobby | Maven `io.github.vvb2060.ndk:dobby:1.2`，见 `vendor/dobby-origin.json` | Apache-2.0 | 静态库和 API 头文件 |
| Zygisk API | [topjohnwu/zygisk-module-sample](https://github.com/topjohnwu/zygisk-module-sample) | ISC | `vendor/zygisk.hpp`，版权与授权段保留 |

各依赖的原版权、授权和免责条款保留在 vendor/ 与 module/licenses/。LSPlant 的运行时布局推导改动另见 vendor/LSPlant/VIVO-PATCH.md。使用所附 CMake 与构建脚本可修改依赖后重新链接模块。
