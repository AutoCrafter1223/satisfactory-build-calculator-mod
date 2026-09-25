#include "SBCProductionCalculator.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	bool ReadJsonArray(const FString& Path, TArray<TSharedPtr<FJsonValue>>& OutValues, FString& OutError)
	{
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *Path))
		{
			OutError = FString::Printf(TEXT("Could not read calculator data: %s"), *Path);
			return false;
		}

		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		if (!FJsonSerializer::Deserialize(Reader, OutValues))
		{
			OutError = FString::Printf(TEXT("Invalid calculator data: %s"), *Path);
			return false;
		}
		return true;
	}

	double NumberOr(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, double DefaultValue = 0.0)
	{
		double Value = DefaultValue;
		Object->TryGetNumberField(Field, Value);
		return Value;
	}

	FString StringOr(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, const TCHAR* DefaultValue = TEXT(""))
	{
		FString Value;
		return Object->TryGetStringField(Field, Value) ? Value : FString(DefaultValue);
	}

	bool BoolOr(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, bool DefaultValue = false)
	{
		bool Value = DefaultValue;
		Object->TryGetBoolField(Field, Value);
		return Value;
	}

	void ReadIngredients(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, TArray<FSBCIngredientDefinition>& OutIngredients)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Object->TryGetArrayField(Field, Values) || !Values)
		{
			return;
		}
		for (const TSharedPtr<FJsonValue>& Value : *Values)
		{
			const TSharedPtr<FJsonObject> Entry = Value->AsObject();
			if (Entry)
			{
				OutIngredients.Add({StringOr(Entry, TEXT("item_id")), NumberOr(Entry, TEXT("amount"))});
			}
		}
	}

	FString Localized(const FString& Korean, const FString& English, bool bKorean)
	{
		return bKorean ? Korean : (English.IsEmpty() ? Korean : English);
	}
}

bool FSBCProductionData::Load(FString& OutError)
{
	bLoaded = false;
	Items.Reset();
	Buildings.Reset();
	Recipes.Reset();
	RecipesByOutput.Reset();

	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("SatisfactoryBuildCalculator"));
	if (!Plugin)
	{
		OutError = TEXT("SatisfactoryBuildCalculator plugin directory was not found.");
		return false;
	}
	const FString DataDirectory = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Data"));
	TArray<TSharedPtr<FJsonValue>> Values;

	if (!ReadJsonArray(FPaths::Combine(DataDirectory, TEXT("items.json")), Values, OutError))
	{
		return false;
	}
	for (const TSharedPtr<FJsonValue>& Value : Values)
	{
		const TSharedPtr<FJsonObject> Object = Value->AsObject();
		if (!Object) continue;
		FSBCItemDefinition Item;
		Item.Id = StringOr(Object, TEXT("id"));
		Item.NameKo = StringOr(Object, TEXT("name_ko"));
		Item.NameEn = StringOr(Object, TEXT("name_en"));
		Item.Unit = StringOr(Object, TEXT("unit"), TEXT("items"));
		Item.bRawResource = BoolOr(Object, TEXT("is_raw_resource"));
		if (!Item.Id.IsEmpty()) Items.Add(Item.Id, MoveTemp(Item));
	}

	Values.Reset();
	if (!ReadJsonArray(FPaths::Combine(DataDirectory, TEXT("buildings.json")), Values, OutError))
	{
		return false;
	}
	for (const TSharedPtr<FJsonValue>& Value : Values)
	{
		const TSharedPtr<FJsonObject> Object = Value->AsObject();
		if (!Object) continue;
		FSBCBuildingDefinition Building;
		Building.Id = StringOr(Object, TEXT("id"));
		Building.NameKo = StringOr(Object, TEXT("name_ko"));
		Building.NameEn = StringOr(Object, TEXT("name_en"));
		Building.BasePowerMW = NumberOr(Object, TEXT("base_power_mw"));
		Building.PowerExponent = NumberOr(Object, TEXT("power_exponent"), 1.321929);
		Building.BoostPowerExponent = NumberOr(Object, TEXT("boost_power_exponent"), 2.0);
		Building.SloopSlots = static_cast<int32>(NumberOr(Object, TEXT("sloop_slots")));
		Building.VariableMinMW = NumberOr(Object, TEXT("variable_min_mw"));
		Building.VariableMaxMW = NumberOr(Object, TEXT("variable_max_mw"));
		if (!Building.Id.IsEmpty()) Buildings.Add(Building.Id, MoveTemp(Building));
	}

	Values.Reset();
	if (!ReadJsonArray(FPaths::Combine(DataDirectory, TEXT("recipes.json")), Values, OutError))
	{
		return false;
	}
	for (const TSharedPtr<FJsonValue>& Value : Values)
	{
		const TSharedPtr<FJsonObject> Object = Value->AsObject();
		if (!Object) continue;
		FSBCRecipeDefinition Recipe;
		Recipe.Id = StringOr(Object, TEXT("id"));
		Recipe.NameKo = StringOr(Object, TEXT("name"));
		Recipe.NameEn = StringOr(Object, TEXT("name_en"));
		Recipe.OutputItemId = StringOr(Object, TEXT("output_item_id"));
		Recipe.OutputAmount = NumberOr(Object, TEXT("output_amount"));
		Recipe.DurationSeconds = NumberOr(Object, TEXT("duration_seconds"));
		Recipe.BuildingId = StringOr(Object, TEXT("building_id"));
		Recipe.bDefault = BoolOr(Object, TEXT("is_default"));
		Recipe.bAlternate = BoolOr(Object, TEXT("is_alternate"));
		Recipe.Kind = StringOr(Object, TEXT("kind"), TEXT("manufacturing"));
		Recipe.GenerationMinMW = NumberOr(Object, TEXT("generation_min_mw"));
		Recipe.GenerationMaxMW = NumberOr(Object, TEXT("generation_max_mw"));
		Recipe.GridBoostFraction = NumberOr(Object, TEXT("grid_boost_fraction"));
		Recipe.SiteLimit = static_cast<int32>(NumberOr(Object, TEXT("site_limit")));
		ReadIngredients(Object, TEXT("ingredients"), Recipe.Ingredients);
		ReadIngredients(Object, TEXT("byproducts"), Recipe.Byproducts);
		if (!Recipe.Id.IsEmpty() && !Recipe.OutputItemId.IsEmpty())
		{
			RecipesByOutput.FindOrAdd(Recipe.OutputItemId).Add(Recipe.Id);
			Recipes.Add(Recipe.Id, MoveTemp(Recipe));
		}
	}

	bLoaded = Items.Num() > 0 && Buildings.Num() > 0 && Recipes.Num() > 0;
	if (!bLoaded) OutError = TEXT("Calculator data is empty.");
	return bLoaded;
}

