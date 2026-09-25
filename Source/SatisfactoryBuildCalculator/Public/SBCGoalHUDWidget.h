#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SBCGoalHUDWidget.generated.h"

class AFGCharacterPlayer;
class SVerticalBox;

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
	TSharedPtr<SVerticalBox> ContentBox;
	int32 SelectedGoalIndex = 0;
};
