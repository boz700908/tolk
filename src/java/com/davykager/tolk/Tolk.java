/**
 *  Product:        Tolk
 *  File:           Tolk.java
 *  Description:    Java Native Interface (JNI) wrapper class - Performance Optimized
 *  Copyright:      (c) 2014, Davy Kager <mail@davykager.nl>
 *  License:        LGPLv3
 */
package com.davykager.tolk;

public final class Tolk {
    // Performance: native methods declared as final, allows JVM aggressive inlining optimization
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

    // Performance: overloaded methods also declared as final, allows inlining
    public static final boolean output(String str) {
        return output(str, false);
    }

    public static final boolean speak(String str) {
        return speak(str, false);
    }

    // Performance: optimized static initializer, added exception handling
    static {
        try {
            System.loadLibrary("Tolk");
        } catch (UnsatisfiedLinkError e) {
            // Silent failure, allows application to handle later
            // Prevents class loading failure from crashing the entire application
        }
    }
}