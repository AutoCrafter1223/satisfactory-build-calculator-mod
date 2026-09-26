#include "SBCRemoteCallObject.h"

#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableManufacturer.h"
#include "FGCharacterPlayer.h"
#include "FGRecipe.h"
#include "Net/UnrealNetwork.h"
#include "SatisfactoryBuildCalculator.h"
#include "SBCGoalSubsystem.h"

USBCRemoteCallObject::USBCRemoteCallObject() = default;

void USBCRemoteCallObject::ServerAddGoalFromBuildable_Implementation(AFGBuildable* TemplateBuildable, int32 TargetCount)
{
	AFGCharacterPlayer* Player = GetOwnerPlayerCharacter();
	ASBCGoalSubsystem* Goals = ASBCGoalSubsystem::Get(this);
	if (!IsValid(Player) || !IsValid(TemplateBuildable) || !IsValid(Goals))
	{
		return;
	}

	TSubclassOf<UFGRecipe> ProductionRecipe;
	if (const AFGBuildableManufacturer* Manufacturer = Cast<AFGBuildableManufacturer>(TemplateBuildable))
	{
		ProductionRecipe = Manufacturer->GetCurrentRecipe();
	}

	int32 PowerShards = 0;
	int32 Somersloops = 0;
	ASBCGoalSubsystem::GetInstalledEnhancementCounts(TemplateBuildable, PowerShards, Somersloops);
	Goals->AddGoal(Player, TemplateBuildable->GetClass(), ProductionRecipe, TargetCount, PowerShards, Somersloops,
		FGuid(), FString(), FString(), 0, 0);
}

void USBCRemoteCallObject::ServerAddGoalFromRecipe_Implementation(
	TSubclassOf<UFGRecipe> RecipeClass,
	TSubclassOf<AFGBuildable> BuildableClass,
	int32 TargetCount,
	int32 PowerShards,
	int32 Somersloops,
	FGuid GoalGroupId,
	const FString& HierarchyKey,
	const FString& ParentHierarchyKey,
	int32 HierarchyDepth,
	int32 HierarchyOrder)
{
	AFGCharacterPlayer* Player = GetOwnerPlayerCharacter();
	ASBCGoalSubsystem* Goals = ASBCGoalSubsystem::Get(this);
	if (!IsValid(Player) || !RecipeClass || !BuildableClass || !IsValid(Goals)) return;

	bool bRecipeSupportsBuilding = false;
	for (const TSubclassOf<UObject>& Producer : UFGRecipe::GetProducedIn(RecipeClass))
	{
		if (Producer == BuildableClass)
		{
			bRecipeSupportsBuilding = true;
			break;
		}
	}
	if (bRecipeSupportsBuilding)
	{
		Goals->AddGoal(Player, BuildableClass, RecipeClass, TargetCount, PowerShards, Somersloops,
			GoalGroupId, HierarchyKey, ParentHierarchyKey, HierarchyDepth, HierarchyOrder);
	}
}

void USBCRemoteCallObject::ServerAddGoalFromClass_Implementation(
	TSubclassOf<AFGBuildable> BuildableClass,
	int32 TargetCount,
	FGuid GoalGroupId,
	const FString& HierarchyKey,
	const FString& ParentHierarchyKey,
	int32 HierarchyDepth,
	int32 HierarchyOrder)
{
	AFGCharacterPlayer* Player = GetOwnerPlayerCharacter();
	ASBCGoalSubsystem* Goals = ASBCGoalSubsystem::Get(this);
	if (IsValid(Player) && BuildableClass && IsValid(Goals))
	{
		Goals->AddGoal(Player, BuildableClass, nullptr, TargetCount, 0, 0,
			GoalGroupId, HierarchyKey, ParentHierarchyKey, HierarchyDepth, HierarchyOrder);
	}
}

