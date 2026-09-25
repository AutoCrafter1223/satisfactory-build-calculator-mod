using UnrealBuildTool;

public class SatisfactoryBuildCalculator : ModuleRules
{
	public SatisfactoryBuildCalculator(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		CppStandard = CppStandardVersion.Cpp20;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"FactoryGame",
			"SML",
			"InputCore",
			"Slate",
			"SlateCore",
			"UMG"
		});
	}
}
