#include "SBCCalculatorWidget.h"

#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "SatisfactoryBuildCalculator.h"
#include "SBCProductionCalculator.h"
#include "FGRecipe.h"
#include "FGRecipeManager.h"
#include "Resources/FGItemDescriptor.h"
#include "Buildables/FGBuildable.h"
#include "Framework/Application/SlateApplication.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	void GatherProductionSummary(
		const TSharedPtr<FSBCProductionNode>& Node,
		TMap<FString, TPair<double, FString>>& RawResources,
		int32& Machines,
		double& ConsumptionMW,
		double& GenerationMW)
	{
		if (!Node) return;
		if (Node->bRawResource)
		{
			TPair<double, FString>& Entry = RawResources.FindOrAdd(Node->ItemName);
			Entry.Key += Node->RequiredRate;
			Entry.Value = Node->Unit;
		}
		else
		{
			Machines += Node->InstalledBuildingCount;
			ConsumptionMW += Node->PowerMW;
			GenerationMW += Node->GenerationMW;
		}
		for (const TSharedPtr<FSBCProductionNode>& Child : Node->Children)
		{
			GatherProductionSummary(Child, RawResources, Machines, ConsumptionMW, GenerationMW);
		}
	}

	void GatherGoalPlan(const TSharedPtr<FSBCProductionNode>& Node, TMap<FString, FSBCCalculatedGoalRequest>& Requests)
	{
		if (!Node) return;
		if (!Node->bRawResource && !Node->BuildingId.IsEmpty() && Node->InstalledBuildingCount > 0)
		{
			const FString Identity = Node->bGenerator ? Node->BuildingId : Node->RecipeId;
			const FString Key = FString::Printf(TEXT("%s|%d|%d"), *Identity, Node->PowerShards, Node->Somersloops);
			FSBCCalculatedGoalRequest& Request = Requests.FindOrAdd(Key);
			Request.RecipeId = Node->RecipeId;
			Request.BuildingId = Node->BuildingId;
			Request.TargetCount += Node->InstalledBuildingCount;
			Request.PowerShards = Node->PowerShards;
			Request.Somersloops = Node->Somersloops;
			Request.bRequiresRecipe = !Node->bGenerator;
		}
		for (const TSharedPtr<FSBCProductionNode>& Child : Node->Children)
		{
			GatherGoalPlan(Child, Requests);
		}
	}
}

FText USBCCalculatorWidget::Text(const TCHAR* Korean, const TCHAR* English) const
{
	return FText::FromString(bKorean ? Korean : English);
}