const FSBCItemDefinition* FSBCProductionData::FindItem(const FString& ItemId) const { return Items.Find(ItemId); }
const FSBCBuildingDefinition* FSBCProductionData::FindBuilding(const FString& BuildingId) const { return Buildings.Find(BuildingId); }
const FSBCRecipeDefinition* FSBCProductionData::FindRecipe(const FString& RecipeId) const { return Recipes.Find(RecipeId); }
const TArray<FString>* FSBCProductionData::FindRecipesForItem(const FString& ItemId) const { return RecipesByOutput.Find(ItemId); }

TSharedPtr<FSBCProductionNode> FSBCProductionCalculator::Calculate(
	const FString& TargetItemId,
	double TargetRate,
	const TMap<FString, FString>& SelectedRecipes,
	const TMap<FString, FSBCMachineSettings>& MachineSettings,
	int32 DefaultPowerShards,
	bool bKorean,
	double ExistingGridMW,
	FString& OutError) const
{
	OutError.Reset();
	if (!Data.FindItem(TargetItemId))
	{
		OutError = bKorean ? TEXT("존재하지 않는 아이템입니다.") : TEXT("Unknown item.");
		return nullptr;
	}
	if (TargetRate <= 0.0)
	{
		OutError = bKorean ? TEXT("목표 생산량은 0보다 커야 합니다.") : TEXT("Target must be greater than zero.");
		return nullptr;
	}
	if (DefaultPowerShards < 0 || DefaultPowerShards > 3)
	{
		OutError = bKorean ? TEXT("동력핵은 0~3개여야 합니다.") : TEXT("Power Shards must be 0-3.");
		return nullptr;
	}
	if (ExistingGridMW < 0.0)
	{
		OutError = bKorean ? TEXT("기존 전력망 발전량은 0 이상이어야 합니다.") : TEXT("Existing grid power cannot be negative.");
		return nullptr;
	}
	return BuildNode(TargetItemId, TargetRate, TEXT("root"), SelectedRecipes, MachineSettings,
		DefaultPowerShards, bKorean, ExistingGridMW, {}, OutError);
}

