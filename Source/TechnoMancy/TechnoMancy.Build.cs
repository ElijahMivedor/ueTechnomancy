// Copyright TechnoMancy. All rights reserved.

using UnrealBuildTool;

public class TechnoMancy : ModuleRules
{
	public TechnoMancy(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		bEnableExceptions = true;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"InputCore",
		});
	}
}