TSharedRef<SWidget> USBCCalculatorWidget::RebuildWidget()
{
	bKorean = FInternationalization::Get().GetCurrentCulture()->GetTwoLetterISOLanguageName() == TEXT("ko");

	TSharedRef<SWidget> Result =
		SAssignNew(RootSizeBox, SBox)
		.WidthOverride(PanelSize.X)
		.HeightOverride(PanelSize.Y)
		[
			SNew(SBorder)
			.Padding(16.0f)
			.BorderBackgroundColor(FLinearColor(0.025f, 0.055f, 0.07f, 0.68f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 12.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.0f)
					[
						SNew(STextBlock)
						.Text(Text(TEXT("세티스팩토리 빌드 계산기"), TEXT("Satisfactory Build Calculator")))
						.ColorAndOpacity(FLinearColor(1.0f, 0.62f, 0.14f))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 22))
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 10.0f, 0.0f)
						[
							SNew(STextBlock)
							.Text_Lambda([this]() { return GetRecipeSyncStatus(); })
							.ColorAndOpacity(FLinearColor(0.38f, 0.86f, 0.72f))
							.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
						[
							SNew(SButton)
							.Text_Lambda([this]()
							{
								return bCompactView
									? Text(TEXT("상세 보기"), TEXT("Detailed view"))
									: Text(TEXT("간소화 보기"), TEXT("Compact view"));
							})
							.OnClicked_Lambda([this]()
							{
								bCompactView = !bCompactView;
								CalculateSelected();
								return FReply::Handled();
							})
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
						[
							SNew(SButton)
							.Text(Text(TEXT("새로고침"), TEXT("Refresh")))
							.OnClicked_Lambda([this]()
							{
								RefreshGameState();
								return FReply::Handled();
							})
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(Text(TEXT("F6 / ESC 닫기"), TEXT("F6 / ESC Close")))
							.ColorAndOpacity(FLinearColor(0.62f, 0.72f, 0.77f))
							.Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))
						]
					]
				]
				+ SVerticalBox::Slot().FillHeight(1.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(0.25f).Padding(0.0f, 0.0f, 12.0f, 0.0f)
					[
						SNew(SBorder)
						.Padding(10.0f)
						.BorderBackgroundColor(FLinearColor(0.04f, 0.08f, 0.10f, 0.66f))
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
							[
								SAssignNew(SearchBox, SEditableTextBox)
								.HintText(Text(TEXT("제품 또는 발전기 검색"), TEXT("Search products or generators")))
								.OnTextChanged_Lambda([this](const FText& NewText)
								{
									SearchText = NewText.ToString();
									RefreshProductList();
								})
							]
							+ SVerticalBox::Slot().FillHeight(1.0f)
							[
								SNew(SScrollBox)
								+ SScrollBox::Slot()
								[
									SAssignNew(ProductListBox, SVerticalBox)
								]
							]
						]
					]
					+ SHorizontalBox::Slot().FillWidth(0.75f)
					[
						SNew(SBorder)
						.Padding(12.0f)
						.BorderBackgroundColor(FLinearColor(0.04f, 0.08f, 0.10f, 0.60f))
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 10.0f, 0.0f)
								[
									SNew(STextBlock)
									.Text(Text(TEXT("목표 생산량"), TEXT("Target rate")))
									.Font(FCoreStyle::GetDefaultFontStyle("Regular", 12))
								]
								+ SHorizontalBox::Slot().FillWidth(1.0f)
								[
									SNew(SSpinBox<double>)
									.MinValue(0.01)
									.MaxValue(1000000.0)
									.Value(TargetRate)
									.OnValueChanged_Lambda([this](double NewValue)
									{
										TargetRate = NewValue;
									})
								]
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.0f, 0.0f, 5.0f, 0.0f)
								[
									SNew(STextBlock)
									.Text(Text(TEXT("전체 동력핵"), TEXT("Global shards")))
									.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
								]
								+ SHorizontalBox::Slot().AutoWidth().Padding(1.0f)
								[
									SNew(SButton).Text(FText::FromString(TEXT("0"))).OnClicked_Lambda([this]() { SetDefaultPowerShards(0); return FReply::Handled(); })
								]
								+ SHorizontalBox::Slot().AutoWidth().Padding(1.0f)
								[
									SNew(SButton).Text(FText::FromString(TEXT("1"))).OnClicked_Lambda([this]() { SetDefaultPowerShards(1); return FReply::Handled(); })
								]
								+ SHorizontalBox::Slot().AutoWidth().Padding(1.0f)
								[
									SNew(SButton).Text(FText::FromString(TEXT("2"))).OnClicked_Lambda([this]() { SetDefaultPowerShards(2); return FReply::Handled(); })
								]
								+ SHorizontalBox::Slot().AutoWidth().Padding(1.0f, 1.0f, 8.0f, 1.0f)
								[
									SNew(SButton).Text(FText::FromString(TEXT("3"))).OnClicked_Lambda([this]() { SetDefaultPowerShards(3); return FReply::Handled(); })
								]
								+ SHorizontalBox::Slot().AutoWidth().Padding(10.0f, 0.0f, 0.0f, 0.0f)
								[
									SNew(SButton)
									.Text(Text(TEXT("계산"), TEXT("Calculate")))
									.OnClicked_Lambda([this]()
									{
										CalculateSelected();
										return FReply::Handled();
									})
								]
								+ SHorizontalBox::Slot().AutoWidth().Padding(8.0f, 0.0f, 0.0f, 0.0f)
								[
									SNew(SButton)
									.Text(Text(TEXT("목표 추가"), TEXT("Add goals")))
									.OnClicked_Lambda([this]()
									{
										OnRequestAddGoals.ExecuteIfBound();
										return FReply::Handled();
									})
								]
							]
							+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)
							[
								SNew(SBorder)
								.Padding(FMargin(10.0f, 7.0f))
								.BorderBackgroundColor(FLinearColor(0.08f, 0.12f, 0.13f, 0.82f))
								[
									SAssignNew(SummaryBox, SVerticalBox)
								]
							]
							+ SVerticalBox::Slot().FillHeight(1.0f)
							[
								SNew(SScrollBox)
								+ SScrollBox::Slot()
								[
									SNew(SScrollBox)
									.Orientation(Orient_Horizontal)
									+ SScrollBox::Slot()
									[
										SAssignNew(ResultColumnsBox, SHorizontalBox)
									]
								]
							]
						]
					]
				]
			]
		];

	RefreshGameState();
	if (SelectedItemId.IsEmpty())
	{
		SelectedItemId = TEXT("Desc_ComputerSuper_C");
	}
	CalculateSelected();
	return Result;
}

