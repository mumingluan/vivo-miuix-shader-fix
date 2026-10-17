package com.axuan.vivomiuix;

/** Two ordinary JNI methods with distinct registered native implementations. */
public final class RuntimeLayoutProbe {
    private RuntimeLayoutProbe() {}
    public static native int first(int value);
    public static native int second(int value);
}
