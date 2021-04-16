using UnrealBuildTool;

public class ExternalEditTextInputs : ModuleRules
{
	public ExternalEditTextInputs(ReadOnlyTargetRules Target) : base(Target)
	{
		DefaultBuildSettings = BuildSettingsVersion.V2;
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"EditorStyle",
				"Engine",
				"InputCore",
				"MainFrame",
				"Slate",
				"SlateCore",
			}
		);
	}
}
