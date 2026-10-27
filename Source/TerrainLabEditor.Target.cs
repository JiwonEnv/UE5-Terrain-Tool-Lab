using UnrealBuildTool;

public class TerrainLabEditorTarget : TargetRules
{
	public TerrainLabEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("TerrainLab");
	}
}