void USBCRemoteCallObject::ServerAddGoalBatch_Implementation(const TArray<FSBCGoalBatchEntry>& Entries, FGuid GoalGroupId)
{
	AFGCharacterPlayer* Player = GetOwnerPlayerCharacter();
	ASBCGoalSubsystem* Goals = ASBCGoalSubsystem::Get(this);
	TArray<FString> Failures;

	if (!IsValid(Player) || !IsValid(Goals) || Entries.IsEmpty())
	{
		Failures.Add(TEXT("Goal subsystem or player is unavailable"));
		ClientReportGoalBatchResult(Entries.Num(), 0, Failures);
		return;
	}

	// Validate every entry before changing save-backed state. This makes the
	// operation all-or-nothing and prevents a silent partial branch import.
	for (const FSBCGoalBatchEntry& Entry : Entries)
	{
		const FString Label = Entry.RecipeClass
			? Entry.RecipeClass->GetName()
			: (Entry.BuildableClass ? Entry.BuildableClass->GetName() : TEXT("Unknown goal"));
		if (!Entry.BuildableClass || Entry.TargetCount <= 0 || Entry.HierarchyKey.IsEmpty())
		{
			Failures.Add(Label);
			continue;
		}
		if (Entry.RecipeClass)
		{
			const bool bSupported = UFGRecipe::GetProducedIn(Entry.RecipeClass).ContainsByPredicate(
				[&Entry](const TSubclassOf<UObject>& Producer) { return Producer == Entry.BuildableClass; });
			if (!bSupported)
			{
				Failures.Add(Label);
			}
		}
	}

	if (!Failures.IsEmpty())
	{
		UE_LOG(LogSatisfactoryBuildCalculator, Warning,
			TEXT("Rejected calculator goal batch before commit: %d expected, %d invalid"),
			Entries.Num(), Failures.Num());
		ClientReportGoalBatchResult(Entries.Num(), 0, Failures);
		return;
	}

	TArray<FGuid> AddedGoalIds;
	for (const FSBCGoalBatchEntry& Entry : Entries)
	{
		const FGuid AddedId = Goals->AddGoal(
			Player,
			Entry.BuildableClass,
			Entry.RecipeClass,
			Entry.TargetCount,
			Entry.PowerShards,
			Entry.Somersloops,
			GoalGroupId,
			Entry.HierarchyKey,
			Entry.ParentHierarchyKey,
			Entry.HierarchyDepth,
			Entry.HierarchyOrder);
		if (AddedId.IsValid())
		{
			AddedGoalIds.Add(AddedId);
		}
		else
		{
			Failures.Add(Entry.RecipeClass ? Entry.RecipeClass->GetName() : Entry.BuildableClass->GetName());
			break;
		}
	}

	if (AddedGoalIds.Num() != Entries.Num())
	{
		// Roll back the entire batch if an unexpected server-side failure occurs.
		for (const FGuid& GoalId : AddedGoalIds)
		{
			Goals->RemoveGoal(Player, GoalId);
		}
		UE_LOG(LogSatisfactoryBuildCalculator, Error,
			TEXT("Rolled back calculator goal batch: %d expected, %d created before failure"),
			Entries.Num(), AddedGoalIds.Num());
		ClientReportGoalBatchResult(Entries.Num(), 0, Failures);
		return;
	}

	UE_LOG(LogSatisfactoryBuildCalculator, Display,
		TEXT("Created complete calculator goal batch: %d/%d"), AddedGoalIds.Num(), Entries.Num());
	ClientReportGoalBatchResult(Entries.Num(), AddedGoalIds.Num(), Failures);
}

void USBCRemoteCallObject::ClientReportGoalBatchResult_Implementation(
	int32 ExpectedCount,
	int32 AddedCount,
	const TArray<FString>& FailedEntries)
{
	OnGoalBatchResult.Broadcast(ExpectedCount, AddedCount, FailedEntries);
}

void USBCRemoteCallObject::ServerRemoveGoal_Implementation(FGuid GoalId)
{
	if (ASBCGoalSubsystem* Goals = ASBCGoalSubsystem::Get(this))
	{
		Goals->RemoveGoal(GetOwnerPlayerCharacter(), GoalId);
	}
}

void USBCRemoteCallObject::ServerClearGoals_Implementation()
{
	if (ASBCGoalSubsystem* Goals = ASBCGoalSubsystem::Get(this))
	{
		Goals->ClearGoalsForPlayer(GetOwnerPlayerCharacter());
	}
}

void USBCRemoteCallObject::ServerSetGoalTargetCount_Implementation(FGuid GoalId, int32 TargetCount)
{
	if (ASBCGoalSubsystem* Goals = ASBCGoalSubsystem::Get(this))
	{
		Goals->SetGoalTargetCount(GetOwnerPlayerCharacter(), GoalId, TargetCount);
	}
}

void USBCRemoteCallObject::ServerSetGoalManualCompletion_Implementation(FGuid GoalId, bool bCompleted)
{
	if (ASBCGoalSubsystem* Goals = ASBCGoalSubsystem::Get(this))
	{
		Goals->SetGoalManualCompletion(GetOwnerPlayerCharacter(), GoalId, bCompleted);
	}
}

void USBCRemoteCallObject::ServerLinkExistingBuildable_Implementation(AFGBuildable* Buildable, FGuid GoalId)
{
	if (ASBCGoalSubsystem* Goals = ASBCGoalSubsystem::Get(this))
	{
		Goals->LinkExistingBuildable(GetOwnerPlayerCharacter(), Buildable, GoalId);
	}
}

void USBCRemoteCallObject::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(USBCRemoteCallObject, bForceNetField);
}
