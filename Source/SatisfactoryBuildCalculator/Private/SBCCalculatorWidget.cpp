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
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Rendering/DrawElements.h"

class SSBCPanGraph final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSBCPanGraph) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		SetClipping(EWidgetClipping::ClipToBoundsAlways);
		ChildSlot
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Top)
		[
			SAssignNew(ContentBox, SBox)
			.WidthOverride(GraphSize.X)
			.HeightOverride(GraphSize.Y)
			[
				SAssignNew(Canvas, SConstraintCanvas)
			]
		];
		ApplyPan();
	}

	void ResetGraph(const FVector2D& NewSize)
	{
		GraphSize = FVector2D(FMath::Max(NewSize.X, 200.0f), FMath::Max(NewSize.Y, 200.0f));
		Connections.Reset();
		CardCenters.Reset();
		if (Canvas.IsValid()) Canvas->ClearChildren();
		if (ContentBox.IsValid())
		{
			ContentBox->SetWidthOverride(GraphSize.X);
			ContentBox->SetHeightOverride(GraphSize.Y);
		}
		ClampPan();
		Invalidate(EInvalidateWidgetReason::Layout);
	}

	void AddCard(const FString& NodeId, const FVector2D& Position, const FVector2D& Size, TSharedRef<SWidget> Card)
	{
		if (!Canvas.IsValid()) return;
		Canvas->AddSlot()
		.Offset(FMargin(Position.X, Position.Y, Size.X, Size.Y))
		.Anchors(FAnchors(0.0f))
		.Alignment(FVector2D::ZeroVector)
		[
			Card
		];
		CardCenters.Add(NodeId, Position + Size * 0.5f);
	}

	void AddConnection(const FVector2D& Start, const FVector2D& End)
	{
		Connections.Add(TPair<FVector2D, FVector2D>(Start, End));
	}

	void ResetView()
	{
		PanOffset = FVector2D(24.0f, 24.0f);
		ClampPan();
		ApplyPan();
	}

	void FocusCard(const FString& NodeId)
	{
		if (const FVector2D* Center = CardCenters.Find(NodeId))
		{
			PanOffset = LastViewportSize * 0.5f - *Center;
			ClampPan();
			ApplyPan();
		}
	}

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
		{
			bDragging = true;
			return FReply::Handled().CaptureMouse(SharedThis(this));
		}
		return FReply::Unhandled();
	}

	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		if (bDragging && MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
		{
			bDragging = false;
			return FReply::Handled().ReleaseMouseCapture();
		}
		return FReply::Unhandled();
	}

	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		if (!bDragging || !HasMouseCapture()) return FReply::Unhandled();
		PanOffset += MouseEvent.GetCursorDelta();
		LastViewportSize = MyGeometry.GetLocalSize();
		ClampPan();
		ApplyPan();
		return FReply::Handled();
	}

	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		LastViewportSize = MyGeometry.GetLocalSize();
		const float Amount = MouseEvent.GetWheelDelta() * 54.0f;
		if (MouseEvent.IsShiftDown()) PanOffset.X += Amount;
		else PanOffset.Y += Amount;
		ClampPan();
		ApplyPan();
		return FReply::Handled();
	}

	virtual int32 OnPaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override
	{
		const_cast<SSBCPanGraph*>(this)->LastViewportSize = AllottedGeometry.GetLocalSize();
		for (const TPair<FVector2D, FVector2D>& Connection : Connections)
		{
			const FVector2D Start = Connection.Key + PanOffset;
			const FVector2D End = Connection.Value + PanOffset;
			const float MiddleX = (Start.X + End.X) * 0.5f;
			TArray<FVector2D> Points{Start, FVector2D(MiddleX, Start.Y), FVector2D(MiddleX, End.Y), End};
			FSlateDrawElement::MakeLines(
				OutDrawElements,
				LayerId,
				AllottedGeometry.ToPaintGeometry(),
				Points,
				ESlateDrawEffect::None,
				FLinearColor(0.34f, 0.62f, 0.66f, 0.78f),
				true,
				2.0f);
		}
		return SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId + 1, InWidgetStyle, bParentEnabled);
	}

