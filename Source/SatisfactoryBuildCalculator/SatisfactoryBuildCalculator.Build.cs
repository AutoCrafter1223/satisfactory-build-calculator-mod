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
			"Json",
			"Projects",
			"InputCore",
			"Slate",
			"SlateCore",
			"UMG"
		});

		RuntimeDependencies.Add("$(PluginDir)/Data/items.json", StagedFileType.NonUFS);
		RuntimeDependencies.Add("$(PluginDir)/Data/recipes.json", StagedFileType.NonUFS);
		RuntimeDependencies.Add("$(PluginDir)/Data/buildings.json", StagedFileType.NonUFS);
		RuntimeDependencies.Add("$(PluginDir)/Data/import_metadata.json", StagedFileType.NonUFS);
	}
}