FReply USBCCalculatorWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		OnRequestClose.ExecuteIfBound();
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

void USBCCalculatorWidget::FocusSearchBox()
{
	if (SearchBox.IsValid())
	{
		FSlateApplication::Get().SetKeyboardFocus(SearchBox, EFocusCause::SetDirectly);
	}
}

void USBCCalculatorWidget::SetPanelSize(const FVector2D& NewSize)
{
	if (PanelSize.Equals(NewSize, 0.5f))
	{
		return;
	}
	PanelSize = NewSize;
	if (RootSizeBox.IsValid())
	{
		RootSizeBox->SetWidthOverride(PanelSize.X);
		RootSizeBox->SetHeightOverride(PanelSize.Y);
	}
}

void USBCCalculatorWidget::RefreshGameState()
{
	UnlockedRecipeIds.Reset();
	UnlockedItemIds.Reset();
	UnlockedBuildingIds.Reset();
	bRecipeStateReady = false;
	if (AFGRecipeManager* RecipeManager = AFGRecipeManager::Get(this))
	{
		if (FSatisfactoryBuildCalculatorModule* Module = FModuleManager::GetModulePtr<FSatisfactoryBuildCalculatorModule>(TEXT("SatisfactoryBuildCalculator")))
		{
			if (const TSharedPtr<FSBCProductionData> Data = Module->GetProductionData())
			{
				FString SyncError;
				Data->SynchronizeRuntimeRecipes(this, SyncError);
			}
		}
		for (const TSubclassOf<UFGRecipe>& RecipeClass : RecipeManager->GetAllAvailableRecipes())
		{
			if (RecipeClass)
			{
				UnlockedRecipeIds.Add(RecipeClass->GetName());
			}
		}
		for (const TSubclassOf<UFGItemDescriptor>& ItemClass : RecipeManager->GetAllAvailableItemDescriptors())
		{
			if (ItemClass)
			{
				UnlockedItemIds.Add(ItemClass->GetName());
			}
		}
		for (const TSubclassOf<AFGBuildable>& BuildingClass : RecipeManager->GetAvailableBuildingsOfType<AFGBuildable>())
		{
			if (BuildingClass)
			{
				UnlockedBuildingIds.Add(BuildingClass->GetName());
			}
		}
		bRecipeStateReady = true;
	}
	RefreshProductList();
	CalculateSelected();
}

bool USBCCalculatorWidget::IsRecipeUnlocked(const FSBCRecipeDefinition& Recipe) const
{
	if (!bRecipeStateReady)
	{
		return true;
	}
	if (Recipe.Kind == TEXT("manufacturing"))
	{
		return UnlockedRecipeIds.Contains(Recipe.Id);
	}
	if (!UnlockedBuildingIds.Contains(Recipe.BuildingId))
	{
		return false;
	}
	for (const FSBCIngredientDefinition& Ingredient : Recipe.Ingredients)
	{
		if (!UnlockedItemIds.Contains(Ingredient.ItemId))
		{
			return false;
		}
	}
	return true;
}

bool USBCCalculatorWidget::IsProductUnlocked(const FString& ItemId) const
{
	FSatisfactoryBuildCalculatorModule* Module = FModuleManager::GetModulePtr<FSatisfactoryBuildCalculatorModule>(TEXT("SatisfactoryBuildCalculator"));
	const TSharedPtr<FSBCProductionData> Data = Module ? Module->GetProductionData() : nullptr;
	const TArray<FString>* Recipes = Data ? Data->FindRecipesForItem(ItemId) : nullptr;
	if (!Recipes || !bRecipeStateReady)
	{
		return !bRecipeStateReady;
	}
	for (const FString& RecipeId : *Recipes)
	{
		if (const FSBCRecipeDefinition* Recipe = Data->FindRecipe(RecipeId); Recipe && IsRecipeUnlocked(*Recipe))
		{
			return true;
		}
	}
	return false;
}

