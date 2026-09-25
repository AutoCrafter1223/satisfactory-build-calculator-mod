#include "SBCGoalHUDWidget.h"

#include "Buildables/FGBuildable.h"
#include "FGCharacterPlayer.h"
#include "FGRecipe.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "SBCGoalSubsystem.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	bool IsKorean()
	{
		return FInternationalization::Get().GetCurrentCulture()->GetTwoLetterISOLanguageName() == TEXT("ko");
	}

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
		return IsKorean() ? FText::FromString(TEXT("알 수 없는 목표")) : FText::FromString(TEXT("Unknown goal"));
	}
}

TSharedRef<SWidget> USBCGoalHUDWidget::RebuildWidget()
{
	TSharedRef<SWidget> Result =
		SNew(SBox)
		.WidthOverride(370.0f)
		[
			SNew(SBorder)
			.Padding(FMargin(12.0f, 10.0f))
			.BorderBackgroundColor(FLinearColor(0.025f, 0.055f, 0.07f, 0.68f))
			[
				SAssignNew(ContentBox, SVerticalBox)
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
	const int32 NewIndex = FMath::Max(0, Index);
	if (SelectedGoalIndex == NewIndex)
	{
		return;
	}

	SelectedGoalIndex = NewIndex;
	RefreshGoals();
}

void USBCGoalHUDWidget::RefreshGoals()
{
	if (!ContentBox.IsValid())
	{
		return;
	}

	const bool bKorean = IsKorean();
	AFGCharacterPlayer* Player = ObservedPlayer.Get();
	ASBCGoalSubsystem* Subsystem = ASBCGoalSubsystem::Get(Player);
	const TArray<FSBCBuildGoal> Goals = Subsystem ? Subsystem->GetGoalsForPlayer(Player) : TArray<FSBCBuildGoal>();

	// The HUD is polled so replicated goal changes appear promptly, but rebuilding the
	// entire Slate tree every poll causes visible flashing. Only rebuild when something
	// the player can see has actually changed.
	uint32 StateHash = GetTypeHash(bKorean);
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

	ContentBox->ClearChildren();
	ContentBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
	[
		SNew(STextBlock)
		.Text(bKorean ? FText::FromString(TEXT("건설 목표")) : FText::FromString(TEXT("BUILD GOALS")))
		.ColorAndOpacity(FLinearColor(1.0f, 0.60f, 0.12f))
		.Font(FCoreStyle::GetDefaultFontStyle("Bold", 17))
	];

	if (Goals.IsEmpty())
	{
		ContentBox->AddSlot().AutoHeight()
		[
			SNew(STextBlock)
			.Text(bKorean ? FText::FromString(TEXT("생산시설을 조준하고 F7을 눌러 목표를 추가하세요."))
				: FText::FromString(TEXT("Aim at a production building and press F7 to add a goal.")))
			.ColorAndOpacity(FLinearColor(0.72f, 0.78f, 0.82f))
			.AutoWrapText(true)
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 12))
		];
	}

	for (int32 Index = 0; Index < Goals.Num(); ++Index)
	{
		const FSBCBuildGoal& Goal = Goals[Index];
		const bool bSelected = Index == FMath::Clamp(SelectedGoalIndex, 0, FMath::Max(0, Goals.Num() - 1));
		const bool bComplete = Goal.IsComplete();
		const FString Progress = FString::Printf(TEXT("%d / %d"), Goal.CompletedCount, Goal.TargetCount);
		const FString Completion = Goal.bManuallyCompleted
			? (bKorean ? TEXT("수동 완료") : TEXT("MANUAL"))
			: (bComplete ? (bKorean ? TEXT("완료") : TEXT("DONE")) : TEXT(""));

		ContentBox->AddSlot().AutoHeight().Padding(0.0f, 2.0f)
		[
			SNew(SBorder)
			.Padding(FMargin(8.0f, 6.0f))
			.BorderBackgroundColor(bSelected ? FLinearColor(0.10f, 0.18f, 0.20f, 0.82f) : FLinearColor(0.05f, 0.09f, 0.11f, 0.70f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.0f)
				[
					SNew(STextBlock)
					.Text(GoalTitle(Goal, Player))
					.ColorAndOpacity(bComplete ? FLinearColor(0.35f, 0.90f, 0.68f) : FLinearColor::White)
					.Font(FCoreStyle::GetDefaultFontStyle(bSelected ? "Bold" : "Regular", 13))
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
					.Text(FText::FromString(Completion.IsEmpty() ? TEXT("") : FString(TEXT("  ✓ ")) + Completion))
					.ColorAndOpacity(FLinearColor(0.35f, 0.90f, 0.68f))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))
				]
			]
		];
	}

	ContentBox->AddSlot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
	[
		SNew(STextBlock)
		.Text(bKorean
			? FText::FromString(TEXT("↑↓ 선택  +/- 수량  Enter 수동 완료  L 기존 시설 연결  Delete 삭제  F8 숨기기"))
			: FText::FromString(TEXT("↑↓ Select  +/- Count  Enter Manual  L Link existing  Delete Remove  F8 Hide")))
		.ColorAndOpacity(FLinearColor(0.52f, 0.62f, 0.67f))
		.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
	];
}
