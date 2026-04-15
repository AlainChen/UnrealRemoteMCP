// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class RemoteMCP : ModuleRules
{
	public RemoteMCP(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"JsonUtilities"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				// Engine core
				"CoreUObject",
				"Engine",
				"Projects",
				"InputCore",
				"Json",
				"JsonUtilities",
				"DeveloperSettings",

				// Editor
				"UnrealEd",
				"EditorFramework",
				"EditorSubsystem",
				"EditorScriptingUtilities",
				"LevelEditor",
				"ToolMenus",

				// UI
				"Slate",
				"SlateCore",
				"UMG",
				"UMGEditor",
				"Blutility",

				// Blueprint graph editing
				"Kismet",
				"KismetCompiler",
				"BlueprintGraph",
				"AssetRegistry",

				// Python
				"PythonScriptPlugin",

				// AI / Behavior Tree
				"AIModule",
				"BehaviorTreeEditor",
				"AIGraph"
			}
		);

		// ----------------------------------------------------------------
		// ECABridge integration (Optional)
		//
		// When the engine contains the ECABridge plugin (Experimental),
		// we link against it and define WITH_ECA_BRIDGE=1 so that
		// MCPECAProxy can call FECACommandRegistry at compile time.
		// When absent (e.g. TestMCP project), WITH_ECA_BRIDGE=0 and
		// all ECA code paths compile to safe stubs.
		//
		// Pattern reference: ECABridge.Build.cs L91-101 (ToolsetRegistry)
		// ----------------------------------------------------------------
		bool bHasECABridge = Directory.Exists(
			Path.Combine(EngineDirectory, "Plugins", "Experimental", "ECABridge", "Source"));
		if (bHasECABridge)
		{
			PrivateDependencyModuleNames.Add("ECABridge");
			PublicDefinitions.Add("WITH_ECA_BRIDGE=1");
		}
		else
		{
			PublicDefinitions.Add("WITH_ECA_BRIDGE=0");
		}
	}
}
