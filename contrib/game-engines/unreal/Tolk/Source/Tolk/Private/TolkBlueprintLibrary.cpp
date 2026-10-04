// Tolk - Unreal Engine plugin: Blueprint exposed API.
#include "TolkBlueprintLibrary.h"

#include "Tolk.h"

void UTolkBlueprintLibrary::Load()
{
  if (!Tolk_IsLoaded())
  {
    Tolk_Load();
  }
}

void UTolkBlueprintLibrary::Unload()
{
  if (Tolk_IsLoaded())
  {
    Tolk_Unload();
  }
}

bool UTolkBlueprintLibrary::IsLoaded()
{
  return Tolk_IsLoaded();
}

void UTolkBlueprintLibrary::TrySAPI(bool trySAPI)
{
  Tolk_TrySAPI(trySAPI);
}

void UTolkBlueprintLibrary::PreferSAPI(bool preferSAPI)
{
  Tolk_PreferSAPI(preferSAPI);
}

FString UTolkBlueprintLibrary::DetectScreenReader()
{
  const wchar_t *Name = Tolk_DetectScreenReader();
  return Name == nullptr ? FString() : FString(Name);
}

bool UTolkBlueprintLibrary::HasSpeech()
{
  return Tolk_HasSpeech();
}

bool UTolkBlueprintLibrary::HasBraille()
{
  return Tolk_HasBraille();
}

bool UTolkBlueprintLibrary::Output(const FString &Text, bool bInterrupt)
{
  return Tolk_Output(*Text, bInterrupt);
}

bool UTolkBlueprintLibrary::Speak(const FString &Text, bool bInterrupt)
{
  return Tolk_Speak(*Text, bInterrupt);
}

bool UTolkBlueprintLibrary::Braille(const FString &Text)
{
  return Tolk_Braille(*Text);
}

bool UTolkBlueprintLibrary::IsSpeaking()
{
  return Tolk_IsSpeaking();
}

bool UTolkBlueprintLibrary::Silence()
{
  return Tolk_Silence();
}