private:
	TSharedPtr<SConstraintCanvas> Canvas;
	TSharedPtr<SBox> ContentBox;
	TArray<TPair<FVector2D, FVector2D>> Connections;
	TMap<FString, FVector2D> CardCenters;
	FVector2D GraphSize = FVector2D(1000.0f, 650.0f);
	FVector2D LastViewportSize = FVector2D(1000.0f, 650.0f);
	FVector2D PanOffset = FVector2D(24.0f, 24.0f);
	bool bDragging = false;

	void ApplyPan()
	{
		if (ContentBox.IsValid()) ContentBox->SetRenderTransform(FSlateRenderTransform(PanOffset));
		Invalidate(EInvalidateWidgetReason::Paint);
	}

	void ClampPan()
	{
		const float Margin = 80.0f;
		const float MinX = GraphSize.X <= LastViewportSize.X
			? Margin : LastViewportSize.X - GraphSize.X - Margin;
		const float MinY = GraphSize.Y <= LastViewportSize.Y
			? Margin : LastViewportSize.Y - GraphSize.Y - Margin;
		PanOffset.X = FMath::Clamp(PanOffset.X, MinX, Margin);
		PanOffset.Y = FMath::Clamp(PanOffset.Y, MinY, Margin);
	}
};

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

	TSharedPtr<FSBCProductionNode> FindProductionNode(const TSharedPtr<FSBCProductionNode>& Node, const FString& NodeId)
	{
		if (!Node) return nullptr;
		if (Node->NodeId == NodeId) return Node;
		for (const TSharedPtr<FSBCProductionNode>& Child : Node->Children)
		{
			if (TSharedPtr<FSBCProductionNode> Match = FindProductionNode(Child, NodeId)) return Match;
		}
		return nullptr;
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
							.Text(Text(TEXT("공정도 초기 위치"), TEXT("Reset view")))
							.OnClicked_Lambda([this]()
							{
								if (ResultGraph.IsValid()) ResultGraph->ResetView();
								return FReply::Handled();
							})
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
						[
							SNew(SButton)
							.Text(Text(TEXT("선택 카드 보기"), TEXT("Focus selected")))
							.OnClicked_Lambda([this]()
							{
								if (ResultGraph.IsValid()) ResultGraph->FocusCard(SelectedGoalNodeId);
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
									.Text(Text(TEXT("선택 공정 목표 추가"), TEXT("Add selected branch")))
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
								SAssignNew(ResultGraph, SSBCPanGraph)
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
	SelectedGoalNodeId = TEXT("root");
	if (ResultGraph.IsValid()) ResultGraph->ResetView();
	RefreshProductList();
	CalculateSelected();
}

void USBCCalculatorWidget::CalculateSelected()
{
	if (!ResultGraph.IsValid()) return;
	LastCalculatedRoot.Reset();
	ResultGraph->ResetGraph(FVector2D(600.0f, 240.0f));
	if (SummaryBox.IsValid()) SummaryBox->ClearChildren();
	auto ShowMessage = [this](const FText& Message, const FLinearColor& Color)
	{
		ResultGraph->AddCard(TEXT("message"), FVector2D(20.0f, 20.0f), FVector2D(500.0f, 70.0f),
			SNew(SBorder)
			.Padding(12.0f)
			.BorderBackgroundColor(FLinearColor(0.04f, 0.08f, 0.10f, 0.88f))
			[
				SNew(STextBlock).Text(Message).ColorAndOpacity(Color).AutoWrapText(true)
			]);
	};
	FSatisfactoryBuildCalculatorModule* Module = FModuleManager::GetModulePtr<FSatisfactoryBuildCalculatorModule>(TEXT("SatisfactoryBuildCalculator"));
	const TSharedPtr<FSBCProductionData> Data = Module ? Module->GetProductionData() : nullptr;
	if (!Data || !Data->IsLoaded())
	{
		ShowMessage(Text(TEXT("계산 데이터를 불러오지 못했습니다."), TEXT("Calculator data could not be loaded.")), FLinearColor::White);
		return;
	}
	if (SelectedItemId.IsEmpty())
	{
		ShowMessage(Text(TEXT("왼쪽에서 제품을 선택하세요."), TEXT("Select a product on the left.")), FLinearColor::White);
		return;
	}
	if (!IsProductUnlocked(SelectedItemId))
	{
		ShowMessage(Text(TEXT("아직 해금되지 않은 제품입니다."), TEXT("This product has not been unlocked yet.")), FLinearColor(1.0f, 0.62f, 0.14f));
		return;
	}

	FSBCProductionCalculator Calculator(*Data);
	FString Error;
	const TSharedPtr<FSBCProductionNode> Root = Calculator.Calculate(SelectedItemId, TargetRate, SelectedRecipes, MachineSettings, DefaultPowerShards, bKorean, 0.0, Error);
	if (!Root)
	{
		ShowMessage(FText::FromString(Error), FLinearColor(1.0f, 0.35f, 0.25f));
		return;
	}
	LastCalculatedRoot = Root;
	if (SelectedGoalNodeId.IsEmpty() || !FindProductionNode(Root, SelectedGoalNodeId)) SelectedGoalNodeId = Root->NodeId;
	BuildResultGraph(Root);
	RefreshSummary(Root);
}

TArray<FSBCCalculatedGoalRequest> USBCCalculatorWidget::GetGoalPlan() const
{
	TMap<FString, FSBCCalculatedGoalRequest> Requests;
	TSharedPtr<FSBCProductionNode> SelectedRoot = FindProductionNode(LastCalculatedRoot, SelectedGoalNodeId);
	GatherGoalPlan(SelectedRoot ? SelectedRoot : LastCalculatedRoot, Requests);
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

void USBCCalculatorWidget::SelectGoalNode(const FString& NodeId)
{
	SelectedGoalNodeId = NodeId;
	BuildResultGraph(LastCalculatedRoot);
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

void USBCCalculatorWidget::BuildResultGraph(const TSharedPtr<FSBCProductionNode>& Root)
{
	if (!Root || !ResultGraph.IsValid()) return;
	struct FGraphEntry
	{
		TSharedPtr<FSBCProductionNode> Node;
		FString ParentName;
		FString ParentNodeId;
		FVector2D Position;
	};
	TArray<FGraphEntry> Entries;
	TMap<FString, FVector2D> Positions;
	const float CardWidth = bCompactView ? 205.0f : 285.0f;
	const float CardHeight = bCompactView ? 105.0f : 190.0f;
	const float HorizontalGap = 74.0f;
	const float VerticalGap = 26.0f;
	float NextLeafCenterY = 40.0f + CardHeight * 0.5f;
	int32 MaxDepth = 0;

	TFunction<float(const TSharedPtr<FSBCProductionNode>&, int32, const FString&, const FString&)> LayoutNode;
	LayoutNode = [this, &LayoutNode, &Entries, &Positions, &NextLeafCenterY, &MaxDepth,
		CardWidth, CardHeight, HorizontalGap, VerticalGap](
			const TSharedPtr<FSBCProductionNode>& Node,
			int32 Depth,
			const FString& ParentName,
			const FString& ParentNodeId) -> float
	{
		MaxDepth = FMath::Max(MaxDepth, Depth);
		TArray<float> ChildCenters;
		if (!CollapsedNodeIds.Contains(Node->NodeId))
		{
			for (const TSharedPtr<FSBCProductionNode>& Child : Node->Children)
			{
				ChildCenters.Add(LayoutNode(Child, Depth + 1, Node->ItemName, Node->NodeId));
			}
		}
		float CenterY = NextLeafCenterY;
		if (ChildCenters.IsEmpty())
		{
			NextLeafCenterY += CardHeight + VerticalGap;
		}
		else
		{
			CenterY = (ChildCenters[0] + ChildCenters.Last()) * 0.5f;
		}
		const FVector2D Position(40.0f + Depth * (CardWidth + HorizontalGap), CenterY - CardHeight * 0.5f);
		Positions.Add(Node->NodeId, Position);
		Entries.Add({Node, ParentName, ParentNodeId, Position});
		return CenterY;
	};

	LayoutNode(Root, 0, FString(), FString());
	const FVector2D GraphSize(
		80.0f + (MaxDepth + 1) * CardWidth + MaxDepth * HorizontalGap,
		FMath::Max(260.0f, NextLeafCenterY + CardHeight * 0.5f + 40.0f));
	ResultGraph->ResetGraph(GraphSize);
	for (const FGraphEntry& Entry : Entries)
	{
		ResultGraph->AddCard(Entry.Node->NodeId, Entry.Position, FVector2D(CardWidth, CardHeight), BuildResultCard(Entry.Node, Entry.ParentName));
		if (!Entry.ParentNodeId.IsEmpty())
		{
			if (const FVector2D* ParentPosition = Positions.Find(Entry.ParentNodeId))
			{
				ResultGraph->AddConnection(
					*ParentPosition + FVector2D(CardWidth, CardHeight * 0.5f),
					Entry.Position + FVector2D(0.0f, CardHeight * 0.5f));
			}
		}
	}
}

TSharedRef<SWidget> USBCCalculatorWidget::BuildResultCard(const TSharedPtr<FSBCProductionNode>& Node, const FString& ParentName)
{
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
	const bool bSelected = SelectedGoalNodeId == NodeId;
	const bool bRoot = NodeId == TEXT("root");
	const FLinearColor Accent = GetBuildingColor(Node);
	return SNew(SBorder)
	.Padding(bSelected ? 2.0f : 1.0f)
	.BorderBackgroundColor(bSelected
		? FLinearColor(1.0f, 0.62f, 0.12f, 0.98f)
		: (bRoot ? FLinearColor(0.88f, 0.52f, 0.14f, 0.82f) : FLinearColor(0.24f, 0.34f, 0.38f, 0.88f)))
	[
		SNew(SBorder)
		.Padding(0.0f)
		.BorderBackgroundColor(FLinearColor(0.035f, 0.075f, 0.09f, 0.92f))
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
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 3.0f)
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
							.ColorAndOpacity(bRoot ? FLinearColor(1.0f, 0.70f, 0.27f) : FLinearColor::White)
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", bCompactView ? 11 : (bRoot ? 14 : 12)))
							.AutoWrapText(true)
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f, 0.0f, 0.0f)
						[
							SNew(SButton)
							.ContentPadding(FMargin(5.0f, 1.0f))
							.Text(bSelected ? Text(TEXT("선택됨"), TEXT("Selected")) : Text(TEXT("선택"), TEXT("Select")))
							.OnClicked_Lambda([this, NodeId]() { SelectGoalNode(NodeId); return FReply::Handled(); })
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f, 0.0f, 0.0f)
						[
							SNew(SButton)
							.Visibility(bHasChildren ? EVisibility::Visible : EVisibility::Collapsed)
							.ContentPadding(FMargin(5.0f, 1.0f))
							.Text(bCollapsed ? Text(TEXT("펴기"), TEXT("Expand")) : Text(TEXT("접기"), TEXT("Collapse")))
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
	];
}
