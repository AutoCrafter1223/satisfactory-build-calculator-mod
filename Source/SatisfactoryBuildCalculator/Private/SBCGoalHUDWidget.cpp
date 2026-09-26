#include "SBCGoalHUDWidget.h"

#include "Buildables/FGBuildable.h"
#include "FGCharacterPlayer.h"
#include "FGInventoryComponent.h"
#include "FGRecipe.h"
#include "Engine/Texture2D.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "SBCGoalSubsystem.h"
#include "SBCHotkeyConfig.h"
#include "SBCLocalization.h"
#include "Styling/CoreStyle.h"
#include "Resources/FGItemDescriptor.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Rendering/DrawElements.h"

class SSBCHierarchyGuide final : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SSBCHierarchyGuide) {}
		SLATE_ARGUMENT(int32, Depth)
		SLATE_ARGUMENT(TArray<bool>, ContinuationColumns)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		Depth = FMath::Clamp(InArgs._Depth, 0, 5);
		ContinuationColumns = InArgs._ContinuationColumns;
	}

	virtual FVector2D ComputeDesiredSize(float) const override
	{
		return FVector2D(Depth > 0 ? Depth * ColumnWidth + 4.0f : 0.0f, 34.0f);
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
		const float Height = AllottedGeometry.GetLocalSize().Y;
		const float MidY = Height * 0.5f;
		const FLinearColor LineColor(0.38f, 0.72f, 0.90f, 0.92f);
		for (int32 Column = 0; Column < Depth; ++Column)
		{
			const float X = 10.0f + Column * ColumnWidth;
			const bool bContinues = ContinuationColumns.IsValidIndex(Column) && ContinuationColumns[Column];
			const bool bCurrentBranch = Column == Depth - 1;
			if (!bCurrentBranch && !bContinues)
			{
				continue;
			}

			const float EndY = bContinues ? Height : MidY;
			TArray<FVector2D> Vertical{FVector2D(X, 0.0f), FVector2D(X, EndY)};
			FSlateDrawElement::MakeLines(
				OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Vertical,
				ESlateDrawEffect::None, LineColor, true, 1.5f);

			if (bCurrentBranch)
			{
				TArray<FVector2D> Horizontal{FVector2D(X, MidY), FVector2D(AllottedGeometry.GetLocalSize().X, MidY)};
				FSlateDrawElement::MakeLines(
					OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Horizontal,
					ESlateDrawEffect::None, LineColor, true, 1.5f);
			}
		}
		return LayerId + 1;
	}

private:
	static constexpr float ColumnWidth = 14.0f;
	int32 Depth = 0;
	TArray<bool> ContinuationColumns;
};

namespace
{
	FText GoalTitle(const FSBCBuildGoal& Goal, AFGCharacterPlayer* Player)
	{
		if (Goal.RecipeClass)
		{
			return Goal.RecipeClass->GetDefaultObject<UFGRecipe>()->GetDisplayName();
		}
		if (Goal.BuildableClass)
		{
			return Goal.BuildableClass->GetDefaultObject<AFGBuildable>()->GetDismantleDisplayName_Implementation(Player);
		}
		return SBCLocalization::Text(SBCLocalization::GetCurrentLanguage(), TEXT("알 수 없는 목표"), TEXT("Unknown goal"), TEXT("未知目标"), TEXT("Unbekanntes Ziel"));
	}

	FText BuildingTitle(const FSBCBuildGoal& Goal, AFGCharacterPlayer* Player)
	{
		if (Goal.BuildableClass)
		{
			return Goal.BuildableClass->GetDefaultObject<AFGBuildable>()->GetDismantleDisplayName_Implementation(Player);
		}
		return SBCLocalization::Text(SBCLocalization::GetCurrentLanguage(), TEXT("생산시설 미지정"), TEXT("No building"), TEXT("未指定建筑"), TEXT("Kein Gebäude"));
	}
}

