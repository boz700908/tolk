/// Tolk - GameMaker binding.
///
/// Call tolk_init() once at game start. Put TolkGml.dll, Tolk.dll and the
/// screen reader client modules in the game's working directory (or ship them
/// as Included Files).

function tolk_init() {
    if (variable_global_exists("tolk_ready") && global.tolk_ready) return true;

    global.tolk_load = external_define("TolkGml.dll", "TolkGml_Load", dll_cdecl, ty_real, 0);
    global.tolk_unload = external_define("TolkGml.dll", "TolkGml_Unload", dll_cdecl, ty_real, 0);
    global.tolk_is_loaded = external_define("TolkGml.dll", "TolkGml_IsLoaded", dll_cdecl, ty_real, 0);
    global.tolk_try_sapi = external_define("TolkGml.dll", "TolkGml_TrySAPI", dll_cdecl, ty_real, 1, ty_real);
    global.tolk_prefer_sapi = external_define("TolkGml.dll", "TolkGml_PreferSAPI", dll_cdecl, ty_real, 1, ty_real);
    global.tolk_detect = external_define("TolkGml.dll", "TolkGml_DetectScreenReader", dll_cdecl, ty_string, 0);
    global.tolk_has_speech = external_define("TolkGml.dll", "TolkGml_HasSpeech", dll_cdecl, ty_real, 0);
    global.tolk_has_braille = external_define("TolkGml.dll", "TolkGml_HasBraille", dll_cdecl, ty_real, 0);
    global.tolk_output = external_define("TolkGml.dll", "TolkGml_Output", dll_cdecl, ty_real, 2, ty_string, ty_real);
    global.tolk_speak = external_define("TolkGml.dll", "TolkGml_Speak", dll_cdecl, ty_real, 2, ty_string, ty_real);
    global.tolk_braille = external_define("TolkGml.dll", "TolkGml_Braille", dll_cdecl, ty_real, 1, ty_string);
    global.tolk_is_speaking = external_define("TolkGml.dll", "TolkGml_IsSpeaking", dll_cdecl, ty_real, 0);
    global.tolk_silence = external_define("TolkGml.dll", "TolkGml_Silence", dll_cdecl, ty_real, 0);

    global.tolk_ready = true;
    tolk_load();
    return true;
}

function tolk_load() {
    if (!tolk_init()) return false;
    return external_call(global.tolk_load) != 0;
}

function tolk_unload() {
    if (!tolk_init()) return false;
    return external_call(global.tolk_unload) != 0;
}

function tolk_is_loaded() {
    if (!tolk_init()) return false;
    return external_call(global.tolk_is_loaded) != 0;
}

/// Turns the fallback speech engines (Windows OneCore and SAPI) on or off.
function tolk_try_sapi(enabled) {
    if (!tolk_init()) return false;
    return external_call(global.tolk_try_sapi, enabled) != 0;
}

/// Moves the fallback speech engines to the front (true) or the end (false).
function tolk_prefer_sapi(preferred) {
    if (!tolk_init()) return false;
    return external_call(global.tolk_prefer_sapi, preferred) != 0;
}

/// Name of the active screen reader, or "" when none is active.
function tolk_detect_screen_reader() {
    if (!tolk_init()) return "";
    return external_call(global.tolk_detect);
}

function tolk_has_speech() {
    if (!tolk_init()) return false;
    return external_call(global.tolk_has_speech) != 0;
}

function tolk_has_braille() {
    if (!tolk_init()) return false;
    return external_call(global.tolk_has_braille) != 0;
}

/// Outputs text using speech and/or braille. This is the preferred call.
function tolk_output(text, interrupt = false) {
    if (!tolk_init()) return false;
    return external_call(global.tolk_output, text, interrupt) != 0;
}

function tolk_speak(text, interrupt = false) {
    if (!tolk_init()) return false;
    return external_call(global.tolk_speak, text, interrupt) != 0;
}

function tolk_braille(text) {
    if (!tolk_init()) return false;
    return external_call(global.tolk_braille, text) != 0;
}

function tolk_is_speaking() {
    if (!tolk_init()) return false;
    return external_call(global.tolk_is_speaking) != 0;
}

function tolk_silence() {
    if (!tolk_init()) return false;
    return external_call(global.tolk_silence) != 0;
}