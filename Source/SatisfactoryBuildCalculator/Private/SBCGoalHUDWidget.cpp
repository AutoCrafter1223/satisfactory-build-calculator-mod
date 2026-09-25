#include "SBCGoalHUDWidget.h"

#include "Buildables/FGBuildable.h"
#include "FGCharacterPlayer.h"
#include "FGRecipe.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "SBCGoalSubsystem.h"
#include "SBCHotkeyConfig.h"
#include "SBCLocalization.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

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
			.Text(FText::FromString(SBCLocalization::Format(
				SBCLocalization::String(Language,
					TEXT("생산시설을 조준하고 {0}을 눌러 목표를 추가하세요."),
					TEXT("Aim at a production building and press {0} to add a goal."),
					TEXT("瞄准生产建筑并按 {0} 添加目标。"),
					TEXT("Ziele auf ein Produktionsgebäude und drücke {0}, um ein Ziel hinzuzufügen.")),
				SBCHotkeys::GetDisplayName(this, ESBCHotkeyAction::AddAimedBuilding).ToString())))
			.ColorAndOpacity(FLinearColor(0.72f, 0.78f, 0.82f))
			.AutoWrapText(true)
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 12))
		];
	}

	TSharedPtr<SWidget> SelectedCard;
	const int32 ClampedSelectedIndex = FMath::Clamp(SelectedGoalIndex, 0, FMath::Max(0, Goals.Num() - 1));
	for (int32 Index = 0; Index < Goals.Num(); ++Index)
	{
		const FSBCBuildGoal& Goal = Goals[Index];
		const bool bSelected = Index == ClampedSelectedIndex;
		const bool bComplete = Goal.IsComplete();
		const FString Progress = FString::Printf(TEXT("%d / %d"), Goal.CompletedCount, Goal.TargetCount);
		const FString Completion = Goal.bManuallyCompleted
			? SBCLocalization::String(Language, TEXT("수동 완료"), TEXT("MANUAL"), TEXT("手动完成"), TEXT("MANUELL"))
			: (bComplete ? SBCLocalization::String(Language, TEXT("완료"), TEXT("DONE"), TEXT("完成"), TEXT("FERTIG")) : TEXT(""));

		TSharedPtr<SBorder> GoalCard;
		GoalListBox->AddSlot().AutoHeight().Padding(0.0f, 2.0f)
		[
			SAssignNew(GoalCard, SBorder)
			.Padding(bSelected ? FMargin(2.0f) : FMargin(0.0f))
			.BorderBackgroundColor(bSelected ? FLinearColor(1.0f, 0.55f, 0.08f, 0.95f) : FLinearColor::Transparent)
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
