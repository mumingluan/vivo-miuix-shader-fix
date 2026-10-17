package com.axuan.vivomiuix;
import java.lang.reflect.Method;
import java.lang.reflect.InvocationTargetException;
import java.util.Arrays;
public final class ShaderHook {
    public Method backup;
    private static final Class<?> RUNTIME;
    static { try { RUNTIME = Class.forName("android.graphics.RuntimeShader"); }
             catch (ClassNotFoundException e) { throw new ExceptionInInitializerError(e); } }
    public Object callback(Object[] args) throws Throwable {
        Object receiver = args[0];
        if (RUNTIME.isInstance(receiver)) return receiver;
        try { return backup.invoke(receiver, Arrays.copyOfRange(args, 1, args.length)); }
        catch (InvocationTargetException e) { throw e.getCause(); }
    }
}
