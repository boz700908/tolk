/**
 *  Product:        Tolk
 *  File:           Tolk.cs
 *  Description:    Unity wrapper around Tolk.dll. Every call is guarded so a
 *                  build without the native library keeps running: the game
 *                  simply loses its speech and braille output.
 *  Copyright:      (c) 2014-2026, Tolk contributors
 *  License:        LGPLv3
 */
using System;
using System.Runtime.InteropServices;

namespace DavyKager
{
  /// <summary>
  /// Thin, allocation-free entry point to <c>Tolk.dll</c> for Unity.
  /// </summary>
  /// <remarks>
  /// Place the native library next to the game: copy the contents of the
  /// matching <c>dist/&lt;arch&gt;/&lt;config&gt;</c> folder (Tolk.dll plus the screen
  /// reader modules) into <c>Assets/Plugins/&lt;platform&gt;</c>. When the library is
  /// missing, <see cref="IsAvailable"/> is <c>false</c> and every method is a
  /// safe no-op that returns <c>false</c>.
  /// </remarks>
  public static class Tolk
  {
    private const string Library = "Tolk";

    private static bool? available;

    [DllImport(Library, CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
    private static extern void Tolk_Load();

    [DllImport(Library, CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool Tolk_IsLoaded();

    [DllImport(Library, CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
    private static extern void Tolk_Unload();

    [DllImport(Library, CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
    private static extern void Tolk_TrySAPI([MarshalAs(UnmanagedType.I1)] bool trySAPI);

    [DllImport(Library, CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
    private static extern void Tolk_PreferSAPI([MarshalAs(UnmanagedType.I1)] bool preferSAPI);

    [DllImport(Library, CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
    private static extern IntPtr Tolk_DetectScreenReader();

    [DllImport(Library, CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool Tolk_HasSpeech();

    [DllImport(Library, CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool Tolk_HasBraille();

    [DllImport(Library, CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool Tolk_Output(
        [MarshalAs(UnmanagedType.LPWStr)] string str,
        [MarshalAs(UnmanagedType.I1)] bool interrupt);

    [DllImport(Library, CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool Tolk_Speak(
        [MarshalAs(UnmanagedType.LPWStr)] string str,
        [MarshalAs(UnmanagedType.I1)] bool interrupt);

    [DllImport(Library, CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool Tolk_Braille([MarshalAs(UnmanagedType.LPWStr)] string str);

    [DllImport(Library, CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool Tolk_IsSpeaking();

    [DllImport(Library, CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
    [return: MarshalAs(UnmanagedType.I1)]
    private static extern bool Tolk_Silence();

    /// <summary>Whether Tolk.dll could be loaded on this machine.</summary>
    public static bool IsAvailable
    {
      get
      {
        if (!available.HasValue)
        {
          try
          {
            Tolk_IsLoaded();
            available = true;
          }
          catch (DllNotFoundException)
          {
            available = false;
          }
          catch (EntryPointNotFoundException)
          {
            available = false;
          }
          catch (BadImageFormatException)
          {
            // A module of the wrong architecture was picked up, e.g. a 32-bit
            // Tolk.dll in a 64-bit player.
            available = false;
          }
        }
        return available.Value;
      }
    }

    /// <summary>Initializes Tolk and detects the active screen reader. Safe to call more than once.</summary>
    public static void Initialize()
    {
      if (IsAvailable)
      {
        Tolk_Load();
      }
    }

    /// <summary>Tests whether Tolk has been initialized.</summary>
    public static bool IsInitialized()
    {
      return IsAvailable && Tolk_IsLoaded();
    }

    /// <summary>Releases Tolk and every screen reader driver it holds.</summary>
    public static void Shutdown()
    {
      if (IsAvailable)
      {
        Tolk_Unload();
      }
    }

    /// <summary>Enables or disables the fallback speech engines (OneCore and SAPI).</summary>
    public static void TrySAPI(bool trySAPI)
    {
      if (IsAvailable)
      {
        Tolk_TrySAPI(trySAPI);
      }
    }

    /// <summary>Moves the fallback speech engines to the front or the end of detection.</summary>
    public static void PreferSAPI(bool preferSAPI)
    {
      if (IsAvailable)
      {
        Tolk_PreferSAPI(preferSAPI);
      }
    }

    /// <summary>Returns the name of the active screen reader, or null when none is active.</summary>
    public static string DetectScreenReader()
    {
      if (!IsAvailable)
      {
        return null;
      }
      IntPtr name = Tolk_DetectScreenReader();
      return name == IntPtr.Zero ? null : Marshal.PtrToStringUni(name);
    }

    /// <summary>Whether the active driver supports speech.</summary>
    public static bool HasSpeech()
    {
      return IsAvailable && Tolk_HasSpeech();
    }

    /// <summary>Whether the active driver supports braille.</summary>
    public static bool HasBraille()
    {
      return IsAvailable && Tolk_HasBraille();
    }

    /// <summary>Outputs text through the active driver, using speech and/or braille.</summary>
    public static bool Output(string str, bool interrupt = false)
    {
      return IsAvailable && Tolk_Output(str, interrupt);
    }

    /// <summary>Speaks text through the active driver.</summary>
    public static bool Speak(string str, bool interrupt = false)
    {
      return IsAvailable && Tolk_Speak(str, interrupt);
    }

    /// <summary>Brailles text through the active driver.</summary>
    public static bool Braille(string str)
    {
      return IsAvailable && Tolk_Braille(str);
    }

    /// <summary>Whether the active driver is currently speaking.</summary>
    public static bool IsSpeaking()
    {
      return IsAvailable && Tolk_IsSpeaking();
    }

    /// <summary>Cancels any speech in progress.</summary>
    public static bool Silence()
    {
      return IsAvailable && Tolk_Silence();
    }
  }
}