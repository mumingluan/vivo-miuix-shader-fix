package com.axuan.vivomiuix;

/** Instance JNI witnesses avoid static class-initialization resolution stubs. */
public final class RuntimeLayoutProbe {
    public RuntimeLayoutProbe() {}
    public native int first(int value);
    public native int second(int value);
}
