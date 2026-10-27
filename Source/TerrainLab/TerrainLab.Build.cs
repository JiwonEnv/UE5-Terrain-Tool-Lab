using UnrealBuildTool;

public class TerrainLab : ModuleRules
{
	public TerrainLab(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "Landscape", "InputCore", "ProceduralMeshComponent" });
		PrivateDependencyModuleNames.AddRange(new string[] { "Json", "JsonUtilities", "UMG", "Slate", "SlateCore" });
	}
}
