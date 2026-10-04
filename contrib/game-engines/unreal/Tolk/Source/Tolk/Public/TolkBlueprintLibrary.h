// Tolk - Unreal Engine plugin: Blueprint exposed API.
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TolkBlueprintLibrary.generated.h"

/** Exposes Tolk to Blueprints and C++. */
UCLASS()
class TOLK_API UTolkBlueprintLibrary : public UBlueprintFunctionLibrary
{
  GENERATED_BODY()

public:
  /** Initializes Tolk and detects the active screen reader. */
  UFUNCTION(BlueprintCallable, Category = "Tolk")
  static void Load();

  /** Releases Tolk and every screen reader driver it holds. */
  UFUNCTION(BlueprintCallable, Category = "Tolk")
  static void Unload();

  /** Whether Tolk has been initialized. */
  UFUNCTION(BlueprintPure, Category = "Tolk")
  static bool IsLoaded();

  /** Enables or disables the fallback speech engines (OneCore and SAPI). */
  UFUNCTION(BlueprintCallable, Category = "Tolk")
  static void TrySAPI(bool trySAPI);

  /** Moves the fallback speech engines to the front or the end of detection. */
  UFUNCTION(BlueprintCallable, Category = "Tolk")
  static void PreferSAPI(bool preferSAPI);

  /** Name of the active screen reader, or an empty string when none is active. */
  UFUNCTION(BlueprintPure, Category = "Tolk")
  static FString DetectScreenReader();

  /** Whether the active driver supports speech. */
  UFUNCTION(BlueprintPure, Category = "Tolk")
  static bool HasSpeech();

  /** Whether the active driver supports braille. */
  UFUNCTION(BlueprintPure, Category = "Tolk")
  static bool HasBraille();

  /** Outputs text through the active driver, using speech and/or braille. */
  UFUNCTION(BlueprintCallable, Category = "Tolk")
  static bool Output(const FString &Text, bool bInterrupt = false);

  /** Speaks text through the active driver. */
  UFUNCTION(BlueprintCallable, Category = "Tolk")
  static bool Speak(const FString &Text, bool bInterrupt = false);

  /** Brailles text through the active driver. */
  UFUNCTION(BlueprintCallable, Category = "Tolk")
  static bool Braille(const FString &Text);

  /** Whether the active driver is currently speaking. */
  UFUNCTION(BlueprintPure, Category = "Tolk")
  static bool IsSpeaking();

  /** Cancels any speech in progress. */
  UFUNCTION(BlueprintCallable, Category = "Tolk")
  static bool Silence();
};