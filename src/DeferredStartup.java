package com.axuan.vivomiuix;

import java.lang.reflect.Constructor;
import java.lang.reflect.Method;

/** One bounded startup handoff; no worker remains after the main queue is ready. */
public final class DeferredStartup {
    private static final long WAIT_NANOS = 5_000_000_000L;
    private static final long RETRY_MILLIS = 25;
    private static boolean started;
    private DeferredStartup() { }
    private static native boolean initialize();
    private static native void report(String message);

    interface Callbacks {
        boolean initialize();
        void report(String message);
    }

    public static void schedule() {
        start(new Callbacks() {
            public boolean initialize() { return DeferredStartup.initialize(); }
            public void report(String message) { DeferredStartup.report(message); }
        });
    }

    static synchronized void start(final Callbacks callbacks) {
        if (started) return;
        started = true;
        final long deadline = System.nanoTime() + WAIT_NANOS;
        Thread worker = new Thread(new Runnable() {
            public void run() {
                try {
                    Class<?> looper = Class.forName("android.os.Looper", false, null);
                    Method getMain = looper.getMethod("getMainLooper");
                    Object main = null;
                    while (System.nanoTime() < deadline && (main = getMain.invoke(null)) == null)
                        Thread.sleep(RETRY_MILLIS);
                    if (main == null) { callbacks.report("Startup timed out before main Looper"); return; }
                    Class<?> handler = Class.forName("android.os.Handler", false, null);
                    Constructor<?> constructor = handler.getConstructor(looper);
                    Object queue = constructor.newInstance(main);
                    Method post = handler.getMethod("post", Runnable.class);
                    Method later = handler.getMethod("postDelayed", Runnable.class, long.class);
                    Class<?> thread = Class.forName("android.app.ActivityThread", false, null);
                    Method application = thread.getDeclaredMethod("currentApplication");
                    application.setAccessible(true);
                    final Object targetQueue = queue;
                    final Method enqueue = later, current = application;
                    Runnable task = new Runnable() {
                        public void run() {
                            try {
                                if (System.nanoTime() >= deadline) { callbacks.report("Startup timed out before Application"); return; }
                                Object app = current.invoke(null);
                                if (app == null) {
                                    if (!Boolean.TRUE.equals(enqueue.invoke(targetQueue, this, RETRY_MILLIS)))
                                        callbacks.report("Startup main queue rejected retry");
                                    return;
                                }
                                if (!"org.lsposed.manager.App".equals(app.getClass().getName())) {
                                    callbacks.report("Startup skipped for ordinary Shell Application"); return;
                                }
                                if (!callbacks.initialize()) callbacks.report("Deferred shader initialization refused");
                            } catch (Throwable failure) { callbacks.report("Deferred shader initialization error: " + failure); }
                        }
                    };
                    if (!Boolean.TRUE.equals(post.invoke(queue, task))) callbacks.report("Startup main queue rejected task");
                } catch (Throwable failure) { callbacks.report("Startup handoff unavailable: " + failure); }
            }
        }, "VivoMiuixShaderInit");
        worker.setDaemon(true);
        worker.start();
    }
}
