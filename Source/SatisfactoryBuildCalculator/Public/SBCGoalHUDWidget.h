#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SBCGoalHUDWidget.generated.h"

class AFGCharacterPlayer;
class SVerticalBox;
class SScrollBox;

UCLASS()
class SATISFACTORYBUILDCALCULATOR_API USBCGoalHUDWidget final : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetObservedPlayer(AFGCharacterPlayer* Player);
	void SetSelectedGoalIndex(int32 Index);
	void RefreshGoals();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	TWeakObjectPtr<AFGCharacterPlayer> ObservedPlayer;
	TSharedPtr<SVerticalBox> GoalListBox;
	TSharedPtr<SScrollBox> GoalScrollBox;
	int32 SelectedGoalIndex = 0;
	double SelectedHighlightUntil = 0.0;
	uint32 LastRenderedStateHash = 0;
	bool bHasRenderedState = false;
};
