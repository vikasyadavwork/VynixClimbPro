// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Assignment : ModuleRules
{
	public Assignment(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "HeadMountedDisplay", "EnhancedInput", "UMG" });
		PrivateDependencyModuleNames.Add("SlateCore");
		PrivateDependencyModuleNames.Add("AnimationCore");
		PrivateDependencyModuleNames.Add("HairStrandsCore");
	}
}
