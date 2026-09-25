#pragma once

#include "CoreMinimal.h"
#include "FGSaveInterface.h"
#include "SBCGoalTypes.h"
#include "Subsystem/ModSubsystem.h"
#include "SBCGoalSubsystem.generated.h"

class AFGBuildable;
class AFGBuildableSubsystem;
class AFGCharacterPlayer;
class UFGRecipe;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSBCGoalsChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSBCAmbiguousBuildableFound, AFGBuildable*, Buildable);

UCLASS(BlueprintType)
class SATISFACTORYBUILDCALCULATOR_API ASBCGoalSubsystem final : public AModSubsystem, public IFGSaveInterface
{
	GENERATED_BODY()

public:
	ASBCGoalSubsystem();

	static ASBCGoalSubsystem* Get(UObject* WorldContext);

	UFUNCTION(BlueprintCallable, Category = "Satisfactory Build Calculator|Goals")
	FGuid AddGoal(
		AFGCharacterPlayer* GoalOwner,
		TSubclassOf<AFGBuildable> BuildableClass,
		TSubclassOf<UFGRecipe> RecipeClass,
		int32 TargetCount,
		int32 RequiredPowerShards,
		int32 RequiredSomersloops);

	UFUNCTION(BlueprintCallable, Category = "Satisfactory Build Calculator|Goals")
	bool RemoveGoal(AFGCharacterPlayer* RequestingPlayer, FGuid GoalId);

	UFUNCTION(BlueprintCallable, Category = "Satisfactory Build Calculator|Goals")
	bool ClearGoalsForPlayer(AFGCharacterPlayer* RequestingPlayer);

	UFUNCTION(BlueprintCallable, Category = "Satisfactory Build Calculator|Goals")
	bool SetGoalTargetCount(AFGCharacterPlayer* RequestingPlayer, FGuid GoalId, int32 TargetCount);

	UFUNCTION(BlueprintCallable, Category = "Satisfactory Build Calculator|Goals")
	bool SetGoalManualCompletion(AFGCharacterPlayer* RequestingPlayer, FGuid GoalId, bool bCompleted);

	UFUNCTION(BlueprintCallable, Category = "Satisfactory Build Calculator|Goals")
	bool LinkExistingBuildable(AFGCharacterPlayer* RequestingPlayer, AFGBuildable* Buildable, FGuid GoalId);

	UFUNCTION(BlueprintCallable, Category = "Satisfactory Build Calculator|Goals")
	bool ResolveAmbiguousBuildable(AFGCharacterPlayer* RequestingPlayer, AFGBuildable* Buildable, FGuid GoalId);

	UFUNCTION(BlueprintPure, Category = "Satisfactory Build Calculator|Goals")
	TArray<FSBCBuildGoal> GetGoalsForPlayer(AFGCharacterPlayer* Player) const;

	UFUNCTION(BlueprintPure, Category = "Satisfactory Build Calculator|Goals")
	TArray<AFGBuildable*> GetAmbiguousBuildablesForPlayer(AFGCharacterPlayer* Player) const;

	static void GetInstalledEnhancementCounts(AFGBuildable* Buildable, int32& OutPowerShards, int32& OutSomersloops);

	UPROPERTY(BlueprintAssignable, Category = "Satisfactory Build Calculator|Goals")
	FSBCGoalsChanged OnGoalsChanged;

	UPROPERTY(BlueprintAssignable, Category = "Satisfactory Build Calculator|Goals")
	FSBCAmbiguousBuildableFound OnAmbiguousBuildableFound;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void PreSaveGame_Implementation(int32 SaveVersion, int32 GameVersion) override {}
	virtual void PostSaveGame_Implementation(int32 SaveVersion, int32 GameVersion) override {}
	virtual void PreLoadGame_Implementation(int32 SaveVersion, int32 GameVersion) override {}
	virtual void PostLoadGame_Implementation(int32 SaveVersion, int32 GameVersion) override;
	virtual void GatherDependencies_Implementation(TArray<UObject*>& OutDependentObjects) override;
	virtual bool NeedTransform_Implementation() override { return false; }
	virtual bool ShouldSave_Implementation() const override { return true; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(SaveGame, ReplicatedUsing = OnRep_Goals)
	TArray<FSBCBuildGoal> Goals;

	UPROPERTY(SaveGame)
	TArray<FSBCTrackedBuildable> TrackedBuildables;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AFGBuildable>> NewBuildableCandidates;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AFGBuildable>> AmbiguousBuildables;

	UPROPERTY(Transient)
	TObjectPtr<AFGBuildableSubsystem> BuildableSubsystem = nullptr;

	FTimerHandle TrackingTimerHandle;

	UFUNCTION()
	void OnRep_Goals();

	UFUNCTION()
	void HandleBuildableConstructed(AFGBuildable* Buildable);

	UFUNCTION()
	void HandleBuildableRemoved(AFGBuildable* Buildable);

	void BindBuildableSubsystem();
	void RefreshTracking();
	void RefreshTrackedBuildable(FSBCTrackedBuildable& Tracked);
	void RecalculateGoalCounts();
	void MarkGoalsChanged();

	TArray<int32> FindMatchingGoalIndices(AFGBuildable* Buildable) const;
	int32 FindGoalIndex(FGuid GoalId) const;
	bool DoesPlayerOwnGoal(const AFGCharacterPlayer* Player, const FSBCBuildGoal& Goal) const;
	bool DoesBuildableMatchGoal(AFGBuildable* Buildable, const FSBCBuildGoal& Goal) const;
};
