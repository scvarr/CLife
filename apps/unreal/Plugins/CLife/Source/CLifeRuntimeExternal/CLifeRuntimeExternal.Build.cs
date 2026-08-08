using System.IO;
using UnrealBuildTool;

public class CLifeRuntimeExternal : ModuleRules
{
    public CLifeRuntimeExternal(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.External;

        if (Target.Platform != UnrealTargetPlatform.Win64)
        {
            throw new BuildException("The CLife demo external module currently supports Win64 only.");
        }

        string IncludeDirectory = Path.Combine(ModuleDirectory, "Include");
        string LibraryDirectory = Path.Combine(ModuleDirectory, "Libraries", "Win64");
        string PresetLibrary = Path.Combine(LibraryDirectory, "clife_presets.lib");
        if (!Directory.Exists(IncludeDirectory) || !File.Exists(PresetLibrary))
        {
            throw new BuildException(
                "CLife libraries are not staged. Run scripts/build_unreal_clife.ps1 from the repository first.");
        }

        PublicSystemIncludePaths.Add(IncludeDirectory);
        PublicAdditionalLibraries.Add(Path.Combine(LibraryDirectory, "clife_core.lib"));
        PublicAdditionalLibraries.Add(Path.Combine(LibraryDirectory, "clife_world.lib"));
        PublicAdditionalLibraries.Add(PresetLibrary);
    }
}