const FSlateBrush* USBCGoalHUDWidget::GetGoalIconBrush(const FSBCBuildGoal& Goal, float Size)
{
	if (!Goal.RecipeClass)
	{
		return nullptr;
	}

	const TArray<FItemAmount> Products = UFGRecipe::GetProducts(Goal.RecipeClass);
	if (Products.IsEmpty() || !Products[0].ItemClass)
	{
		return nullptr;
	}

	const FString ItemId = Products[0].ItemClass->GetName();
	if (const TSharedPtr<FSlateBrush>* Existing = ItemIconBrushes.Find(ItemId))
	{
		return Existing->Get();
	}

	UTexture2D* Texture = UFGItemDescriptor::GetSmallIcon(Products[0].ItemClass);
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

TSharedRef<SWidget> USBCGoalHUDWidget::RebuildWidget()
{
	TSharedRef<SWidget> Result =
		SNew(SBox)
		.WidthOverride(312.0f)
		.HeightOverride(460.0f)
		[
			SNew(SBorder)
			.Padding(FMargin(12.0f, 10.0f))
			.BorderBackgroundColor(FLinearColor(0.025f, 0.055f, 0.07f, 0.68f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
				[
					SNew(STextBlock)
					.Text_Lambda([]() { return SBCLocalization::Text(SBCLocalization::GetCurrentLanguage(), TEXT("건설 목표"), TEXT("BUILD GOALS"), TEXT("建造目标"), TEXT("BAUZIELE")); })
					.ColorAndOpacity(FLinearColor(1.0f, 0.60f, 0.12f))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 17))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Text_Lambda([this]()
					{
						const FString Key = SBCHotkeys::GetDisplayName(this, ESBCHotkeyAction::ManualCompletion).ToString();
						return FText::FromString(SBCLocalization::Format(
							SBCLocalization::String(SBCLocalization::GetCurrentLanguage(),
								TEXT("↑↓ 선택 · +/- 수량 · {0} 완료/취소"),
								TEXT("↑↓ Select · +/- Count · {0} Complete/Undo"),
								TEXT("↑↓ 选择 · +/- 数量 · {0} 完成/撤销"),
								TEXT("↑↓ Auswahl · +/- Anzahl · {0} Fertig/Zurück")),
							Key));
					})
					.ColorAndOpacity(FLinearColor(0.66f, 0.73f, 0.77f))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 1.0f, 0.0f, 7.0f)
				[
					SNew(STextBlock)
					.Text_Lambda([this]()
					{
						const FString AddKey = SBCHotkeys::GetDisplayName(this, ESBCHotkeyAction::AddAimedBuilding).ToString();
						const FString HudKey = SBCHotkeys::GetDisplayName(this, ESBCHotkeyAction::ToggleGoalHUD).ToString();
						return FText::FromString(SBCLocalization::Format(
							SBCLocalization::String(SBCLocalization::GetCurrentLanguage(),
								TEXT("{0} 시설 추가 · L 연결 · Del 삭제 · {1} 숨김"),
								TEXT("{0} Add · L Link · Del Remove · {1} Hide"),
								TEXT("{0} 添加设施 · L 连接 · Del 删除 · {1} 隐藏"),
								TEXT("{0} Gebäude · L Verbinden · Entf Löschen · {1} Ausblenden")),
							AddKey, HudKey));
					})
					.ColorAndOpacity(FLinearColor(0.53f, 0.63f, 0.68f))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
				]
				+ SVerticalBox::Slot().FillHeight(1.0f)
				[
					SAssignNew(GoalScrollBox, SScrollBox)
					+ SScrollBox::Slot()
					[
						SAssignNew(GoalListBox, SVerticalBox)
					]
				]
			]
		];

	bHasRenderedState = false;
	RefreshGoals();
	return Result;
}

void USBCGoalHUDWidget::SetObservedPlayer(AFGCharacterPlayer* Player)
{
	if (ObservedPlayer.Get() == Player)
	{
		return;
	}

	ObservedPlayer = Player;
	bHasRenderedState = false;
	RefreshGoals();
}

void USBCGoalHUDWidget::SetSelectedGoalIndex(int32 Index)
{
	SelectedGoalIndex = FMath::Max(0, Index);
	SelectedHighlightUntil = FPlatformTime::Seconds() + 5.0;
	bHasRenderedState = false;
	RefreshGoals();
}

