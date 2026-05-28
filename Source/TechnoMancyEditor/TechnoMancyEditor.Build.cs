// Copyright TechnoMancy. All rights reserved.

using UnrealBuildTool;

public class TechnoMancyEditor : ModuleRules
{
	public TechnoMancyEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		bEnableExceptions = true;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"TechnoMancy",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd",
			"AssetTools",
			"AssetRegistry",
			"ContentBrowser",
			"Slate",
			"SlateCore",
			"UMG",
			"Blutility",
			"UMGEditor",
			"EditorStyle",
			"EditorFramework",
			"PropertyEditor",
			"InputCore",
			"ToolMenus",
		});
	}
}
