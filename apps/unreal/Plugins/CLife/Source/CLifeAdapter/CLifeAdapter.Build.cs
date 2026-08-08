using UnrealBuildTool;

public class CLifeAdapter : ModuleRules
{
    public CLifeAdapter(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Cpp20;
        bEnableExceptions = true;

        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core",
            "CoreUObject",
            "Engine"
        });

        PrivateDependencyModuleNames.AddRange(new[]
        {
            "CLifeRuntimeExternal",
            "InputCore",
            "Slate",
            "SlateCore"
        });
    }
}
