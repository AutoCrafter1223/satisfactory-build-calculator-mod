#pragma once

#include "CoreMinimal.h"
#include "Online/PlayerInfoCache.h"
#include "SBCGoalTypes.generated.h"

class AFGBuildable;
class UFGRecipe;

USTRUCT(BlueprintType)
struct SATISFACTORYBUILDCALCULATOR_API FSBCBuildGoal
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, SaveGame)
	FGuid GoalId;

	UPROPERTY(BlueprintReadOnly, SaveGame)
	FPlayerInfoHandle Owner;

	UPROPERTY(BlueprintReadOnly, SaveGame)
	TSubclassOf<AFGBuildable> BuildableClass;

	UPROPERTY(BlueprintReadOnly, SaveGame)
	TSubclassOf<UFGRecipe> RecipeClass;

	UPROPERTY(BlueprintReadOnly, SaveGame)
	int32 RequiredPowerShards = 0;

	UPROPERTY(BlueprintReadOnly, SaveGame)
	int32 RequiredSomersloops = 0;

	UPROPERTY(BlueprintReadOnly, SaveGame)
	int32 TargetCount = 1;

	UPROPERTY(BlueprintReadOnly, SaveGame)
	int32 CompletedCount = 0;

	UPROPERTY(BlueprintReadOnly, SaveGame)
	bool bManuallyCompleted = false;

	bool IsComplete() const { return bManuallyCompleted || CompletedCount >= TargetCount; }
};

USTRUCT()
struct SATISFACTORYBUILDCALCULATOR_API FSBCTrackedBuildable
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	TObjectPtr<AFGBuildable> Buildable = nullptr;

	UPROPERTY(SaveGame)
	FGuid AssignedGoalId;

	UPROPERTY(SaveGame)
	bool bUserSelectedGoal = false;
};