const FSBCRecipeDefinition* FSBCProductionCalculator::SelectRecipe(
	const FString& ItemId,
	const FString& NodeId,
	const TMap<FString, FString>& SelectedRecipes) const
{
	if (const FString* SelectedId = SelectedRecipes.Find(NodeId))
	{
		if (const FSBCRecipeDefinition* Selected = Data.FindRecipe(*SelectedId); Selected && Selected->OutputItemId == ItemId)
		{
			return Selected;
		}
	}
	const TArray<FString>* Candidates = Data.FindRecipesForItem(ItemId);
	if (!Candidates) return nullptr;
	for (const FString& RecipeId : *Candidates)
	{
		const FSBCRecipeDefinition* Recipe = Data.FindRecipe(RecipeId);
		if (Recipe && Recipe->bDefault) return Recipe;
	}
	return Candidates->IsEmpty() ? nullptr : Data.FindRecipe((*Candidates)[0]);
}

TSharedPtr<FSBCProductionNode> FSBCProductionCalculator::BuildNode(
	const FString& ItemId,
	double Rate,
	const FString& NodeId,
	const TMap<FString, FString>& SelectedRecipes,
	const TMap<FString, FSBCMachineSettings>& MachineSettings,
	int32 DefaultPowerShards,
	bool bKorean,
	double ExistingGridMW,
	const TSet<FString>& Ancestry,
	FString& OutError) const
{
	const FSBCItemDefinition* Item = Data.FindItem(ItemId);
	if (!Item) return nullptr;
	if (Ancestry.Contains(ItemId))
	{
		OutError = bKorean ? FString::Printf(TEXT("순환 레시피를 감지했습니다: %s"), *Item->NameKo)
			: FString::Printf(TEXT("Cyclic recipe: %s"), *Item->NameEn);
		return nullptr;
	}

	TSharedPtr<FSBCProductionNode> Node = MakeShared<FSBCProductionNode>();
	Node->NodeId = NodeId;
	Node->ItemId = ItemId;
	Node->ItemName = Localized(Item->NameKo, Item->NameEn, bKorean);
	Node->RequiredRate = Rate;
	Node->Unit = Item->Unit;
	Node->bRawResource = Item->bRawResource;
	if (Item->bRawResource) return Node;

	const FSBCRecipeDefinition* Recipe = SelectRecipe(ItemId, NodeId, SelectedRecipes);
	if (!Recipe)
	{
		OutError = bKorean ? FString::Printf(TEXT("%s의 레시피가 없습니다."), *Item->NameKo)
			: FString::Printf(TEXT("No recipe for %s."), *Item->NameEn);
		return nullptr;
	}
	const FSBCBuildingDefinition* Building = Data.FindBuilding(Recipe->BuildingId);
	if (!Building || Recipe->OutputAmount <= 0.0 || Recipe->DurationSeconds <= 0.0)
	{
		OutError = bKorean ? TEXT("레시피의 생산시설 데이터가 올바르지 않습니다.") : TEXT("The recipe building data is invalid.");
		return nullptr;
	}

	Node->RecipeId = Recipe->Id;
	Node->RecipeName = Localized(Recipe->NameKo, Recipe->NameEn, bKorean);
	Node->BuildingId = Building->Id;
	Node->BuildingName = Localized(Building->NameKo, Building->NameEn, bKorean);
	Node->bGenerator = Recipe->Kind == TEXT("generator") || Recipe->Kind == TEXT("geothermal") || Recipe->Kind == TEXT("augmenter");
	const FSBCMachineSettings Settings = MachineSettings.FindRef(NodeId);
	Node->PowerShards = Node->bGenerator ? 0 : FMath::Clamp(MachineSettings.Contains(NodeId) ? Settings.PowerShards : DefaultPowerShards, 0, 3);
	Node->Somersloops = Node->bGenerator ? 0 : FMath::Clamp(Settings.Somersloops, 0, Building->SloopSlots);
	const double Clock = 1.0 + 0.5 * Node->PowerShards;
	const double Boost = Building->SloopSlots > 0 ? 1.0 + static_cast<double>(Node->Somersloops) / Building->SloopSlots : 1.0;
	const double PerMachineRate = Recipe->OutputAmount * 60.0 / Recipe->DurationSeconds;
	Node->ExactBuildingCount = Rate / (PerMachineRate * Clock * Boost);
	Node->InstalledBuildingCount = FMath::CeilToInt(Node->ExactBuildingCount - 1e-10);

	if (Recipe->Kind == TEXT("geothermal"))
	{
		if (Recipe->SiteLimit > 0 && Node->InstalledBuildingCount > Recipe->SiteLimit)
		{
			OutError = bKorean ? TEXT("사용 가능한 간헐천 수를 초과했습니다.") : TEXT("The available geyser limit was exceeded.");
			return nullptr;
		}
		Node->GenerationMW = Node->InstalledBuildingCount * PerMachineRate;
		Node->GenerationMinMW = Node->InstalledBuildingCount * Recipe->GenerationMinMW;
		Node->GenerationMaxMW = Node->InstalledBuildingCount * Recipe->GenerationMaxMW;
	}
	else if (Recipe->Kind == TEXT("augmenter"))
	{
		Node->InstalledBuildingCount = 0;
		for (int32 Count = 1; Count <= Recipe->SiteLimit; ++Count)
		{
			const double Added = (ExistingGridMW + Count * PerMachineRate) * (1.0 + Count * Recipe->GridBoostFraction) - ExistingGridMW;
			if (Added >= Rate)
			{
				Node->InstalledBuildingCount = Count;
				Node->GenerationMW = Node->GenerationMinMW = Node->GenerationMaxMW = Added;
				break;
			}
		}
		if (Node->InstalledBuildingCount == 0)
		{
			OutError = bKorean ? TEXT("사용 가능한 외계 전력 증폭기 수로 목표를 달성할 수 없습니다.")
				: TEXT("This target exceeds the available Power Augmenters.");
			return nullptr;
		}
		Node->ExactBuildingCount = Node->InstalledBuildingCount;
	}
	else if (Recipe->Kind == TEXT("generator"))
	{
		Node->GenerationMW = Node->GenerationMinMW = Node->GenerationMaxMW = Rate;
	}
	else
	{
		for (int32 MachineIndex = 0; MachineIndex < Node->InstalledBuildingCount; ++MachineIndex)
		{
			const double FractionalLoad = FMath::Clamp(Node->ExactBuildingCount - MachineIndex, 0.0, 1.0);
			const double MachineClock = FMath::Max(0.01, Clock * FractionalLoad);
			const double BoostPower = FMath::Pow(Boost, Building->BoostPowerExponent);
			const double AverageBase = Building->VariableMaxMW > 0.0
				? (Building->VariableMinMW + Building->VariableMaxMW) / 2.0 : Building->BasePowerMW;
			const double PeakBase = Building->VariableMaxMW > 0.0 ? Building->VariableMaxMW : Building->BasePowerMW;
			Node->PowerMW += AverageBase * FMath::Pow(MachineClock, Building->PowerExponent) * BoostPower;
			Node->PeakPowerMW += PeakBase * FMath::Pow(MachineClock, Building->PowerExponent) * BoostPower;
		}
	}

	for (const FSBCIngredientDefinition& Byproduct : Recipe->Byproducts)
	{
		Node->Byproducts.Add({Byproduct.ItemId, Byproduct.Amount * Rate / Recipe->OutputAmount});
	}

	TSet<FString> NextAncestry = Ancestry;
	NextAncestry.Add(ItemId);
	for (int32 Index = 0; Index < Recipe->Ingredients.Num(); ++Index)
	{
		const FSBCIngredientDefinition& Ingredient = Recipe->Ingredients[Index];
		const double RequiredRate = Recipe->Kind == TEXT("augmenter")
			? Node->InstalledBuildingCount * Ingredient.Amount
			: Rate * Ingredient.Amount / (Recipe->OutputAmount * Boost);
		const FString ChildId = FString::Printf(TEXT("%s/%d:%s"), *NodeId, Index, *Ingredient.ItemId);
		TSharedPtr<FSBCProductionNode> Child = BuildNode(Ingredient.ItemId, RequiredRate, ChildId, SelectedRecipes,
			MachineSettings, DefaultPowerShards, bKorean, ExistingGridMW, NextAncestry, OutError);
		if (!Child) return nullptr;
		Node->Children.Add(MoveTemp(Child));
	}
	return Node;
}