FText USBCCalculatorWidget::GetRecipeSyncStatus() const
{
	if (!bRecipeStateReady)
	{
		return Text(TEXT("제조법 동기화 대기"), TEXT("Recipe sync pending"));
	}
	FSatisfactoryBuildCalculatorModule* Module = FModuleManager::GetModulePtr<FSatisfactoryBuildCalculatorModule>(TEXT("SatisfactoryBuildCalculator"));
	const TSharedPtr<FSBCProductionData> Data = Module ? Module->GetProductionData() : nullptr;
	const int32 RuntimeCount = Data ? Data->GetRuntimeRecipeCount() : 0;
	return FText::FromString(bKorean
		? FString::Printf(TEXT("해금 %d개 · 추가 제조법 %d개"), UnlockedRecipeIds.Num(), RuntimeCount)
		: FString::Printf(TEXT("%d unlocked · %d additional recipes"), UnlockedRecipeIds.Num(), RuntimeCount));
}

void USBCCalculatorWidget::RefreshProductList()
{
	if (!ProductListBox.IsValid()) return;
	ProductListBox->ClearChildren();
	FSatisfactoryBuildCalculatorModule* Module = FModuleManager::GetModulePtr<FSatisfactoryBuildCalculatorModule>(TEXT("SatisfactoryBuildCalculator"));
	const TSharedPtr<FSBCProductionData> Data = Module ? Module->GetProductionData() : nullptr;
	if (!Data || !Data->IsLoaded()) return;

	TArray<const FSBCItemDefinition*> Matches;
	for (const TPair<FString, FSBCItemDefinition>& Pair : Data->GetItems())
	{
		const FSBCItemDefinition& Item = Pair.Value;
		if (!Data->FindRecipesForItem(Item.Id)) continue;
		const FString Name = bKorean ? Item.NameKo : Item.NameEn;
		if (SearchText.IsEmpty() || Name.Contains(SearchText, ESearchCase::IgnoreCase))
		{
			Matches.Add(&Item);
		}
	}
	Matches.Sort([this](const FSBCItemDefinition& A, const FSBCItemDefinition& B)
	{
		return (bKorean ? A.NameKo : A.NameEn) < (bKorean ? B.NameKo : B.NameEn);
	});

	for (const FSBCItemDefinition* Item : Matches)
	{
		const FString ItemId = Item->Id;
		const bool bUnlocked = IsProductUnlocked(ItemId);
		const FString DisplayName = bKorean ? Item->NameKo : Item->NameEn;
		const FText Name = FText::FromString(bUnlocked
			? DisplayName
			: FString::Printf(TEXT("%s  [%s]"), *DisplayName, bKorean ? TEXT("잠김") : TEXT("Locked")));
		ProductListBox->AddSlot().AutoHeight().Padding(0.0f, 1.0f)
		[
			SNew(SButton)
			.IsEnabled(bUnlocked)
			.ContentPadding(FMargin(8.0f, 5.0f))
			.OnClicked_Lambda([this, ItemId]()
			{
				SelectProduct(ItemId);
				return FReply::Handled();
			})
			[
				SNew(STextBlock)
				.Text(Name)
				.Font(FCoreStyle::GetDefaultFontStyle(SelectedItemId == ItemId ? "Bold" : "Regular", 11))
			]
		];
	}
}

void USBCCalculatorWidget::SelectProduct(const FString& ItemId)
{
	if (!IsProductUnlocked(ItemId)) return;
	SelectedItemId = ItemId;
	SelectedRecipes.Reset();
	MachineSettings.Reset();
	CollapsedNodeIds.Reset();
	RefreshProductList();
	CalculateSelected();
}

