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

	UPROPERTY(BlueprintReadOnly)
	FGuid GoalId;

	UPROPERTY(BlueprintReadOnly)
	FPlayerInfoHandle Owner;

	UPROPERTY(BlueprintReadOnly)
	TSubclassOf<AFGBuildable> BuildableClass;

	UPROPERTY(BlueprintReadOnly)
	TSubclassOf<UFGRecipe> RecipeClass;

	UPROPERTY(BlueprintReadOnly)
	int32 RequiredPowerShards = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 RequiredSomersloops = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 TargetCount = 1;

	UPROPERTY(BlueprintReadOnly)
	int32 CompletedCount = 0;

	UPROPERTY(BlueprintReadOnly)
	bool bManuallyCompleted = false;

	// Hierarchy metadata is intentionally separate from GoalId so calculator
	// branches can be reconstructed across multiplayer RPCs. Goal state is
	// session-only and is never written to the player's save file.
	UPROPERTY(BlueprintReadOnly)
	FGuid GoalGroupId;

	UPROPERTY(BlueprintReadOnly)
	FString HierarchyKey;

	UPROPERTY(BlueprintReadOnly)
	FString ParentHierarchyKey;

	UPROPERTY(BlueprintReadOnly)
	int32 HierarchyDepth = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 HierarchyOrder = 0;

	bool IsComplete() const { return bManuallyCompleted || CompletedCount >= TargetCount; }
};

USTRUCT()
struct SATISFACTORYBUILDCALCULATOR_API FSBCTrackedBuildable
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<AFGBuildable> Buildable = nullptr;

	UPROPERTY()
	FGuid AssignedGoalId;

	UPROPERTY()
	bool bUserSelectedGoal = false;
};
