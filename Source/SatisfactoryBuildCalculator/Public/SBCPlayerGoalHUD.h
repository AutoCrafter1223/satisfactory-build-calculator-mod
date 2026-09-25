#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SBCPlayerGoalHUD.generated.h"

class AFGBuildable;
class AFGCharacterPlayer;
class AFGPlayerController;
class USBCGoalHUDWidget;
class USBCCalculatorWidget;
class USBCRemoteCallObject;
struct FInputKeyBinding;

UCLASS(NotBlueprintable, Transient)
class SATISFACTORYBUILDCALCULATOR_API ASBCPlayerGoalHUD final : public AActor
{
	GENERATED_BODY()

public:
	ASBCPlayerGoalHUD();
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<AFGPlayerController> PlayerController = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<AFGCharacterPlayer> PlayerCharacter = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<USBCGoalHUDWidget> GoalWidget = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<USBCCalculatorWidget> CalculatorWidget = nullptr;

	int32 SelectedGoalIndex = 0;
	bool bCalculatorToggleBound = false;
	bool bGoalsDelegateBound = false;
	bool bReleaseEscapeCapture = false;
	bool bCalculatorClosedByWidget = false;
	FInputKeyBinding* CalculatorEscapeBinding = nullptr;

	bool EnsureLocalPlayer();
	USBCRemoteCallObject* GetRemoteCallObject() const;
	AFGBuildable* GetLookedAtBuildable() const;
	void ToggleWidget();
	void ToggleCalculatorWidget();
	void CloseCalculatorWidget();
	void AddCalculatorPlanToGoals();
	void UpdateCalculatorLayout();
	void AddLookedAtGoal();
	void ChangeSelection(int32 Delta);
	void ChangeTargetCount(int32 Delta);
	void ToggleManualCompletion();
	void RemoveSelectedGoal();
	void LinkLookedAtBuildable();

	UFUNCTION()
	void HandleGoalsChanged();
};
