# vivo runtime layout adaptation

Based on LSPosed/LSPlant commit `d8b5d1dbb664abc606644036822e4bb64547edf6`.

This branch derives ArtMethod layout from observed method samples and registered JNI entry points, checks method spacing and declaring-class references, and refuses ambiguous layouts. It is used by vivo Miuix Shader Fix. The JNI entry resolver can be supplied by the caller; unrelated runtime behavior is inherited from upstream.

Modified files: `art_method.cxx`, `layout_probe.hpp`, `lsplant.hpp`, and `hook_helper.hpp` under `lsplant/src/main/jni/`. Maintainer of these changes: Axuan. Upstream copyrights and LGPL-3.0 terms remain in place.

These adaptations were checked against offline ART/libcore samples. They do not establish compatibility with every Android runtime or prove live GC behavior.
