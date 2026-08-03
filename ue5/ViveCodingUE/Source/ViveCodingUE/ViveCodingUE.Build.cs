// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class ViveCodingUE : ModuleRules
{
	public ViveCodingUE(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "UMG" });

		// OnlineSubsystemSteam is deliberately NOT linked here. All session code goes through
		// IOnlineSubsystem / IOnlineSession so that swapping Steam <-> NULL is a DefaultEngine.ini change only.
		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore", "OnlineSubsystem", "OnlineSubsystemUtils" });
	}
}