void USBCGoalHUDWidget::RefreshGoals()
{
	if (!GoalListBox.IsValid())
	{
		return;
	}

	const ESBCLanguage Language = SBCLocalization::GetCurrentLanguage();
	AFGCharacterPlayer* Player = ObservedPlayer.Get();
	ASBCGoalSubsystem* Subsystem = ASBCGoalSubsystem::Get(Player);
	const TArray<FSBCBuildGoal> Goals = Subsystem ? Subsystem->GetGoalsForPlayer(Player) : TArray<FSBCBuildGoal>();

	uint32 StateHash = GetTypeHash(static_cast<uint8>(Language));
	StateHash = HashCombine(StateHash, GetTypeHash(SelectedGoalIndex));
	StateHash = HashCombine(StateHash, GetTypeHash(Goals.Num()));
	for (const FSBCBuildGoal& Goal : Goals)
	{
		StateHash = HashCombine(StateHash, GetTypeHash(Goal.GoalId));
		StateHash = HashCombine(StateHash, GetTypeHash(Goal.BuildableClass));
		StateHash = HashCombine(StateHash, GetTypeHash(Goal.RecipeClass));
		StateHash = HashCombine(StateHash, GetTypeHash(Goal.TargetCount));
		StateHash = HashCombine(StateHash, GetTypeHash(Goal.CompletedCount));
		StateHash = HashCombine(StateHash, GetTypeHash(Goal.bManuallyCompleted));
		StateHash = HashCombine(StateHash, GetTypeHash(Goal.GoalGroupId));
		StateHash = HashCombine(StateHash, GetTypeHash(Goal.HierarchyKey));
		StateHash = HashCombine(StateHash, GetTypeHash(Goal.ParentHierarchyKey));
		StateHash = HashCombine(StateHash, GetTypeHash(Goal.HierarchyDepth));
		StateHash = HashCombine(StateHash, GetTypeHash(Goal.HierarchyOrder));
	}

	if (bHasRenderedState && LastRenderedStateHash == StateHash)
	{
		return;
	}
	LastRenderedStateHash = StateHash;
	bHasRenderedState = true;

	GoalListBox->ClearChildren();
	if (Goals.IsEmpty())
	{
		GoalListBox->AddSlot().AutoHeight()
		[
			SNew(STextBlock)
			.Text_Lambda([this]()
			{
				return FText::FromString(SBCLocalization::Format(
					SBCLocalization::String(SBCLocalization::GetCurrentLanguage(),
						TEXT("생산시설을 조준하고 {0}을 눌러 목표를 추가하세요."),
						TEXT("Aim at a production building and press {0} to add a goal."),
						TEXT("瞄准生产建筑并按 {0} 添加目标。"),
						TEXT("Ziele auf ein Produktionsgebäude und drücke {0}, um ein Ziel hinzuzufügen.")),
					SBCHotkeys::GetDisplayName(this, ESBCHotkeyAction::AddAimedBuilding).ToString()));
			})
			.ColorAndOpacity(FLinearColor(0.72f, 0.78f, 0.82f))
			.AutoWrapText(true)
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 12))
		];
	}

	TSharedPtr<SWidget> SelectedCard;
	const int32 ClampedSelectedIndex = FMath::Clamp(SelectedGoalIndex, 0, FMath::Max(0, Goals.Num() - 1));
	auto VisibleHierarchyKey = [](const FSBCBuildGoal& Goal)
	{
		return Goal.GoalGroupId.IsValid() && !Goal.HierarchyKey.IsEmpty()
			? Goal.GoalGroupId.ToString(EGuidFormats::Digits) + TEXT("|") + Goal.HierarchyKey
			: FString();
	};
	TMap<FString, int32> VisibleGoalIndices;
	for (int32 GoalIndex = 0; GoalIndex < Goals.Num(); ++GoalIndex)
	{
		const FString Key = VisibleHierarchyKey(Goals[GoalIndex]);
		if (!Key.IsEmpty()) VisibleGoalIndices.Add(Key, GoalIndex);
	}
	for (int32 Index = 0; Index < Goals.Num(); ++Index)
	{
		const FSBCBuildGoal& Goal = Goals[Index];
		const bool bSelected = Index == ClampedSelectedIndex;
		const bool bComplete = Goal.IsComplete();
		const FString Progress = FString::Printf(TEXT("%d / %d"), Goal.CompletedCount, Goal.TargetCount);
		const FString Completion = Goal.bManuallyCompleted
			? SBCLocalization::String(Language, TEXT("수동 완료"), TEXT("MANUAL"), TEXT("手动完成"), TEXT("MANUELL"))
			: (bComplete ? SBCLocalization::String(Language, TEXT("완료"), TEXT("DONE"), TEXT("完成"), TEXT("FERTIG")) : TEXT(""));
		const FSlateBrush* GoalIconBrush = GetGoalIconBrush(Goal, 22.0f);
		// Reconstruct the hierarchy from visible parent keys. A completed/hidden
		// parent therefore cannot leave an orphaned rail on the far left.
		TArray<int32> VisiblePath;
		TSet<int32> VisitedIndices;
		int32 PathIndex = Index;
		while (Goals.IsValidIndex(PathIndex) && !VisitedIndices.Contains(PathIndex))
		{
			VisitedIndices.Add(PathIndex);
			VisiblePath.Insert(PathIndex, 0);
			const FSBCBuildGoal& PathGoal = Goals[PathIndex];
			if (!PathGoal.GoalGroupId.IsValid() || PathGoal.ParentHierarchyKey.IsEmpty()) break;
			const FString ParentKey = PathGoal.GoalGroupId.ToString(EGuidFormats::Digits) + TEXT("|") + PathGoal.ParentHierarchyKey;
			const int32* ParentIndex = VisibleGoalIndices.Find(ParentKey);
			if (!ParentIndex || *ParentIndex >= PathIndex) break;
			PathIndex = *ParentIndex;
		}
		if (VisiblePath.Num() > 6)
		{
			VisiblePath.RemoveAt(0, VisiblePath.Num() - 6);
		}
		const int32 VisibleDepth = FMath::Max(0, VisiblePath.Num() - 1);
		TArray<bool> ContinuationColumns;
		ContinuationColumns.Reserve(VisibleDepth);
		for (int32 Column = 0; Column < VisibleDepth; ++Column)
		{
			bool bHasLaterBranch = false;
			const FSBCBuildGoal& BranchGoal = Goals[VisiblePath[Column + 1]];
			for (int32 LaterIndex = Index + 1; LaterIndex < Goals.Num(); ++LaterIndex)
			{
				const FSBCBuildGoal& LaterGoal = Goals[LaterIndex];
				if (LaterGoal.GoalGroupId != Goal.GoalGroupId) break;
				if (LaterGoal.ParentHierarchyKey == BranchGoal.ParentHierarchyKey &&
					LaterGoal.HierarchyKey != BranchGoal.HierarchyKey)
				{
					bHasLaterBranch = true;
					break;
				}
			}
			ContinuationColumns.Add(bHasLaterBranch);
		}

		TSharedPtr<SBorder> GoalCard;
		GoalListBox->AddSlot().AutoHeight().Padding(0.0f, 2.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Fill)
			[
				SNew(SSBCHierarchyGuide)
				.Depth(VisibleDepth)
				.ContinuationColumns(ContinuationColumns)
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f)
			[
				SAssignNew(GoalCard, SBorder)
				.Padding(bSelected ? FMargin(3.0f) : FMargin(0.0f))
				.BorderBackgroundColor(bSelected ? FLinearColor(1.0f, 0.72f, 0.10f, 1.0f) : FLinearColor::Transparent)
				[
				SNew(SBorder)
				.Padding(FMargin(8.0f, 6.0f))
				.BorderBackgroundColor_Lambda([this, bSelected]()
				{
					if (!bSelected)
					{
						return FSlateColor(FLinearColor(0.05f, 0.09f, 0.11f, 0.70f));
					}
					return FSlateColor(FPlatformTime::Seconds() < SelectedHighlightUntil
						? FLinearColor(0.18f, 0.30f, 0.31f, 0.94f)
						: FLinearColor(0.10f, 0.18f, 0.20f, 0.82f));
				})
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 6.0f, 0.0f)
					[
						SNew(SBox).WidthOverride(22.0f).HeightOverride(22.0f)
						.Visibility(GoalIconBrush ? EVisibility::Visible : EVisibility::Collapsed)
						[
							SNew(SImage).Image(GoalIconBrush)
						]
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(STextBlock)
							.Text(GoalTitle(Goal, Player))
							.ColorAndOpacity(bComplete ? FLinearColor(0.35f, 0.90f, 0.68f) : FLinearColor::White)
							.Font(FCoreStyle::GetDefaultFontStyle(bSelected ? "Bold" : "Regular", 13))
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 1.0f, 0.0f, 0.0f)
						[
							SNew(STextBlock)
							.Text(BuildingTitle(Goal, Player))
							.ColorAndOpacity(FLinearColor(0.55f, 0.72f, 0.78f))
							.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(10.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(FText::FromString(Progress))
						.ColorAndOpacity(FLinearColor(1.0f, 0.68f, 0.22f))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(STextBlock)
						.Text(FText::FromString(Completion))
						.ColorAndOpacity(FLinearColor(0.35f, 0.90f, 0.68f))
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))
					]
				]
				]
			]
		];

		if (bSelected)
		{
			SelectedCard = GoalCard;
		}
	}

	if (GoalScrollBox.IsValid() && SelectedCard.IsValid())
	{
		GoalScrollBox->ScrollDescendantIntoView(SelectedCard, true, EDescendantScrollDestination::Center);
	}
}
