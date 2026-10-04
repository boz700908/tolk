// Tolk - Unreal Engine plugin build rules.
using System.IO;
using UnrealBuildTool;

public class Tolk : ModuleRules
{
  public Tolk(ReadOnlyTargetRules Target) : base(Target)
  {
    PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

    PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });

    string ThirdParty = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", "..", "ThirdParty", "Tolk"));
    PublicIncludePaths.Add(Path.Combine(ThirdParty, "include"));

    if (Target.Platform == UnrealTargetPlatform.Win64)
    {
      // One folder per architecture under ThirdParty/Tolk/runtime: "x64" or
      // "arm64". Copy the matching dist/<arch>/<config> folder there.
      string Architecture = Target.WindowsPlatform.Architecture.ToString().ToLowerInvariant();
      string RuntimeDir = Path.Combine(ThirdParty, "runtime", Architecture == "arm64" ? "arm64" : "x64");

      PublicAdditionalLibraries.Add(Path.Combine(RuntimeDir, "Tolk.lib"));

      // Tolk.dll and its screen reader client modules must sit next to the
      // game executable; list them as runtime dependencies so packaging and
      // the editor copy them automatically.
      foreach (string File in Directory.GetFiles(RuntimeDir))
      {
        string Name = Path.GetFileName(File);
        if (Name.EndsWith(".dll") || Name.EndsWith(".ini"))
        {
          RuntimeDependencies.Add(Path.Combine("$(BinaryOutputDir)", Name), File);
        }
      }
    }
  }
}