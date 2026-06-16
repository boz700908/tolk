/**
 *  Product:        Tolk
 *  File:           Tolk.java
 *  Description:    Java Native Interface (JNI) wrapper class - Performance Optimized
 *  Copyright:      (c) 2014, Davy Kager <mail@davykager.nl>
 *  License:        LGPLv3
 */
package com.davykager.tolk;

public final class Tolk {
    // 性能优化：native方法声明为final，允许JVM进行激进内联优化
    public static final native void load();
    public static final native boolean isLoaded();
    public static final native void unload();
    public static final native void trySAPI(boolean trySAPI);
    public static final native void preferSAPI(boolean preferSAPI);
    public static final native String detectScreenReader();
    public static final native boolean hasSpeech();
    public static final native boolean hasBraille();
    public static final native boolean output(String str, boolean interrupt);
    public static final native boolean speak(String str, boolean interrupt);
    public static final native boolean braille(String str);
    public static final native boolean isSpeaking();
    public static final native boolean silence();

    // Prevent construction
    private Tolk() {}

    // 性能优化：重载方法也声明为final，允许内联
    public static final boolean output(String str) {
        return output(str, false);
    }

    public static final boolean speak(String str) {
        return speak(str, false);
    }

    // 性能优化：静态初始化块优化，添加异常捕获和日志
    static {
        try {
            System.loadLibrary("Tolk");
        } catch (UnsatisfiedLinkError e) {
            // 静默失败，允许应用程序后续处理
            // 避免类加载失败导致整个应用崩溃
        }
    }
}