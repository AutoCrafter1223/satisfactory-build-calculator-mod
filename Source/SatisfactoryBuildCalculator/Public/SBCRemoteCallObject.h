#pragma once

#include "CoreMinimal.h"
#include "FGRemoteCallObject.h"
#include "SBCRemoteCallObject.generated.h"

class AFGBuildable;
class UFGRecipe;

USTRUCT()
struct SATISFACTORYBUILDCALCULATOR_API FSBCGoalBatchEntry
{
	GENERATED_BODY()

	UPROPERTY()
	TSubclassOf<UFGRecipe> RecipeClass;

	UPROPERTY()
	TSubclassOf<AFGBuildable> BuildableClass;

	UPROPERTY()
	int32 TargetCount = 1;

	UPROPERTY()
	int32 PowerShards = 0;

	UPROPERTY()
	int32 Somersloops = 0;

	UPROPERTY()
	FString HierarchyKey;

	UPROPERTY()
	FString ParentHierarchyKey;

	UPROPERTY()
	int32 HierarchyDepth = 0;

	UPROPERTY()
	int32 HierarchyOrder = 0;
};

DECLARE_MULTICAST_DELEGATE_ThreeParams(FSBCGoalBatchResult, int32, int32, const TArray<FString>&);

UCLASS(NotBlueprintable)
class SATISFACTORYBUILDCALCULATOR_API USBCRemoteCallObject final : public UFGRemoteCallObject
{
	GENERATED_BODY()

public:
	USBCRemoteCallObject();
	FSBCGoalBatchResult OnGoalBatchResult;

	UFUNCTION(BlueprintCallable, Server, Reliable)
	void ServerAddGoalFromBuildable(AFGBuildable* TemplateBuildable, int32 TargetCount);

	UFUNCTION(BlueprintCallable, Server, Reliable)
	void ServerAddGoalFromRecipe(
		TSubclassOf<UFGRecipe> RecipeClass,
		TSubclassOf<AFGBuildable> BuildableClass,
		int32 TargetCount,
		int32 PowerShards,
		int32 Somersloops,
		FGuid GoalGroupId,
		const FString& HierarchyKey,
		const FString& ParentHierarchyKey,
		int32 HierarchyDepth,
		int32 HierarchyOrder);

	UFUNCTION(BlueprintCallable, Server, Reliable)
	void ServerAddGoalFromClass(
		TSubclassOf<AFGBuildable> BuildableClass,
		int32 TargetCount,
		FGuid GoalGroupId,
		const FString& HierarchyKey,
		const FString& ParentHierarchyKey,
		int32 HierarchyDepth,
		int32 HierarchyOrder);

	// A calculator branch is validated and committed as one batch so a bad
	// recipe/building mapping can never leave a partially-created goal group.
	UFUNCTION(Server, Reliable)
	void ServerAddGoalBatch(const TArray<FSBCGoalBatchEntry>& Entries, FGuid GoalGroupId);

	UFUNCTION(Client, Reliable)
	void ClientReportGoalBatchResult(int32 ExpectedCount, int32 AddedCount, const TArray<FString>& FailedEntries);

	UFUNCTION(BlueprintCallable, Server, Reliable)
	void ServerRemoveGoal(FGuid GoalId);

	UFUNCTION(BlueprintCallable, Server, Reliable)
	void ServerClearGoals();

	UFUNCTION(BlueprintCallable, Server, Reliable)
	void ServerSetGoalTargetCount(FGuid GoalId, int32 TargetCount);

	UFUNCTION(BlueprintCallable, Server, Reliable)
	void ServerSetGoalManualCompletion(FGuid GoalId, bool bCompleted);

	UFUNCTION(BlueprintCallable, Server, Reliable)
	void ServerLinkExistingBuildable(AFGBuildable* Buildable, FGuid GoalId);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UPROPERTY(Replicated)
	bool bForceNetField = false;
};
