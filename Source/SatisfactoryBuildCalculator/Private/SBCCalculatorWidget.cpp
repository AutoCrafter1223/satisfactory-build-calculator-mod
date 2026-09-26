#include "SBCCalculatorWidget.h"

#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "SatisfactoryBuildCalculator.h"
#include "SBCProductionCalculator.h"
#include "SBCHotkeyConfig.h"
#include "FGRecipe.h"
#include "FGRecipeManager.h"
#include "Resources/FGItemDescriptor.h"
#include "Buildables/FGBuildable.h"
#include "Engine/Texture2D.h"
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
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Images/SImage.h"
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
		ApplyPan();
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
		Zoom = 1.0f;
		PanOffset = FVector2D(24.0f, 24.0f);
		ClampPan();
		ApplyPan();
	}

	void FocusCard(const FString& NodeId)
	{
		if (const FVector2D* Center = CardCenters.Find(NodeId))
		{
			PanOffset = LastViewportSize * 0.5f - *Center * Zoom;
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
		const FVector2D Cursor = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
		const FVector2D ContentPoint = (Cursor - PanOffset) / Zoom;
		Zoom = FMath::Clamp(Zoom + MouseEvent.GetWheelDelta() * 0.08f, 0.65f, 1.20f);
		PanOffset = Cursor - ContentPoint * Zoom;
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
			const FVector2D Start = Connection.Key * Zoom + PanOffset;
			const FVector2D End = Connection.Value * Zoom + PanOffset;
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
	float Zoom = 1.0f;
	bool bDragging = false;

	void ApplyPan()
	{
		if (ContentBox.IsValid())
		{
			ContentBox->SetRenderTransform(FSlateRenderTransform(FScale2D(Zoom), PanOffset));
		}
		Invalidate(EInvalidateWidgetReason::Paint);
	}

	void ClampPan()
	{
		const float Margin = 80.0f;
		const FVector2D ScaledSize = GraphSize * Zoom;
		// Allow small and large graphs to pan on both axes. Keep only a small
		// portion visible so a graph cannot be lost completely off-screen.
		PanOffset.X = FMath::Clamp(PanOffset.X, Margin - ScaledSize.X, LastViewportSize.X - Margin);
		PanOffset.Y = FMath::Clamp(PanOffset.Y, Margin - ScaledSize.Y, LastViewportSize.Y - Margin);
	}
};

namespace
{
	struct FSBCSummaryEntry
	{
		FString ItemId;
		FString Name;
		double Rate = 0.0;
		FString Unit;
	};

	FString RateUnit(ESBCLanguage Language, const FString& Unit)
	{
		if (Unit == TEXT("m3"))
		{
			return Language == ESBCLanguage::Korean ? TEXT("m³/분") : TEXT("m³/min");
		}
		return SBCLocalization::String(Language, TEXT("개/분"), TEXT("item/min"), TEXT("个/分钟"), TEXT("Stk./min"));
	}

	FString FormatRate(ESBCLanguage Language, double Rate, const FString& Unit)
	{
		const TCHAR* Separator = Language == ESBCLanguage::English ? TEXT(" ") : TEXT("");
		return FString::Printf(TEXT("%.2f%s%s"), Rate, Separator, *RateUnit(Language, Unit));
	}

	bool IsGeneratorRecipe(const FSBCRecipeDefinition& Recipe)
	{
		return Recipe.Kind == TEXT("generator") || Recipe.Kind == TEXT("geothermal") || Recipe.Kind == TEXT("augmenter");
	}

	bool IsUnpackageRecipe(const FSBCRecipeDefinition& Recipe)
	{
		return Recipe.Id.StartsWith(TEXT("Recipe_Unpackage"), ESearchCase::IgnoreCase);
	}

	FString GeneratorRecipeLabel(const FSBCProductionData& Data, const FSBCRecipeDefinition& Recipe)
	{
		if (!Recipe.Ingredients.IsEmpty())
		{
			if (const FSBCItemDefinition* Fuel = Data.FindItem(Recipe.Ingredients[0].ItemId))
			{
				return Fuel->LocalizedName.IsEmpty() ? Fuel->NameEn : Fuel->LocalizedName;
			}
		}
		const FString Name = Recipe.LocalizedName.IsEmpty() ? Recipe.NameEn : Recipe.LocalizedName;
		FString Prefix;
		FString Suffix;
		return Name.Split(TEXT(" · "), &Prefix, &Suffix, ESearchCase::CaseSensitive, ESearchDir::FromEnd) ? Suffix : Name;
	}

	void GatherProductionSummary(
		const TSharedPtr<FSBCProductionNode>& Node,
		TMap<FString, FSBCSummaryEntry>& RawResources,
		TMap<FString, FSBCSummaryEntry>& RequiredProducts,
		int32& Machines,
		double& ConsumptionMW,
		double& GenerationMW,
		bool bRoot = true)
	{
		if (!Node) return;
		if (Node->bRawResource)
		{
			FSBCSummaryEntry& Entry = RawResources.FindOrAdd(Node->ItemId);
			Entry.ItemId = Node->ItemId;
			Entry.Name = Node->ItemName;
			Entry.Rate += Node->RequiredRate;
			Entry.Unit = Node->Unit;
		}
		else
		{
			if (!bRoot)
			{
				FSBCSummaryEntry& Entry = RequiredProducts.FindOrAdd(Node->ItemId);
				Entry.ItemId = Node->ItemId;
				Entry.Name = Node->ItemName;
				Entry.Rate += Node->RequiredRate;
				Entry.Unit = Node->Unit;
			}
			Machines += Node->InstalledBuildingCount;
			ConsumptionMW += Node->PowerMW;
			GenerationMW += Node->GenerationMW;
		}
		for (const TSharedPtr<FSBCProductionNode>& Child : Node->Children)
		{
			GatherProductionSummary(Child, RawResources, RequiredProducts, Machines, ConsumptionMW, GenerationMW, false);
		}
	}

	void GatherGoalPlan(
		const TSharedPtr<FSBCProductionNode>& Node,
		TMap<FString, FSBCCalculatedGoalRequest>& Requests,
		const FString& ParentHierarchyKey,
		int32 HierarchyDepth,
		int32& NextOrder)
	{
		if (!Node) return;
		FString ChildParentKey = ParentHierarchyKey;
		int32 ChildDepth = HierarchyDepth;
		if (!Node->bRawResource && !Node->BuildingId.IsEmpty() && Node->InstalledBuildingCount > 0)
		{
			const FString Identity = Node->bGenerator ? Node->BuildingId : Node->RecipeId;
			const FString Key = FString::Printf(TEXT("%s|%d|%d"), *Identity, Node->PowerShards, Node->Somersloops);
			const bool bNewRequest = !Requests.Contains(Key);
			FSBCCalculatedGoalRequest& Request = Requests.FindOrAdd(Key);
			Request.RecipeId = Node->RecipeId;
			Request.BuildingId = Node->BuildingId;
			Request.TargetCount += Node->InstalledBuildingCount;
			Request.PowerShards = Node->PowerShards;
			Request.Somersloops = Node->Somersloops;
			Request.bRequiresRecipe = !Node->bGenerator;
			if (bNewRequest)
			{
				Request.HierarchyKey = Key;
				Request.ParentHierarchyKey = ParentHierarchyKey;
				Request.HierarchyDepth = HierarchyDepth;
				Request.HierarchyOrder = NextOrder++;
			}
			ChildParentKey = Key;
			ChildDepth = HierarchyDepth + 1;
		}
		for (const TSharedPtr<FSBCProductionNode>& Child : Node->Children)
		{
			GatherGoalPlan(Child, Requests, ChildParentKey, ChildDepth, NextOrder);
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

FText USBCCalculatorWidget::Text(const TCHAR* Korean, const TCHAR* English, const TCHAR* Chinese, const TCHAR* German) const
{
	return SBCLocalization::Text(Language, Korean, English, Chinese, German);
}

const FSlateBrush* USBCCalculatorWidget::GetItemIconBrush(const FString& ItemId, float Size)
{
	if (const TSharedPtr<FSlateBrush>* Existing = ItemIconBrushes.Find(ItemId))
	{
		return Existing->Get();
	}

	FSatisfactoryBuildCalculatorModule* Module = FModuleManager::GetModulePtr<FSatisfactoryBuildCalculatorModule>(TEXT("SatisfactoryBuildCalculator"));
	const TSharedPtr<FSBCProductionData> Data = Module ? Module->GetProductionData() : nullptr;
	const FSBCItemDefinition* Item = Data ? Data->FindItem(ItemId) : nullptr;
	UTexture2D* Texture = Item && Item->DescriptorClass ? UFGItemDescriptor::GetSmallIcon(Item->DescriptorClass) : nullptr;
	if (!IsValid(Texture))
	{
		return nullptr;
	}

	ItemIconTextures.Add(ItemId, Texture);
	TSharedPtr<FSlateBrush> Brush = MakeShared<FSlateBrush>();
	Brush->SetResourceObject(Texture);
	Brush->ImageSize = FVector2D(Size, Size);
	Brush->DrawAs = ESlateBrushDrawType::Image;
	ItemIconBrushes.Add(ItemId, Brush);
	return Brush.Get();
}

TSharedRef<SWidget> USBCCalculatorWidget::RebuildWidget()
{
	Language = SBCLocalization::GetCurrentLanguage();

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
						.Text(FText::FromString(TEXT("Satisfactory Build Calculator")))
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
								return bAllowLockedRecipes
									? Text(TEXT("전체 제조법"), TEXT("All recipes"), TEXT("全部配方"), TEXT("Alle Rezepte"))
									: Text(TEXT("해금만"), TEXT("Unlocked only"), TEXT("仅已解锁"), TEXT("Nur freigesch."));
							})
							.ToolTipText_Lambda([this]()
							{
								return bAllowLockedRecipes
									? Text(TEXT("잠긴 제조법도 선택할 수 있습니다."), TEXT("Locked recipes can also be selected."), TEXT("也可以选择未解锁的配方。"), TEXT("Gesperrte Rezepte können gewählt werden."))
									: Text(TEXT("현재 세이브에서 해금된 제조법만 선택합니다."), TEXT("Only recipes unlocked in this save can be selected."), TEXT("只能选择当前存档中已解锁的配方。"), TEXT("Nur in diesem Spielstand freigeschaltete Rezepte."));
							})
							.OnClicked_Lambda([this]()
							{
								bAllowLockedRecipes = !bAllowLockedRecipes;
								if (!bAllowLockedRecipes)
								{
									FSatisfactoryBuildCalculatorModule* Module = FModuleManager::GetModulePtr<FSatisfactoryBuildCalculatorModule>(TEXT("SatisfactoryBuildCalculator"));
									const TSharedPtr<FSBCProductionData> Data = Module ? Module->GetProductionData() : nullptr;
									for (auto It = SelectedRecipes.CreateIterator(); It; ++It)
									{
										const FSBCRecipeDefinition* Recipe = Data ? Data->FindRecipe(It.Value()) : nullptr;
										if (!Recipe || !IsRecipeUnlocked(*Recipe)) It.RemoveCurrent();
									}
								}
								CalculateSelected();
								return FReply::Handled();
							})
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
						[
							SNew(SButton)
							.Text_Lambda([this]()
							{
								return bHideLockedProducts
									? Text(TEXT("잠김 표시"), TEXT("Show locked"), TEXT("显示未解锁"), TEXT("Gesperrte zeigen"))
									: Text(TEXT("잠김 숨김"), TEXT("Hide locked"), TEXT("隐藏未解锁"), TEXT("Gesperrte ausblenden"));
							})
							.OnClicked_Lambda([this]()
							{
								bHideLockedProducts = !bHideLockedProducts;
								RefreshProductList();
								return FReply::Handled();
							})
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
						[
							SNew(SButton)
							.Text_Lambda([this]()
							{
								return bCompactView
									? Text(TEXT("상세 보기"), TEXT("Detailed view"), TEXT("详细视图"), TEXT("Detailansicht"))
									: Text(TEXT("간소화 보기"), TEXT("Compact view"), TEXT("简洁视图"), TEXT("Kompaktansicht"));
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
							.Text_Lambda([this]() { return Text(TEXT("공정도 초기 위치"), TEXT("Reset view"), TEXT("重置视图"), TEXT("Ansicht zurücksetzen")); })
							.OnClicked_Lambda([this]()
							{
								if (ResultGraph.IsValid()) ResultGraph->ResetView();
								return FReply::Handled();
							})
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
						[
							SNew(SButton)
							.Text_Lambda([this]() { return Text(TEXT("선택 카드 보기"), TEXT("Focus selected"), TEXT("定位所选卡片"), TEXT("Auswahl anzeigen")); })
							.OnClicked_Lambda([this]()
							{
								if (ResultGraph.IsValid()) ResultGraph->FocusCard(SelectedGoalNodeId);
								return FReply::Handled();
							})
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
						[
							SNew(SButton)
							.Text_Lambda([this]() { return Text(TEXT("새로고침"), TEXT("Refresh"), TEXT("刷新"), TEXT("Aktualisieren")); })
							.OnClicked_Lambda([this]()
							{
								RefreshGameState();
								return FReply::Handled();
							})
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text_Lambda([this]()
							{
								const FString Key = SBCHotkeys::GetDisplayName(this, ESBCHotkeyAction::ToggleCalculator).ToString();
								return FText::FromString(SBCLocalization::Format(
									SBCLocalization::String(Language, TEXT("{0} / ESC 닫기"), TEXT("{0} / ESC Close"), TEXT("{0} / ESC 关闭"), TEXT("{0} / ESC Schließen")),
									Key));
							})
							.ColorAndOpacity(FLinearColor(0.62f, 0.72f, 0.77f))
							.Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))
						]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
				[
					SNew(STextBlock)
					.Visibility_Lambda([this]() { return GoalOperationStatus.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
					.Text_Lambda([this]() { return FText::FromString(GoalOperationStatus); })
					.ColorAndOpacity_Lambda([this]()
					{
						return bGoalOperationFailed
							? FSlateColor(FLinearColor(1.0f, 0.38f, 0.22f))
							: FSlateColor(FLinearColor(0.38f, 0.92f, 0.68f));
					})
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
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
								.HintText_Lambda([this]() { return Text(TEXT("제품 또는 발전기 검색"), TEXT("Search products or generators"), TEXT("搜索产品或发电机"), TEXT("Produkt oder Generator suchen")); })
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
									.Text_Lambda([this]() { return Text(TEXT("목표 생산량"), TEXT("Target rate"), TEXT("目标产量"), TEXT("Zielrate")); })
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
									.Text_Lambda([this]() { return Text(TEXT("전체 동력핵"), TEXT("Global shards"), TEXT("全局能量碎片"), TEXT("Globale Energiesplitter")); })
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
									.Text_Lambda([this]() { return Text(TEXT("계산"), TEXT("Calculate"), TEXT("计算"), TEXT("Berechnen")); })
									.OnClicked_Lambda([this]()
									{
										CalculateSelected();
										return FReply::Handled();
									})
								]
								+ SHorizontalBox::Slot().AutoWidth().Padding(8.0f, 0.0f, 0.0f, 0.0f)
								[
									SNew(SButton)
									.Text_Lambda([this]() { return Text(TEXT("선택 공정 목표 추가"), TEXT("Add selected branch"), TEXT("添加所选分支目标"), TEXT("Ausgewählten Zweig hinzufügen")); })
									.OnClicked_Lambda([this]()
									{
									OnRequestAddGoals.ExecuteIfBound();
									return FReply::Handled();
								})
							]
							+ SHorizontalBox::Slot().AutoWidth().Padding(8.0f, 0.0f, 0.0f, 0.0f)
							[
								SNew(SButton)
								.Text_Lambda([this]()
								{
									const bool bAwaitingConfirmation = bClearGoalsConfirmationArmed &&
										FPlatformTime::Seconds() <= ClearGoalsConfirmationExpiresAt;
									return bAwaitingConfirmation
										? Text(TEXT("한 번 더 누르세요"), TEXT("Confirm clear"), TEXT("再次点击确认"), TEXT("Löschen bestätigen"))
										: Text(TEXT("목표 초기화"), TEXT("Clear goals"), TEXT("清空目标"), TEXT("Ziele löschen"));
								})
								.OnClicked_Lambda([this]()
								{
									const double Now = FPlatformTime::Seconds();
									if (!bClearGoalsConfirmationArmed || Now > ClearGoalsConfirmationExpiresAt)
									{
										bClearGoalsConfirmationArmed = true;
										ClearGoalsConfirmationExpiresAt = Now + 3.0;
										return FReply::Handled();
									}

									bClearGoalsConfirmationArmed = false;
									ClearGoalsConfirmationExpiresAt = 0.0;
									OnRequestClearGoals.ExecuteIfBound();
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
	if (InKeyEvent.GetKey() == EKeys::Escape ||
		InKeyEvent.GetKey() == SBCHotkeys::Get(this, ESBCHotkeyAction::ToggleCalculator))
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
	Language = SBCLocalization::GetCurrentLanguage();
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
		if (const FSBCRecipeDefinition* Recipe = Data->FindRecipe(RecipeId); Recipe && !IsUnpackageRecipe(*Recipe) && IsRecipeUnlocked(*Recipe))
		{
			return true;
		}
	}
	return false;
}

bool USBCCalculatorWidget::IsGeneratorBuildingUnlocked(const FString& BuildingId) const
{
	FSatisfactoryBuildCalculatorModule* Module = FModuleManager::GetModulePtr<FSatisfactoryBuildCalculatorModule>(TEXT("SatisfactoryBuildCalculator"));
	const TSharedPtr<FSBCProductionData> Data = Module ? Module->GetProductionData() : nullptr;
	if (!Data) return false;
	for (const TPair<FString, FSBCRecipeDefinition>& Pair : Data->GetRecipes())
	{
		const FSBCRecipeDefinition& Recipe = Pair.Value;
		if (Recipe.BuildingId == BuildingId && IsGeneratorRecipe(Recipe) && IsRecipeUnlocked(Recipe)) return true;
	}
	return false;
}

FText USBCCalculatorWidget::GetRecipeSyncStatus() const
{
	if (!bRecipeStateReady)
	{
		return Text(TEXT("제조법 동기화 대기"), TEXT("Recipe sync pending"), TEXT("等待配方同步"), TEXT("Rezeptsynchronisierung ausstehend"));
	}
	FSatisfactoryBuildCalculatorModule* Module = FModuleManager::GetModulePtr<FSatisfactoryBuildCalculatorModule>(TEXT("SatisfactoryBuildCalculator"));
	const TSharedPtr<FSBCProductionData> Data = Module ? Module->GetProductionData() : nullptr;
	const int32 RuntimeCount = Data ? Data->GetRuntimeRecipeCount() : 0;
	return FText::FromString(SBCLocalization::Format(
		SBCLocalization::String(Language,
			TEXT("해금 {0}개 · 추가 제조법 {1}개"),
			TEXT("{0} unlocked · {1} additional recipes"),
			TEXT("已解锁 {0} 个 · 新增配方 {1} 个"),
			TEXT("{0} freigeschaltet · {1} zusätzliche Rezepte")),
		UnlockedRecipeIds.Num(), RuntimeCount));
}

void USBCCalculatorWidget::RefreshProductList()
{
	if (!ProductListBox.IsValid()) return;
	ProductListBox->ClearChildren();
	FSatisfactoryBuildCalculatorModule* Module = FModuleManager::GetModulePtr<FSatisfactoryBuildCalculatorModule>(TEXT("SatisfactoryBuildCalculator"));
	const TSharedPtr<FSBCProductionData> Data = Module ? Module->GetProductionData() : nullptr;
	if (!Data || !Data->IsLoaded()) return;

	struct FProductListEntry
	{
		FString Id;
		FString Name;
		bool bGenerator = false;
		bool bUnlocked = false;
	};
	TArray<FProductListEntry> Matches;
	for (const TPair<FString, FSBCItemDefinition>& Pair : Data->GetItems())
	{
		const FSBCItemDefinition& Item = Pair.Value;
		if (Item.Id == TEXT("Electricity_MW")) continue;
		if (!Data->FindRecipesForItem(Item.Id)) continue;
		const FString Name = Item.LocalizedName.IsEmpty() ? Item.NameEn : Item.LocalizedName;
		if (SearchText.IsEmpty() || Name.Contains(SearchText, ESearchCase::IgnoreCase))
		{
			Matches.Add({Item.Id, Name, false, IsProductUnlocked(Item.Id)});
		}
	}
	TSet<FString> AddedGeneratorBuildings;
	for (const TPair<FString, FSBCRecipeDefinition>& Pair : Data->GetRecipes())
	{
		const FSBCRecipeDefinition& Recipe = Pair.Value;
		if (!IsGeneratorRecipe(Recipe) || AddedGeneratorBuildings.Contains(Recipe.BuildingId)) continue;
		const FSBCBuildingDefinition* Building = Data->FindBuilding(Recipe.BuildingId);
		if (!Building) continue;
		AddedGeneratorBuildings.Add(Recipe.BuildingId);
		const FString Name = Building->LocalizedName.IsEmpty() ? Building->NameEn : Building->LocalizedName;
		if (SearchText.IsEmpty() || Name.Contains(SearchText, ESearchCase::IgnoreCase))
		{
			Matches.Add({Recipe.BuildingId, Name, true, IsGeneratorBuildingUnlocked(Recipe.BuildingId)});
		}
	}
	Matches.Sort([](const FProductListEntry& A, const FProductListEntry& B)
	{
		return A.Name < B.Name;
	});

	for (const FProductListEntry& Entry : Matches)
	{
		if (bHideLockedProducts && !Entry.bUnlocked)
		{
			continue;
		}
		const FString Locked = SBCLocalization::String(Language, TEXT("잠김"), TEXT("Locked"), TEXT("未解锁"), TEXT("Gesperrt"));
		const FText Name = FText::FromString(Entry.bUnlocked
			? Entry.Name
			: FString::Printf(TEXT("%s  [%s]"), *Entry.Name, *Locked));
		ProductListBox->AddSlot().AutoHeight().Padding(0.0f, 1.0f)
		[
			SNew(SButton)
			.IsEnabled(Entry.bUnlocked)
			.ContentPadding(FMargin(8.0f, 5.0f))
			.OnClicked_Lambda([this, Entry]()
			{
				if (Entry.bGenerator) SelectGenerator(Entry.Id);
				else SelectProduct(Entry.Id);
				return FReply::Handled();
			})
			[
				SNew(STextBlock)
				.Text(Name)
				.Font(FCoreStyle::GetDefaultFontStyle(
					(Entry.bGenerator ? SelectedGeneratorBuildingId == Entry.Id : SelectedGeneratorBuildingId.IsEmpty() && SelectedItemId == Entry.Id)
						? "Bold" : "Regular", 11))
			]
		];
	}
}

void USBCCalculatorWidget::SelectProduct(const FString& ItemId)
{
	if (!IsProductUnlocked(ItemId)) return;
	SelectedItemId = ItemId;
	SelectedGeneratorBuildingId.Reset();
	SelectedRecipes.Reset();
	MachineSettings.Reset();
	CollapsedNodeIds.Reset();
	SelectedGoalNodeId = TEXT("root");
	if (ResultGraph.IsValid()) ResultGraph->ResetView();
	RefreshProductList();
	CalculateSelected();
}

void USBCCalculatorWidget::SelectGenerator(const FString& BuildingId)
{
	if (!IsGeneratorBuildingUnlocked(BuildingId)) return;
	FSatisfactoryBuildCalculatorModule* Module = FModuleManager::GetModulePtr<FSatisfactoryBuildCalculatorModule>(TEXT("SatisfactoryBuildCalculator"));
	const TSharedPtr<FSBCProductionData> Data = Module ? Module->GetProductionData() : nullptr;
	if (!Data) return;

	TArray<const FSBCRecipeDefinition*> Candidates;
	for (const TPair<FString, FSBCRecipeDefinition>& Pair : Data->GetRecipes())
	{
		if (Pair.Value.BuildingId == BuildingId && IsGeneratorRecipe(Pair.Value) && IsRecipeUnlocked(Pair.Value))
		{
			Candidates.Add(&Pair.Value);
		}
	}
	Candidates.Sort([](const FSBCRecipeDefinition& A, const FSBCRecipeDefinition& B)
	{
		if (A.bDefault != B.bDefault) return A.bDefault;
		return A.LocalizedName < B.LocalizedName;
	});
	if (Candidates.IsEmpty()) return;

	SelectedItemId = TEXT("Electricity_MW");
	SelectedGeneratorBuildingId = BuildingId;
	SelectedRecipes.Reset();
	SelectedRecipes.Add(TEXT("root"), Candidates[0]->Id);
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
		ShowMessage(Text(TEXT("계산 데이터를 불러오지 못했습니다."), TEXT("Calculator data could not be loaded."), TEXT("无法加载计算数据。"), TEXT("Die Rechnerdaten konnten nicht geladen werden.")), FLinearColor::White);
		return;
	}
	if (SelectedItemId.IsEmpty())
	{
		ShowMessage(Text(TEXT("왼쪽에서 제품을 선택하세요."), TEXT("Select a product on the left."), TEXT("请在左侧选择产品。"), TEXT("Wähle links ein Produkt aus.")), FLinearColor::White);
		return;
	}
	if (!IsProductUnlocked(SelectedItemId))
	{
		ShowMessage(Text(TEXT("아직 해금되지 않은 제품입니다."), TEXT("This product has not been unlocked yet."), TEXT("该产品尚未解锁。"), TEXT("Dieses Produkt ist noch nicht freigeschaltet.")), FLinearColor(1.0f, 0.62f, 0.14f));
		return;
	}

	FSBCProductionCalculator Calculator(*Data);
	FString Error;
	const TSharedPtr<FSBCProductionNode> Root = Calculator.Calculate(SelectedItemId, TargetRate, SelectedRecipes, MachineSettings, DefaultPowerShards, Language, 0.0, Error);
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
	int32 NextOrder = 0;
	GatherGoalPlan(SelectedRoot ? SelectedRoot : LastCalculatedRoot, Requests, FString(), 0, NextOrder);
	TArray<FSBCCalculatedGoalRequest> Result;
	Requests.GenerateValueArray(Result);
	Result.Sort([](const FSBCCalculatedGoalRequest& A, const FSBCCalculatedGoalRequest& B)
	{
		return A.HierarchyOrder < B.HierarchyOrder;
	});
	return Result;
}

void USBCCalculatorWidget::SetGoalAddResult(
	int32 ExpectedCount,
	int32 AddedCount,
	const TArray<FString>& FailedEntries)
{
	bGoalOperationFailed = AddedCount != ExpectedCount || !FailedEntries.IsEmpty();
	if (!bGoalOperationFailed)
	{
		GoalOperationStatus = SBCLocalization::Format(
			SBCLocalization::String(Language,
				TEXT("건설 목표 {0}/{1}개를 추가했습니다."),
				TEXT("Added {0}/{1} construction goals."),
				TEXT("已添加 {0}/{1} 个建造目标。"),
				TEXT("{0}/{1} Bauziele hinzugefügt.")),
			AddedCount, ExpectedCount);
	}
	else
	{
		const FString FailedLabel = FailedEntries.IsEmpty() ? TEXT("-") : FString::Join(FailedEntries, TEXT(", "));
		GoalOperationStatus = SBCLocalization::Format(
			SBCLocalization::String(Language,
				TEXT("목표 추가 중단: 예상 {0}개, 추가 {1}개 · 확인 필요: {2}"),
				TEXT("Goal import stopped: {0} expected, {1} added · Check: {2}"),
				TEXT("目标导入已停止：预计 {0}，已添加 {1} · 请检查：{2}"),
				TEXT("Zielimport gestoppt: {0} erwartet, {1} hinzugefügt · Prüfen: {2}")),
			ExpectedCount, AddedCount, FailedLabel);
	}
	InvalidateLayoutAndVolatility();
}

void USBCCalculatorWidget::RefreshSummary(const TSharedPtr<FSBCProductionNode>& Root)
{
	if (!SummaryBox.IsValid()) return;
	SummaryBox->ClearChildren();
	TMap<FString, FSBCSummaryEntry> RawResources;
	TMap<FString, FSBCSummaryEntry> RequiredProducts;
	int32 Machines = 0;
	double ConsumptionMW = 0.0;
	double GenerationMW = 0.0;
	GatherProductionSummary(Root, RawResources, RequiredProducts, Machines, ConsumptionMW, GenerationMW);

	SummaryBox->AddSlot().AutoHeight()
	[
		SNew(STextBlock)
		.Text(FText::FromString(SBCLocalization::Format(
			SBCLocalization::String(Language,
				TEXT("생산 요약   설치 {0}대   소비 {1} MW   발전 {2} MW"),
				TEXT("Production summary   {0} machines   {1} MW used   {2} MW generated"),
				TEXT("生产摘要   安装 {0} 台   耗电 {1} MW   发电 {2} MW"),
				TEXT("Produktionsübersicht   {0} Gebäude   {1} MW Verbrauch   {2} MW Erzeugung")),
			Machines, FString::Printf(TEXT("%.1f"), ConsumptionMW), FString::Printf(TEXT("%.1f"), GenerationMW))))
		.ColorAndOpacity(FLinearColor(1.0f, 0.66f, 0.18f))
		.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
	];

	if (!RawResources.IsEmpty())
	{
		TArray<FSBCSummaryEntry> Parts;
		RawResources.GenerateValueArray(Parts);
		Parts.Sort([](const FSBCSummaryEntry& A, const FSBCSummaryEntry& B) { return A.Name < B.Name; });
		TSharedRef<SWrapBox> ItemFlow = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(12.0f, 3.0f));
		for (const FSBCSummaryEntry& Entry : Parts)
		{
			const FSlateBrush* IconBrush = GetItemIconBrush(Entry.ItemId, 16.0f);
			ItemFlow->AddSlot()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SBox).WidthOverride(16.0f).HeightOverride(16.0f)
					.Visibility(IconBrush ? EVisibility::Visible : EVisibility::Collapsed)
					[
						SNew(SImage).Image(IconBrush)
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(FString::Printf(TEXT("%s %s"), *Entry.Name, *FormatRate(Language, Entry.Rate, Entry.Unit))))
					.ColorAndOpacity(FLinearColor(0.68f, 0.78f, 0.80f))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
				]
			];
		}
		SummaryBox->AddSlot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::FromString(SBCLocalization::String(Language, TEXT("원자재"), TEXT("Raw"), TEXT("原材料"), TEXT("Rohstoffe"))))
				.ColorAndOpacity(FLinearColor(0.68f, 0.78f, 0.80f))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
			[
				ItemFlow
			]
		];
	}

	if (!RequiredProducts.IsEmpty())
	{
		TArray<FSBCSummaryEntry> Parts;
		RequiredProducts.GenerateValueArray(Parts);
		Parts.Sort([](const FSBCSummaryEntry& A, const FSBCSummaryEntry& B) { return A.Name < B.Name; });
		TSharedRef<SWrapBox> ItemFlow = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(12.0f, 3.0f));
		for (const FSBCSummaryEntry& Entry : Parts)
		{
			const FSlateBrush* IconBrush = GetItemIconBrush(Entry.ItemId, 16.0f);
			ItemFlow->AddSlot()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SBox).WidthOverride(16.0f).HeightOverride(16.0f)
					.Visibility(IconBrush ? EVisibility::Visible : EVisibility::Collapsed)
					[
						SNew(SImage).Image(IconBrush)
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(FString::Printf(TEXT("%s %s"), *Entry.Name, *FormatRate(Language, Entry.Rate, Entry.Unit))))
					.ColorAndOpacity(FLinearColor(0.72f, 0.82f, 0.84f))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
				]
			];
		}
		SummaryBox->AddSlot().AutoHeight().Padding(0.0f, 3.0f, 0.0f, 0.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::FromString(SBCLocalization::String(Language, TEXT("필요 제품"), TEXT("Required products"), TEXT("所需产品"), TEXT("Benötigte Produkte"))))
				.ColorAndOpacity(FLinearColor(0.72f, 0.82f, 0.84f))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
			[
				ItemFlow
			]
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
		if (!Recipe || IsUnpackageRecipe(*Recipe)) continue;
		const TSharedPtr<FSBCProductionNode> CurrentNode = FindProductionNode(LastCalculatedRoot, NodeId);
		if (CurrentNode && CurrentNode->bGenerator && Recipe->BuildingId != CurrentNode->BuildingId) continue;
		const bool bUnlocked = IsRecipeUnlocked(*Recipe);
		const bool bSelectable = bUnlocked || bAllowLockedRecipes;
		const FString Label = IsGeneratorRecipe(*Recipe)
			? GeneratorRecipeLabel(*Data, *Recipe)
			: (Recipe->LocalizedName.IsEmpty() ? Recipe->NameEn : Recipe->LocalizedName);
		const FString Locked = SBCLocalization::String(Language, TEXT("잠김"), TEXT("Locked"), TEXT("未解锁"), TEXT("Gesperrt"));
		Menu->AddSlot().AutoHeight().Padding(2.0f)
		[
			SNew(SButton)
			.IsEnabled(bSelectable)
			.ContentPadding(FMargin(8.0f, 4.0f))
			.Text(FText::FromString(bUnlocked
				? Label
				: FString::Printf(TEXT("%s  [%s]"), *Label, *Locked)))
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
	if (!bAllowLockedRecipes)
	{
		FSatisfactoryBuildCalculatorModule* Module = FModuleManager::GetModulePtr<FSatisfactoryBuildCalculatorModule>(TEXT("SatisfactoryBuildCalculator"));
		const TSharedPtr<FSBCProductionData> Data = Module ? Module->GetProductionData() : nullptr;
		const FSBCRecipeDefinition* Recipe = Data ? Data->FindRecipe(RecipeId) : nullptr;
		if (!Recipe || !IsRecipeUnlocked(*Recipe)) return;
	}
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
	const float CardWidth = bCompactView ? 225.0f : 315.0f;
	const float CardHeight = bCompactView ? 105.0f : 165.0f;
	const float HorizontalGap = 74.0f;
	const float VerticalGap = 26.0f;
	int32 MaxDepth = 0;

	TFunction<float(const TSharedPtr<FSBCProductionNode>&, int32, const FString&, const FString&, float)> LayoutNode;
	LayoutNode = [this, &LayoutNode, &Entries, &Positions, &MaxDepth,
		CardWidth, CardHeight, HorizontalGap, VerticalGap](
			const TSharedPtr<FSBCProductionNode>& Node,
			int32 Depth,
			const FString& ParentName,
			const FString& ParentNodeId,
			float TopY) -> float
	{
		MaxDepth = FMath::Max(MaxDepth, Depth);
		const FVector2D Position(40.0f + Depth * (CardWidth + HorizontalGap), TopY);
		Positions.Add(Node->NodeId, Position);
		Entries.Add({Node, ParentName, ParentNodeId, Position});

		float ChildrenHeight = 0.0f;
		if (!CollapsedNodeIds.Contains(Node->NodeId))
		{
			for (const TSharedPtr<FSBCProductionNode>& Child : Node->Children)
			{
				ChildrenHeight += LayoutNode(Child, Depth + 1, Node->ItemName, Node->NodeId, TopY + ChildrenHeight);
			}
		}
		return FMath::Max(CardHeight + VerticalGap, ChildrenHeight);
	};

	const float TreeHeight = LayoutNode(Root, 0, FString(), FString(), 40.0f);
	const FVector2D GraphSize(
		80.0f + (MaxDepth + 1) * CardWidth + MaxDepth * HorizontalGap,
		FMath::Max(260.0f, TreeHeight - VerticalGap + 80.0f));
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
	FString Detail;
	if (Node->bRawResource)
	{
		Detail = SBCLocalization::String(Language, TEXT("원자재 · 외부 입력"), TEXT("Raw resource · external input"), TEXT("原材料 · 外部输入"), TEXT("Rohstoff · externe Zufuhr"));
	}
	else if (Node->bGenerator)
	{
		Detail = SBCLocalization::Format(
			SBCLocalization::String(Language, TEXT("{0} · 설치 {1}기"), TEXT("{0} · build {1}"), TEXT("{0} · 建 {1} 台"), TEXT("{0} · Bau {1}")),
			Node->BuildingName, Node->InstalledBuildingCount);
	}
	else
	{
		Detail = SBCLocalization::Format(
			SBCLocalization::String(Language,
				TEXT("{0} · 계산 {1}대 · 설치 {2}대 · {3} MW"),
				TEXT("{0} · {1} required · build {2} · {3} MW"),
				TEXT("{0} · 计算 {1} 台 · 安装 {2} 台 · {3} MW"),
				TEXT("{0} · {1} berechnet · {2} installiert · {3} MW")),
			Node->BuildingName, FString::Printf(TEXT("%.2f"), Node->ExactBuildingCount), Node->InstalledBuildingCount, FString::Printf(TEXT("%.1f"), Node->PowerMW));
	}

	const FString CompactDetail = Node->bRawResource
		? SBCLocalization::String(Language, TEXT("외부 입력"), TEXT("External input"), TEXT("外部输入"), TEXT("Externe Zufuhr"))
		: SBCLocalization::Format(
			SBCLocalization::String(Language, TEXT("{0} · 설치 {1}대"), TEXT("{0} · build {1}"), TEXT("{0} · 建 {1} 台"), TEXT("{0} · Bau {1}")),
			Node->BuildingName, Node->InstalledBuildingCount);
	FString ShortStats;
	if (!Node->bRawResource && !Node->bGenerator)
	{
		ShortStats = SBCLocalization::Format(
			SBCLocalization::String(Language,
				TEXT("필요 {0} / 설치 {1} · {2} MW"),
				TEXT("req. {0} / build {1} · {2} MW"),
				TEXT("需 {0} / 建 {1} · {2} MW"),
				TEXT("Bed. {0} / Bau {1} · {2} MW")),
			FString::Printf(TEXT("%.2f"), Node->ExactBuildingCount), Node->InstalledBuildingCount, FString::Printf(TEXT("%.1f"), Node->PowerMW));
	}
	else if (Node->bGenerator)
	{
		ShortStats = SBCLocalization::Format(
			SBCLocalization::String(Language, TEXT("설치 {0}기"), TEXT("build {0}"), TEXT("建 {0} 台"), TEXT("Bau {0}")),
			Node->InstalledBuildingCount);
	}

	TSharedRef<SWidget> DetailWidget = SNew(STextBlock)
		.Text(FText::FromString(bCompactView ? CompactDetail : Detail))
		.ToolTipText(FText::FromString(Detail))
		.ColorAndOpacity(FLinearColor(0.58f, 0.75f, 0.78f))
		.Font(FCoreStyle::GetDefaultFontStyle("Regular", bCompactView ? 8 : 9))
		.AutoWrapText(false)
		.OverflowPolicy(ETextOverflowPolicy::Ellipsis);
	if (!bCompactView && !Node->bRawResource)
	{
		DetailWidget = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(0.0f, 0.0f, 6.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Node->BuildingName))
				.ToolTipText(FText::FromString(Node->BuildingName))
				.ColorAndOpacity(FLinearColor(0.58f, 0.75f, 0.78f))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
				.AutoWrapText(false)
				.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(ShortStats))
				.ToolTipText(FText::FromString(Detail))
				.ColorAndOpacity(FLinearColor(0.66f, 0.80f, 0.82f))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
				.AutoWrapText(false)
			];
	}
	const FMargin CardPadding = bCompactView ? FMargin(8.0f, 6.0f) : FMargin(10.0f, 8.0f);
	FSatisfactoryBuildCalculatorModule* Module = FModuleManager::GetModulePtr<FSatisfactoryBuildCalculatorModule>(TEXT("SatisfactoryBuildCalculator"));
	const TSharedPtr<FSBCProductionData> Data = Module ? Module->GetProductionData() : nullptr;
	const TArray<FString>* CandidateRecipes = Data ? Data->FindRecipesForItem(Node->ItemId) : nullptr;
	int32 CompatibleRecipeCount = 0;
	if (CandidateRecipes && Data)
	{
		for (const FString& RecipeId : *CandidateRecipes)
		{
			const FSBCRecipeDefinition* Recipe = Data->FindRecipe(RecipeId);
			if (Recipe && !IsUnpackageRecipe(*Recipe) && (!Node->bGenerator || Recipe->BuildingId == Node->BuildingId)) ++CompatibleRecipeCount;
		}
	}
	FString RecipeButtonLabel = Node->RecipeName;
	if (Node->bGenerator && Data)
	{
		if (const FSBCRecipeDefinition* Recipe = Data->FindRecipe(Node->RecipeId)) RecipeButtonLabel = GeneratorRecipeLabel(*Data, *Recipe);
	}
	const FSBCBuildingDefinition* Building = Data ? Data->FindBuilding(Node->BuildingId) : nullptr;
	const int32 MaxSomersloops = Building ? Building->SloopSlots : 0;
	const FString NodeId = Node->NodeId;
	const FString ItemId = Node->ItemId;
	const bool bHasChildren = !Node->Children.IsEmpty();
	const bool bCollapsed = CollapsedNodeIds.Contains(NodeId);
	const bool bSelected = SelectedGoalNodeId == NodeId;
	const bool bRoot = NodeId == TEXT("root");
	const FLinearColor Accent = GetBuildingColor(Node);
	const FSlateBrush* ItemIconBrush = GetItemIconBrush(Node->ItemId, bCompactView ? 20.0f : 24.0f);
	return SNew(SBorder)
	.Padding(bSelected ? 3.0f : 1.0f)
	.BorderBackgroundColor(bSelected
		? FLinearColor(1.0f, 0.80f, 0.18f, 1.0f)
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
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 6.0f, 0.0f)
						[
							SNew(SBox)
							.WidthOverride(bCompactView ? 20.0f : 24.0f)
							.HeightOverride(bCompactView ? 20.0f : 24.0f)
							.Visibility(ItemIconBrush ? EVisibility::Visible : EVisibility::Collapsed)
							[
								SNew(SImage).Image(ItemIconBrush)
							]
						]
						+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(FText::FromString(Node->ItemName))
							.ToolTipText(FText::FromString(Node->ItemName))
							.ColorAndOpacity(bRoot ? FLinearColor(1.0f, 0.70f, 0.27f) : FLinearColor::White)
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", bCompactView ? 11 : (bRoot ? 14 : 12)))
							.AutoWrapText(false)
							.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.0f, 0.0f, 0.0f, 0.0f)
						[
							SNew(SBox).WidthOverride(84.0f).HeightOverride(22.0f)
							[
								SNew(SButton)
								.ContentPadding(FMargin(3.0f, 1.0f))
								.HAlign(HAlign_Center)
								.VAlign(VAlign_Center)
								.OnClicked_Lambda([this, NodeId]() { SelectGoalNode(NodeId); return FReply::Handled(); })
								[
									SNew(STextBlock)
									.Text(bSelected
										? Text(TEXT("선택됨"), TEXT("Selected"), TEXT("已选择"), TEXT("Gewählt"))
										: Text(TEXT("선택"), TEXT("Select"), TEXT("选择"), TEXT("Wählen")))
									.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
									.Justification(ETextJustify::Center)
								]
							]
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f, 0.0f, 0.0f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(FText::FromString(Node->bGenerator
								? FString::Printf(TEXT("%.2fMW"), Node->RequiredRate)
								: FormatRate(Language, Node->RequiredRate, Node->Unit)))
							.ColorAndOpacity(FLinearColor(0.95f, 0.97f, 0.98f))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", bCompactView ? 10 : 12))
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.0f, 0.0f, 0.0f, 0.0f)
						[
							SNew(SBox).WidthOverride(84.0f).HeightOverride(22.0f)
							.Visibility(bHasChildren ? EVisibility::Visible : EVisibility::Collapsed)
							[
								SNew(SButton)
								.ContentPadding(FMargin(3.0f, 1.0f))
								.HAlign(HAlign_Center)
								.VAlign(VAlign_Center)
								.OnClicked_Lambda([this, NodeId]() { ToggleNodeCollapsed(NodeId); return FReply::Handled(); })
								[
									SNew(STextBlock)
									.Text(bCollapsed
										? Text(TEXT("펴기"), TEXT("Expand"), TEXT("展开"), TEXT("Aufklappen"))
										: Text(TEXT("접기"), TEXT("Collapse"), TEXT("折叠"), TEXT("Einklappen")))
									.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
									.Justification(ETextJustify::Center)
								]
							]
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
					[
						DetailWidget
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
					[
						SNew(SComboButton)
						.Visibility(!bCompactView && !Node->bRawResource ? EVisibility::Visible : EVisibility::Collapsed)
						.IsEnabled(CompatibleRecipeCount > 1)
						.OnGetMenuContent_Lambda([this, NodeId, ItemId]() { return BuildRecipeMenu(NodeId, ItemId); })
						.ButtonContent()
						[
							SNew(STextBlock)
							.Text(FText::FromString(RecipeButtonLabel))
							.ToolTipText(FText::FromString(RecipeButtonLabel))
							.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
							.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 5.0f, 0.0f, 0.0f)
					[
						SNew(SHorizontalBox)
						.Visibility(!bCompactView && !Node->bRawResource && !Node->bGenerator ? EVisibility::Visible : EVisibility::Collapsed)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 4.0f, 0.0f)
						[
							SNew(STextBlock)
							.Text(Text(TEXT("동력핵"), TEXT("Shards"), TEXT("能量碎片"), TEXT("Splitter")))
							.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
							.Visibility(!bCompactView && !Node->bRawResource && !Node->bGenerator ? EVisibility::Visible : EVisibility::Collapsed)
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
						[
							SNew(SSpinBox<int32>)
							.MinValue(0).MaxValue(3).MinSliderValue(0).MaxSliderValue(3)
							.Value(Node->PowerShards)
							.Visibility(!bCompactView && !Node->bRawResource && !Node->bGenerator ? EVisibility::Visible : EVisibility::Collapsed)
							.OnValueCommitted_Lambda([this, NodeId](int32 Value, ETextCommit::Type) { SetNodePowerShards(NodeId, Value); })
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 4.0f, 0.0f)
						[
							SNew(STextBlock)
							.Text(Text(TEXT("소머슬룹"), TEXT("Somersloops"), TEXT("增幅器"), TEXT("Somersloops")))
							.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
							.Visibility(!bCompactView && !Node->bRawResource && !Node->bGenerator && MaxSomersloops > 0 ? EVisibility::Visible : EVisibility::Collapsed)
						]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(SSpinBox<int32>)
							.MinValue(0).MaxValue(MaxSomersloops).MinSliderValue(0).MaxSliderValue(MaxSomersloops)
							.Value(Node->Somersloops)
							.Visibility(!bCompactView && !Node->bRawResource && !Node->bGenerator && MaxSomersloops > 0 ? EVisibility::Visible : EVisibility::Collapsed)
							.OnValueCommitted_Lambda([this, NodeId](int32 Value, ETextCommit::Type) { SetNodeSomersloops(NodeId, Value); })
						]
					]
				]
			]
		]
	];
}