void USBCCalculatorWidget::CalculateSelected()
{
	if (!ResultColumnsBox.IsValid()) return;
	LastCalculatedRoot.Reset();
	ResultColumnsBox->ClearChildren();
	ResultColumns.Reset();
	if (SummaryBox.IsValid()) SummaryBox->ClearChildren();
	FSatisfactoryBuildCalculatorModule* Module = FModuleManager::GetModulePtr<FSatisfactoryBuildCalculatorModule>(TEXT("SatisfactoryBuildCalculator"));
	const TSharedPtr<FSBCProductionData> Data = Module ? Module->GetProductionData() : nullptr;
	if (!Data || !Data->IsLoaded())
	{
		EnsureResultColumn(0)->AddSlot().AutoHeight()[SNew(STextBlock).Text(Text(TEXT("계산 데이터를 불러오지 못했습니다."), TEXT("Calculator data could not be loaded.")))];
		return;
	}
	if (SelectedItemId.IsEmpty())
	{
		EnsureResultColumn(0)->AddSlot().AutoHeight()[SNew(STextBlock).Text(Text(TEXT("왼쪽에서 제품을 선택하세요."), TEXT("Select a product on the left.")))];
		return;
	}
	if (!IsProductUnlocked(SelectedItemId))
	{
		EnsureResultColumn(0)->AddSlot().AutoHeight()
		[
			SNew(STextBlock)
			.Text(Text(TEXT("아직 해금되지 않은 제품입니다."), TEXT("This product has not been unlocked yet.")))
			.ColorAndOpacity(FLinearColor(1.0f, 0.62f, 0.14f))
		];
		return;
	}

	FSBCProductionCalculator Calculator(*Data);
	FString Error;
	const TSharedPtr<FSBCProductionNode> Root = Calculator.Calculate(SelectedItemId, TargetRate, SelectedRecipes, MachineSettings, DefaultPowerShards, bKorean, 0.0, Error);
	if (!Root)
	{
		EnsureResultColumn(0)->AddSlot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(Error)).ColorAndOpacity(FLinearColor(1.0f, 0.35f, 0.25f))];
		return;
	}
	LastCalculatedRoot = Root;
	AddResultNode(Root, 0);
	RefreshSummary(Root);
}

TArray<FSBCCalculatedGoalRequest> USBCCalculatorWidget::GetGoalPlan() const
{
	TMap<FString, FSBCCalculatedGoalRequest> Requests;
	GatherGoalPlan(LastCalculatedRoot, Requests);
	TArray<FSBCCalculatedGoalRequest> Result;
	Requests.GenerateValueArray(Result);
	return Result;
}

void USBCCalculatorWidget::RefreshSummary(const TSharedPtr<FSBCProductionNode>& Root)
{
	if (!SummaryBox.IsValid()) return;
	SummaryBox->ClearChildren();
	TMap<FString, TPair<double, FString>> RawResources;
	int32 Machines = 0;
	double ConsumptionMW = 0.0;
	double GenerationMW = 0.0;
	GatherProductionSummary(Root, RawResources, Machines, ConsumptionMW, GenerationMW);

	SummaryBox->AddSlot().AutoHeight()
	[
		SNew(STextBlock)
		.Text(FText::FromString(bKorean
			? FString::Printf(TEXT("생산 요약   설치 %d대   소비 %.1f MW   발전 %.1f MW"), Machines, ConsumptionMW, GenerationMW)
			: FString::Printf(TEXT("Production summary   %d machines   %.1f MW used   %.1f MW generated"), Machines, ConsumptionMW, GenerationMW)))
		.ColorAndOpacity(FLinearColor(1.0f, 0.66f, 0.18f))
		.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
	];

	if (!RawResources.IsEmpty())
	{
		TArray<FString> Parts;
		for (const TPair<FString, TPair<double, FString>>& Pair : RawResources)
		{
			const FString Unit = Pair.Value.Value == TEXT("m3") ? TEXT("m³/min") : (bKorean ? TEXT("개/min") : TEXT("/min"));
			Parts.Add(FString::Printf(TEXT("%s %.2f %s"), *Pair.Key, Pair.Value.Key, *Unit));
		}
		Parts.Sort();
		SummaryBox->AddSlot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString((bKorean ? TEXT("원자재: ") : TEXT("Raw: ")) + FString::Join(Parts, TEXT("  ·  "))))
			.ColorAndOpacity(FLinearColor(0.68f, 0.78f, 0.80f))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
			.AutoWrapText(true)
		];
	}
}

