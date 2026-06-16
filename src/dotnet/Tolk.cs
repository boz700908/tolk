/**
 *  Product:        Tolk
 *  File:           Tolk.cs
 *  Description:    .NET wrapper class (in C#) - Performance Optimized
 *  Copyright:      (c) 2014, Davy Kager <mail@davykager.nl>
 *  License:        LGPLv3
 */
using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Security;

namespace DavyKager {
  public sealed class Tolk {
    // 性能优化：SuppressUnmanagedCodeSecurity 跳过安全检查，减少P/Invoke开销
    // 性能优化：移除 SetLastError=true（Tolk DLL不设置Win32错误码，无需额外检查）
    [SuppressUnmanagedCodeSecurity]
    [DllImport("Tolk.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
    private static extern void Tolk_Load();

    [SuppressUnmanagedCodeSecurity]
    [DllImport("Tolk.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool Tolk_IsLoaded();

    [SuppressUnmanagedCodeSecurity]
    [DllImport("Tolk.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
    private static extern void Tolk_Unload();

    [SuppressUnmanagedCodeSecurity]
    [DllImport("Tolk.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
    private static extern void Tolk_TrySAPI([MarshalAs(UnmanagedType.I1)] bool trySAPI);

    [SuppressUnmanagedCodeSecurity]
    [DllImport("Tolk.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
    private static extern void Tolk_PreferSAPI([MarshalAs(UnmanagedType.I1)] bool preferSAPI);

    [SuppressUnmanagedCodeSecurity]
    [DllImport("Tolk.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr Tolk_DetectScreenReader();

    [SuppressUnmanagedCodeSecurity]
    [DllImport("Tolk.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool Tolk_HasSpeech();

    [SuppressUnmanagedCodeSecurity]
    [DllImport("Tolk.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool Tolk_HasBraille();

    [SuppressUnmanagedCodeSecurity]
    [DllImport("Tolk.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool Tolk_Output(
        [MarshalAs(UnmanagedType.LPWStr)] string str,
        [MarshalAs(UnmanagedType.I1)] bool interrupt);

    [SuppressUnmanagedCodeSecurity]
    [DllImport("Tolk.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool Tolk_Speak(
        [MarshalAs(UnmanagedType.LPWStr)] string str,
        [MarshalAs(UnmanagedType.I1)] bool interrupt);

    [SuppressUnmanagedCodeSecurity]
    [DllImport("Tolk.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool Tolk_Braille([MarshalAs(UnmanagedType.LPWStr)] string str);

    [SuppressUnmanagedCodeSecurity]
    [DllImport("Tolk.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool Tolk_IsSpeaking();

    [SuppressUnmanagedCodeSecurity]
    [DllImport("Tolk.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool Tolk_Silence();

    // Prevent construction
    private Tolk() { }

    // 性能优化：AggressiveInlining 强制内联小方法，减少方法调用开销
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static void Load() => Tolk_Load();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static bool IsLoaded() => Tolk_IsLoaded();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static void Unload() => Tolk_Unload();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static void TrySAPI(bool trySAPI) => Tolk_TrySAPI(trySAPI);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static void PreferSAPI(bool preferSAPI) => Tolk_PreferSAPI(preferSAPI);

    // Prevent the marshaller from freeing the unmanaged string
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static string DetectScreenReader() => Marshal.PtrToStringUni(Tolk_DetectScreenReader());

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static bool HasSpeech() => Tolk_HasSpeech();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static bool HasBraille() => Tolk_HasBraille();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static bool Output(string str, bool interrupt = false) => Tolk_Output(str, interrupt);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static bool Speak(string str, bool interrupt = false) => Tolk_Speak(str, interrupt);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static bool Braille(string str) => Tolk_Braille(str);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static bool IsSpeaking() => Tolk_IsSpeaking();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static bool Silence() => Tolk_Silence();
  }
}