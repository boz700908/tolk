/**
 *  Product:        Tolk
 *  File:           ScreenReaderDriverWE.cpp
 *  Description:    Driver for the Window-Eyes screen reader.
 *  Copyright:      (c) 2014, Davy Kager <mail@davykager.nl>
 *  License:        LGPLv3
 */

#include "ScreenReaderDriverWE.h"

ScreenReaderDriverWE::ScreenReaderDriverWE() :
  ScreenReaderDriver(L"Window-Eyes", true, true),
  controller(nullptr),
  speech(nullptr),
  braille(nullptr)
{
  varOpt.vt = VT_ERROR;
  varOpt.scode = DISP_E_PARAMNOTFOUND;
  if (IsRunning()) Initialize();
}

ScreenReaderDriverWE::~ScreenReaderDriverWE() {
  Finalize();
}

bool ScreenReaderDriverWE::Speak(const wchar_t *str, bool interrupt) {
  if (!controller || !speech) return false;
  if (interrupt && !Silence()) return false;
  const BSTR bstr = SysAllocString(str);
  if (!bstr) return false;
  const bool succeeded = SUCCEEDED(speech->Speak(bstr, varOpt));
  SysFreeString(bstr);
  return succeeded;
}

bool ScreenReaderDriverWE::Braille(const wchar_t *str) {
  if (!controller || !braille) return false;
  const BSTR bstr = SysAllocString(str);
  if (!bstr) return false;
  const bool succeeded = SUCCEEDED(braille->Display(bstr, varOpt, varOpt));
  SysFreeString(bstr);
  return succeeded;
}

bool ScreenReaderDriverWE::Silence() {
  if (!controller || !speech) return false;
  return SUCCEEDED(speech->Silence());
}

bool ScreenReaderDriverWE::IsActive() {
  if (!IsRunning()) {
    Finalize();
    return false;
  }
  if (!controller) Initialize();
  return (!!controller);
}

bool ScreenReaderDriverWE::Output(const wchar_t *str, bool interrupt) {
  if (!controller || !speech || !braille) return false;
  if (interrupt && !Silence()) return false;
  const BSTR bstr = SysAllocString(str);
  if (!bstr) return false;
  // Beware short-circuiting.
  const bool speakSucceeded = SUCCEEDED(speech->Speak(bstr, varOpt));
  const bool brailleSucceeded = SUCCEEDED(braille->Display(bstr, varOpt, varOpt));
  SysFreeString(bstr);
  return (speakSucceeded || brailleSucceeded);
}

void ScreenReaderDriverWE::Initialize() {
  if (controller || FAILED(CoCreateInstance(CLSID_Application, nullptr, CLSCTX_INPROC_SERVER, IID__Application, (void **)&controller)))
    return;
  if (FAILED(controller->get_Speech(&speech)) || FAILED(controller->get_Braille(&braille))) {
    Finalize();
    return;
  }
}

void ScreenReaderDriverWE::Finalize() {
  if (braille) {
    braille->Release();
    braille = nullptr;
  }
  if (speech) {
    speech->Release();
    speech = nullptr;
  }
  if (controller) {
    controller->Release();
    controller = nullptr;
  }
}
