using UnrealBuildTool;

public class TerrainLab : ModuleRules
{
	public TerrainLab(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "Landscape" });
	}
}
