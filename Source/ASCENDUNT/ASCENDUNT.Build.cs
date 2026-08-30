// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class ASCENDUNT : ModuleRules
{
	public ASCENDUNT(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "NetCore",
            "IrisCore",
            "EnhancedInput",
            "GameplayTags",
            "GameplayTasks",
            "GameplayAbilities",
            "AIModule",
            "NavigationSystem",
            "Niagara",
            "UMG",
            "OnlineSubsystem",
            "OnlineSubsystemUtils",
            "HTTP",
            "Sockets",
            "Json",
            "JsonUtilities",
            }
		);

        PublicIncludePaths.AddRange(new string[]
		{

		});

        PrivateDependencyModuleNames.AddRange(new string[] 
		{
            "Slate",
			"SlateCore"
        });

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
