using UnrealBuildTool;
using System.Collections.Generic;

public class BeaconholdEditorTarget : TargetRules
{
	public BeaconholdEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("Beaconhold");
	}
}
