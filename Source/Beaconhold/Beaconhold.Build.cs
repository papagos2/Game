using UnrealBuildTool;
using System.IO;

public class Beaconhold : ModuleRules
{
	public Beaconhold(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// The simulation core (Sim/) is plain C++ with unique helper names, but it is compiled
		// file by file anyway so anonymous-namespace helpers can never collide.
		bUseUnity = false;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"Slate",
			"SlateCore",
			"DeveloperSettings",
			"MeshDescription",
			"StaticMeshDescription",
			"RenderCore",
			"RHI",
		});

		PrivateIncludePaths.Add(ModuleDirectory);
		PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Sim"));
	}
}
