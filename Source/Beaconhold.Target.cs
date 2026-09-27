using UnrealBuildTool;
using System.Collections.Generic;

public class BeaconholdTarget : TargetRules
{
	public BeaconholdTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("Beaconhold");
	}
}
