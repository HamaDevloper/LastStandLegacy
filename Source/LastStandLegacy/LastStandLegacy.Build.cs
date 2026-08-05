// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LastStandLegacy : ModuleRules
{
    public LastStandLegacy(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        // 🔹 مۆدیۆلە بنەڕەتییەکان کە لە فایلی Header (.h)دا پێویستن
        PublicDependencyModuleNames.AddRange(new string[] {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "NetCore",
            "EnhancedInput"
        });

        // 🔹 مۆدیۆلانەی تەنها لە فایلی (.cpp)دا بەکاریان دەهێنیت (تایبەت بە UI و AI و Physics)
        PrivateDependencyModuleNames.AddRange(new string[] {
            "UMG",
            "Slate",
            "SlateCore",
            "PhysicsCore",
            "AIModule",
            "Niagara"
        });
    }
}