TSharedRef<SWidget> USBCCalculatorWidget::BuildRecipeMenu(FString NodeId, FString ItemId)
{
	TSharedRef<SVerticalBox> Menu = SNew(SVerticalBox);
	FSatisfactoryBuildCalculatorModule* Module = FModuleManager::GetModulePtr<FSatisfactoryBuildCalculatorModule>(TEXT("SatisfactoryBuildCalculator"));
	const TSharedPtr<FSBCProductionData> Data = Module ? Module->GetProductionData() : nullptr;
	const TArray<FString>* CandidateIds = Data ? Data->FindRecipesForItem(ItemId) : nullptr;
	if (!CandidateIds)
	{
		return Menu;
	}
	for (const FString& RecipeId : *CandidateIds)
	{
		const FSBCRecipeDefinition* Recipe = Data->FindRecipe(RecipeId);
		if (!Recipe) continue;
		const bool bUnlocked = IsRecipeUnlocked(*Recipe);
		const FString Label = bKorean ? Recipe->NameKo : Recipe->NameEn;
		Menu->AddSlot().AutoHeight().Padding(2.0f)
		[
			SNew(SButton)
			.IsEnabled(bUnlocked)
			.ContentPadding(FMargin(8.0f, 4.0f))
			.Text(FText::FromString(bUnlocked
				? Label
				: FString::Printf(TEXT("%s  [%s]"), *Label, bKorean ? TEXT("잠김") : TEXT("Locked"))))
			.OnClicked_Lambda([this, NodeId, RecipeId]()
			{
				SelectRecipeForNode(NodeId, RecipeId);
				return FReply::Handled();
			})
		];
	}
	return SNew(SBorder).Padding(4.0f).BorderBackgroundColor(FLinearColor(0.025f, 0.055f, 0.07f, 0.96f))[Menu];
}

void USBCCalculatorWidget::SelectRecipeForNode(const FString& NodeId, const FString& RecipeId)
{
	SelectedRecipes.Add(NodeId, RecipeId);
	CalculateSelected();
}

void USBCCalculatorWidget::SetNodePowerShards(const FString& NodeId, int32 Count)
{
	MachineSettings.FindOrAdd(NodeId).PowerShards = FMath::Clamp(Count, 0, 3);
	CalculateSelected();
}

void USBCCalculatorWidget::SetNodeSomersloops(const FString& NodeId, int32 Count)
{
	MachineSettings.FindOrAdd(NodeId).Somersloops = FMath::Max(0, Count);
	CalculateSelected();
}

void USBCCalculatorWidget::SetDefaultPowerShards(int32 Count)
{
	DefaultPowerShards = FMath::Clamp(Count, 0, 3);
	MachineSettings.Reset();
	CalculateSelected();
}

TSharedPtr<SVerticalBox> USBCCalculatorWidget::EnsureResultColumn(int32 Depth)
{
	if (!ResultColumnsBox.IsValid()) return nullptr;
	while (ResultColumns.Num() <= Depth)
	{
		const int32 ColumnDepth = ResultColumns.Num();
		TSharedPtr<SVerticalBox> Column;
		const FString ColumnTitle = ColumnDepth == 0
			? (bKorean ? TEXT("완제품") : TEXT("Final product"))
			: (bKorean
				? FString::Printf(TEXT(">  재료 %d단계"), ColumnDepth)
				: FString::Printf(TEXT(">  Ingredient level %d"), ColumnDepth));
		ResultColumnsBox->AddSlot().AutoWidth().Padding(ColumnDepth == 0 ? 0.0f : 8.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(SBox)
			.WidthOverride(bCompactView ? 205.0f : 285.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(2.0f, 0.0f, 2.0f, 6.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(ColumnTitle))
					.ColorAndOpacity(ColumnDepth == 0 ? FLinearColor(1.0f, 0.66f, 0.18f) : FLinearColor(0.42f, 0.78f, 0.78f))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SAssignNew(Column, SVerticalBox)
				]
			]
		];
		ResultColumns.Add(Column);
	}
	return ResultColumns[Depth];
}

void USBCCalculatorWidget::ToggleNodeCollapsed(const FString& NodeId)
{
	if (CollapsedNodeIds.Contains(NodeId))
	{
		CollapsedNodeIds.Remove(NodeId);
	}
	else
	{
		CollapsedNodeIds.Add(NodeId);
	}
	CalculateSelected();
}

FLinearColor USBCCalculatorWidget::GetBuildingColor(const TSharedPtr<FSBCProductionNode>& Node) const
{
	if (!Node || Node->bRawResource) return FLinearColor(0.25f, 0.72f, 0.43f, 1.0f);
	if (Node->bGenerator) return FLinearColor(0.94f, 0.56f, 0.14f, 1.0f);
	const FString& Id = Node->BuildingId;
	if (Id.Contains(TEXT("Smelter"))) return FLinearColor(0.95f, 0.47f, 0.20f, 1.0f);
	if (Id.Contains(TEXT("Foundry"))) return FLinearColor(0.90f, 0.25f, 0.18f, 1.0f);
	if (Id.Contains(TEXT("Constructor"))) return FLinearColor(0.15f, 0.74f, 0.70f, 1.0f);
	if (Id.Contains(TEXT("Assembler"))) return FLinearColor(0.25f, 0.55f, 0.93f, 1.0f);
	if (Id.Contains(TEXT("Manufacturer"))) return FLinearColor(0.55f, 0.38f, 0.88f, 1.0f);
	if (Id.Contains(TEXT("Refinery"))) return FLinearColor(0.88f, 0.64f, 0.16f, 1.0f);
	if (Id.Contains(TEXT("Packager"))) return FLinearColor(0.16f, 0.66f, 0.88f, 1.0f);
	if (Id.Contains(TEXT("Blender"))) return FLinearColor(0.82f, 0.32f, 0.66f, 1.0f);
	if (Id.Contains(TEXT("Particle"))) return FLinearColor(0.74f, 0.45f, 0.92f, 1.0f);
	return FLinearColor(0.38f, 0.66f, 0.78f, 1.0f);
}

