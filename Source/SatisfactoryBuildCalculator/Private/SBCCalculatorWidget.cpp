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
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

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
							]
							+ SVerticalBox::Slot().FillHeight(1.0f)
							[
								SNew(SScrollBox)
								+ SScrollBox::Slot()
								[
									SAssignNew(ResultBox, SVerticalBox)
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
	return FText::FromString(bKorean
		? FString::Printf(TEXT("해금 제조법 %d개 동기화"), UnlockedRecipeIds.Num())
		: FString::Printf(TEXT("%d unlocked recipes synced"), UnlockedRecipeIds.Num()));
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
			: FString::Printf(TEXT("%s  🔒"), *DisplayName));
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
	RefreshProductList();
	CalculateSelected();
}

void USBCCalculatorWidget::CalculateSelected()
{
	if (!ResultBox.IsValid()) return;
	ResultBox->ClearChildren();
	FSatisfactoryBuildCalculatorModule* Module = FModuleManager::GetModulePtr<FSatisfactoryBuildCalculatorModule>(TEXT("SatisfactoryBuildCalculator"));
	const TSharedPtr<FSBCProductionData> Data = Module ? Module->GetProductionData() : nullptr;
	if (!Data || !Data->IsLoaded())
	{
		ResultBox->AddSlot().AutoHeight()[SNew(STextBlock).Text(Text(TEXT("계산 데이터를 불러오지 못했습니다."), TEXT("Calculator data could not be loaded.")))];
		return;
	}
	if (SelectedItemId.IsEmpty())
	{
		ResultBox->AddSlot().AutoHeight()[SNew(STextBlock).Text(Text(TEXT("왼쪽에서 제품을 선택하세요."), TEXT("Select a product on the left.")))];
		return;
	}
	if (!IsProductUnlocked(SelectedItemId))
	{
		ResultBox->AddSlot().AutoHeight()
		[
			SNew(STextBlock)
			.Text(Text(TEXT("아직 해금되지 않은 제품입니다."), TEXT("This product has not been unlocked yet.")))
			.ColorAndOpacity(FLinearColor(1.0f, 0.62f, 0.14f))
		];
		return;
	}

	FSBCProductionCalculator Calculator(*Data);
	FString Error;
	const TSharedPtr<FSBCProductionNode> Root = Calculator.Calculate(SelectedItemId, TargetRate, {}, {}, 0, bKorean, 0.0, Error);
	if (!Root)
	{
		ResultBox->AddSlot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(Error)).ColorAndOpacity(FLinearColor(1.0f, 0.35f, 0.25f))];
		return;
	}
	AddResultNode(Root, 0);
}

void USBCCalculatorWidget::AddResultNode(const TSharedPtr<FSBCProductionNode>& Node, int32 Depth)
{
	if (!Node || !ResultBox.IsValid()) return;
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

	const float Indent = bCompactView ? Depth * 10.0f : Depth * 18.0f;
	const FMargin CardPadding = bCompactView ? FMargin(7.0f, 3.0f) : FMargin(9.0f, 6.0f);
	ResultBox->AddSlot().AutoHeight().Padding(Indent, 2.0f, 0.0f, 2.0f)
	[
		SNew(SBorder)
		.Padding(CardPadding)
		.BorderBackgroundColor(Depth == 0 ? FLinearColor(0.12f, 0.18f, 0.20f, 0.78f) : FLinearColor(0.055f, 0.09f, 0.11f, 0.62f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::FromString(FString::Printf(TEXT("%s  %.2f %s"), *Node->ItemName, Node->RequiredRate, *RateUnit)))
				.ColorAndOpacity(Depth == 0 ? FLinearColor(1.0f, 0.68f, 0.22f) : FLinearColor::White)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", Depth == 0 ? 14 : 12))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Detail))
				.ColorAndOpacity(FLinearColor(0.58f, 0.72f, 0.78f))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
				.Visibility(bCompactView ? EVisibility::Collapsed : EVisibility::Visible)
			]
		]
	];
	for (const TSharedPtr<FSBCProductionNode>& Child : Node->Children)
	{
		AddResultNode(Child, Depth + 1);
	}
}
