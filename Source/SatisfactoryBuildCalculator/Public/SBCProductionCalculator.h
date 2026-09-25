#pragma once

#include "CoreMinimal.h"

struct FSBCIngredientDefinition
{
	FString ItemId;
	double Amount = 0.0;
};

struct FSBCItemDefinition
{
	FString Id;
	FString NameKo;
	FString NameEn;
	FString Unit = TEXT("items");
	bool bRawResource = false;
};

struct FSBCBuildingDefinition
{
	FString Id;
	FString NameKo;
	FString NameEn;
	double BasePowerMW = 0.0;
	double PowerExponent = 1.321929;
	double BoostPowerExponent = 2.0;
	int32 SloopSlots = 0;
	double VariableMinMW = 0.0;
	double VariableMaxMW = 0.0;
};

struct FSBCRecipeDefinition
{
	FString Id;
	FString NameKo;
	FString NameEn;
	FString OutputItemId;
	double OutputAmount = 0.0;
	double DurationSeconds = 0.0;
	FString BuildingId;
	TArray<FSBCIngredientDefinition> Ingredients;
	TArray<FSBCIngredientDefinition> Byproducts;
	bool bDefault = false;
	bool bAlternate = false;
	FString Kind = TEXT("manufacturing");
	double GenerationMinMW = 0.0;
	double GenerationMaxMW = 0.0;
	double GridBoostFraction = 0.0;
	int32 SiteLimit = 0;
};

struct FSBCMachineSettings
{
	int32 PowerShards = 0;
	int32 Somersloops = 0;
};

struct FSBCProductionNode
{
	FString NodeId;
	FString ItemId;
	FString ItemName;
	FString RecipeId;
	FString RecipeName;
	FString BuildingId;
	FString BuildingName;
	FString Unit;
	double RequiredRate = 0.0;
	double ExactBuildingCount = 0.0;
	int32 InstalledBuildingCount = 0;
	int32 PowerShards = 0;
	int32 Somersloops = 0;
	double PowerMW = 0.0;
	double PeakPowerMW = 0.0;
	double GenerationMW = 0.0;
	double GenerationMinMW = 0.0;
	double GenerationMaxMW = 0.0;
	bool bRawResource = false;
	bool bGenerator = false;
	TArray<FSBCIngredientDefinition> Byproducts;
	TArray<TSharedPtr<FSBCProductionNode>> Children;
};

class SATISFACTORYBUILDCALCULATOR_API FSBCProductionData
{
public:
	bool Load(FString& OutError);
	bool SynchronizeRuntimeRecipes(UObject* WorldContext, FString& OutError);
	bool IsLoaded() const { return bLoaded; }
	const FSBCItemDefinition* FindItem(const FString& ItemId) const;
	const FSBCBuildingDefinition* FindBuilding(const FString& BuildingId) const;
	const FSBCRecipeDefinition* FindRecipe(const FString& RecipeId) const;
	const TArray<FString>* FindRecipesForItem(const FString& ItemId) const;
	const TMap<FString, FSBCItemDefinition>& GetItems() const { return Items; }
	const TMap<FString, FSBCRecipeDefinition>& GetRecipes() const { return Recipes; }
	int32 GetBuildingCount() const { return Buildings.Num(); }
	int32 GetRecipeCount() const { return Recipes.Num(); }
	int32 GetRuntimeRecipeCount() const { return RuntimeRecipeCount; }

private:
	bool bLoaded = false;
	int32 RuntimeRecipeCount = 0;
	TSet<FString> RuntimeRecipeIds;
	TMap<FString, FSBCItemDefinition> Items;
	TMap<FString, FSBCBuildingDefinition> Buildings;
	TMap<FString, FSBCRecipeDefinition> Recipes;
	TMap<FString, TArray<FString>> RecipesByOutput;
};

class SATISFACTORYBUILDCALCULATOR_API FSBCProductionCalculator
{
public:
	explicit FSBCProductionCalculator(const FSBCProductionData& InData) : Data(InData) {}

	TSharedPtr<FSBCProductionNode> Calculate(
		const FString& TargetItemId,
		double TargetRate,
		const TMap<FString, FString>& SelectedRecipes,
		const TMap<FString, FSBCMachineSettings>& MachineSettings,
		int32 DefaultPowerShards,
		bool bKorean,
		double ExistingGridMW,
		FString& OutError) const;

private:
	const FSBCProductionData& Data;

	TSharedPtr<FSBCProductionNode> BuildNode(
		const FString& ItemId,
		double Rate,
		const FString& NodeId,
		const TMap<FString, FString>& SelectedRecipes,
		const TMap<FString, FSBCMachineSettings>& MachineSettings,
		int32 DefaultPowerShards,
		bool bKorean,
		double ExistingGridMW,
		const TSet<FString>& Ancestry,
		FString& OutError) const;

	const FSBCRecipeDefinition* SelectRecipe(
		const FString& ItemId,
		const FString& NodeId,
		const TMap<FString, FString>& SelectedRecipes) const;
};