void USBCCalculatorWidget::AddResultNode(const TSharedPtr<FSBCProductionNode>& Node, int32 Depth, const FString& ParentName)
{
	if (!Node || !ResultColumnsBox.IsValid()) return;
	TSharedPtr<SVerticalBox> Column = EnsureResultColumn(Depth);
	if (!Column.IsValid()) return;
	const FString RateUnit = Node->Unit == TEXT("m3") ? TEXT("m³/min") : (Node->bGenerator ? TEXT("MW") : TEXT("개/min"));
	FString Detail;
	if (Node->bRawResource)
	{
		Detail = bKorean ? TEXT("원자재 · 외부 입력") : TEXT("Raw resource · external input");
	}
	else if (Node->bGenerator)
	{
		Detail = bKorean
			? FString::Printf(TEXT("%s · 설치 %d기"), *Node->BuildingName, Node->InstalledBuildingCount)
			: FString::Printf(TEXT("%s · %d installed"), *Node->BuildingName, Node->InstalledBuildingCount);
	}
	else
	{
		Detail = bKorean
			? FString::Printf(TEXT("%s · 계산 %.2f대 · 설치 %d대 · %.1f MW"),
				*Node->BuildingName, Node->ExactBuildingCount, Node->InstalledBuildingCount, Node->PowerMW)
			: FString::Printf(TEXT("%s · %.2f calculated · %d installed · %.1f MW"),
				*Node->BuildingName, Node->ExactBuildingCount, Node->InstalledBuildingCount, Node->PowerMW);
	}

	const FString CompactDetail = Node->bRawResource
		? (bKorean ? TEXT("외부 입력") : TEXT("External input"))
		: (bKorean
			? FString::Printf(TEXT("%s · 설치 %d대"), *Node->BuildingName, Node->InstalledBuildingCount)
			: FString::Printf(TEXT("%s · %d installed"), *Node->BuildingName, Node->InstalledBuildingCount));
	const FMargin CardPadding = bCompactView ? FMargin(8.0f, 6.0f) : FMargin(10.0f, 8.0f);
	FSatisfactoryBuildCalculatorModule* Module = FModuleManager::GetModulePtr<FSatisfactoryBuildCalculatorModule>(TEXT("SatisfactoryBuildCalculator"));
	const TSharedPtr<FSBCProductionData> Data = Module ? Module->GetProductionData() : nullptr;
	const TArray<FString>* CandidateRecipes = Data ? Data->FindRecipesForItem(Node->ItemId) : nullptr;
	const FSBCBuildingDefinition* Building = Data ? Data->FindBuilding(Node->BuildingId) : nullptr;
	const int32 MaxSomersloops = Building ? Building->SloopSlots : 0;
	const FString NodeId = Node->NodeId;
	const FString ItemId = Node->ItemId;
	const bool bHasChildren = !Node->Children.IsEmpty();
	const bool bCollapsed = CollapsedNodeIds.Contains(NodeId);
	const FLinearColor Accent = GetBuildingColor(Node);
	Column->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 7.0f)
	[
		SNew(SBorder)
		.Padding(1.0f)
		.BorderBackgroundColor(Depth == 0 ? FLinearColor(1.0f, 0.66f, 0.18f, 0.92f) : FLinearColor(0.24f, 0.34f, 0.38f, 0.88f))
		[
			SNew(SBorder)
			.Padding(0.0f)
			.BorderBackgroundColor(FLinearColor(0.035f, 0.075f, 0.09f, 0.90f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SBox).WidthOverride(4.0f)
					[
						SNew(SBorder).BorderBackgroundColor(Accent)
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f)
				[
					SNew(SBorder)
					.Padding(CardPadding)
					.BorderBackgroundColor(FLinearColor::Transparent)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
						[
							SNew(STextBlock)
							.Visibility(ParentName.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible)
							.Text(FText::FromString(ParentName.IsEmpty() ? FString() : FString::Printf(TEXT("<  %s"), *ParentName)))
							.ColorAndOpacity(FLinearColor(0.38f, 0.66f, 0.70f))
							.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
							.AutoWrapText(true)
						]
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Text(FText::FromString(Node->ItemName))
								.ColorAndOpacity(Depth == 0 ? FLinearColor(1.0f, 0.70f, 0.27f) : FLinearColor::White)
								.Font(FCoreStyle::GetDefaultFontStyle("Bold", bCompactView ? 11 : (Depth == 0 ? 14 : 12)))
								.AutoWrapText(true)
							]
							+ SHorizontalBox::Slot().AutoWidth().Padding(5.0f, 0.0f, 0.0f, 0.0f)
							[
								SNew(SButton)
								.Visibility(bHasChildren ? EVisibility::Visible : EVisibility::Collapsed)
								.ContentPadding(FMargin(5.0f, 1.0f))
								.Text(FText::FromString(bCollapsed ? TEXT("[+]") : TEXT("[-]")))
								.OnClicked_Lambda([this, NodeId]() { ToggleNodeCollapsed(NodeId); return FReply::Handled(); })
							]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f, 0.0f, 0.0f)
						[
							SNew(STextBlock)
							.Text(FText::FromString(FString::Printf(TEXT("%.2f %s"), Node->RequiredRate, *RateUnit)))
							.ColorAndOpacity(FLinearColor(0.95f, 0.97f, 0.98f))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", bCompactView ? 10 : 12))
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
						[
							SNew(STextBlock)
							.Text(FText::FromString(bCompactView ? CompactDetail : Detail))
							.ColorAndOpacity(FLinearColor(0.58f, 0.75f, 0.78f))
							.Font(FCoreStyle::GetDefaultFontStyle("Regular", bCompactView ? 8 : 9))
							.AutoWrapText(true)
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
						[
							SNew(SComboButton)
							.Visibility(!bCompactView && !Node->bRawResource ? EVisibility::Visible : EVisibility::Collapsed)
							.IsEnabled(CandidateRecipes && CandidateRecipes->Num() > 1)
							.OnGetMenuContent_Lambda([this, NodeId, ItemId]() { return BuildRecipeMenu(NodeId, ItemId); })
							.ButtonContent()
							[
								SNew(STextBlock)
								.Text(FText::FromString(Node->RecipeName))
								.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
							]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 5.0f, 0.0f, 0.0f)
						[
							SNew(SHorizontalBox)
							.Visibility(!bCompactView && !Node->bRawResource && !Node->bGenerator ? EVisibility::Visible : EVisibility::Collapsed)
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 4.0f, 0.0f)
							[
								SNew(STextBlock).Text(Text(TEXT("동력핵"), TEXT("Shards"))).Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
							]
							+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
							[
								SNew(SSpinBox<int32>)
								.MinValue(0).MaxValue(3).MinSliderValue(0).MaxSliderValue(3)
								.Value(Node->PowerShards)
								.OnValueCommitted_Lambda([this, NodeId](int32 Value, ETextCommit::Type) { SetNodePowerShards(NodeId, Value); })
							]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 4.0f, 0.0f)
							[
								SNew(STextBlock).Text(Text(TEXT("소매슬루프"), TEXT("Somersloops"))).Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
								.Visibility(MaxSomersloops > 0 ? EVisibility::Visible : EVisibility::Collapsed)
							]
							+ SHorizontalBox::Slot().AutoWidth()
							[
								SNew(SSpinBox<int32>)
								.MinValue(0).MaxValue(MaxSomersloops).MinSliderValue(0).MaxSliderValue(MaxSomersloops)
								.Value(Node->Somersloops)
								.Visibility(MaxSomersloops > 0 ? EVisibility::Visible : EVisibility::Collapsed)
								.OnValueCommitted_Lambda([this, NodeId](int32 Value, ETextCommit::Type) { SetNodeSomersloops(NodeId, Value); })
							]
						]
					]
				]
			]
		]
	];
	if (bCollapsed) return;
	for (const TSharedPtr<FSBCProductionNode>& Child : Node->Children)
	{
		AddResultNode(Child, Depth + 1, Node->ItemName);
	}
}
