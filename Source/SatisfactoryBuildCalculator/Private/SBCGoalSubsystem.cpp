#include "SBCGoalSubsystem.h"

#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableFactory.h"
#include "Buildables/FGBuildableManufacturer.h"
#include "FGCharacterPlayer.h"
#include "FGBuildableSubsystem.h"
#include "FGInventoryComponent.h"
#include "FGRecipe.h"
#include "Net/UnrealNetwork.h"
#include "Resources/FGPowerShardDescriptor.h"
#include "Subsystem/SubsystemActorManager.h"
#include "TimerManager.h"

ASBCGoalSubsystem::ASBCGoalSubsystem()
{
	ReplicationPolicy = ESubsystemReplicationPolicy::SpawnOnServer_Replicate;
	bReplicates = true;
	bAlwaysRelevant = true;
}

ASBCGoalSubsystem* ASBCGoalSubsystem::Get(UObject* WorldContext)
{
	if (!IsValid(WorldContext) || !IsValid(WorldContext->GetWorld()))
	{
		return nullptr;
	}

	if (USubsystemActorManager* Manager = WorldContext->GetWorld()->GetSubsystem<USubsystemActorManager>())
	{
		return Manager->GetSubsystemActor<ASBCGoalSubsystem>();
	}
	return nullptr;
}

void ASBCGoalSubsystem::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		BindBuildableSubsystem();
		GetWorldTimerManager().SetTimer(TrackingTimerHandle, this, &ASBCGoalSubsystem::RefreshTracking, 1.0f, true, 1.0f);
	}
}

void ASBCGoalSubsystem::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(TrackingTimerHandle);
	if (IsValid(BuildableSubsystem))
	{
		BuildableSubsystem->BuildableConstructedGlobalDelegate.RemoveDynamic(this, &ASBCGoalSubsystem::HandleBuildableConstructed);
		BuildableSubsystem->mBuildableRemovedDelegate.RemoveDynamic(this, &ASBCGoalSubsystem::HandleBuildableRemoved);
	}
	Super::EndPlay(EndPlayReason);
}

void ASBCGoalSubsystem::BindBuildableSubsystem()
{
	if (IsValid(BuildableSubsystem))
	{
		return;
	}

	BuildableSubsystem = AFGBuildableSubsystem::Get(GetWorld());
	if (IsValid(BuildableSubsystem))
	{
		BuildableSubsystem->BuildableConstructedGlobalDelegate.AddUniqueDynamic(this, &ASBCGoalSubsystem::HandleBuildableConstructed);
		BuildableSubsystem->mBuildableRemovedDelegate.AddUniqueDynamic(this, &ASBCGoalSubsystem::HandleBuildableRemoved);
	}
}

FGuid ASBCGoalSubsystem::AddGoal(
	AFGCharacterPlayer* GoalOwner,
	TSubclassOf<AFGBuildable> BuildableClass,
	TSubclassOf<UFGRecipe> RecipeClass,
	int32 TargetCount,
	int32 RequiredPowerShards,
	int32 RequiredSomersloops,
	FGuid GoalGroupId,
	const FString& HierarchyKey,
	const FString& ParentHierarchyKey,
	int32 HierarchyDepth,
	int32 HierarchyOrder)
{
	if (!HasAuthority() || !IsValid(GoalOwner) || !BuildableClass)
	{
		return FGuid();
	}

	FSBCBuildGoal& Goal = Goals.AddDefaulted_GetRef();
	Goal.GoalId = FGuid::NewGuid();
	Goal.Owner = GoalOwner->GetPlayerInfoHandle();
	Goal.BuildableClass = BuildableClass;
	Goal.RecipeClass = RecipeClass;
	Goal.TargetCount = FMath::Max(1, TargetCount);
	Goal.RequiredPowerShards = FMath::Clamp(RequiredPowerShards, 0, 3);
	Goal.RequiredSomersloops = FMath::Max(0, RequiredSomersloops);
	Goal.GoalGroupId = GoalGroupId;
	Goal.HierarchyKey = HierarchyKey;
	Goal.ParentHierarchyKey = ParentHierarchyKey;
	Goal.HierarchyDepth = FMath::Max(0, HierarchyDepth);
	Goal.HierarchyOrder = FMath::Max(0, HierarchyOrder);
	MarkGoalsChanged();
	return Goal.GoalId;
}

bool ASBCGoalSubsystem::RemoveGoal(AFGCharacterPlayer* RequestingPlayer, FGuid GoalId)
{
	const int32 GoalIndex = FindGoalIndex(GoalId);
	if (!HasAuthority() || GoalIndex == INDEX_NONE || !DoesPlayerOwnGoal(RequestingPlayer, Goals[GoalIndex]))
	{
		return false;
	}

	Goals.RemoveAt(GoalIndex);
	for (FSBCTrackedBuildable& Tracked : TrackedBuildables)
	{
		if (Tracked.AssignedGoalId == GoalId)
		{
			Tracked.AssignedGoalId.Invalidate();
			Tracked.bUserSelectedGoal = false;
		}
	}
	MarkGoalsChanged();
	return true;
}

bool ASBCGoalSubsystem::ClearGoalsForPlayer(AFGCharacterPlayer* RequestingPlayer)
{
	if (!HasAuthority() || !IsValid(RequestingPlayer))
	{
		return false;
	}

	const FPlayerInfoHandle PlayerHandle = RequestingPlayer->GetPlayerInfoHandle();
	TSet<FGuid> RemovedGoalIds;
	for (const FSBCBuildGoal& Goal : Goals)
	{
		if (Goal.Owner.IsSameAccount(PlayerHandle))
		{
			RemovedGoalIds.Add(Goal.GoalId);
		}
	}
	if (RemovedGoalIds.IsEmpty())
	{
		return false;
	}

	Goals.RemoveAll([&PlayerHandle](const FSBCBuildGoal& Goal)
	{
		return Goal.Owner.IsSameAccount(PlayerHandle);
	});
	TrackedBuildables.RemoveAll([&RemovedGoalIds](const FSBCTrackedBuildable& Tracked)
	{
		return RemovedGoalIds.Contains(Tracked.AssignedGoalId);
	});
	NewBuildableCandidates.RemoveAll([&PlayerHandle](const TObjectPtr<AFGBuildable>& Buildable)
	{
		return IsValid(Buildable) && Buildable->GetBuiltBy().IsSameAccount(PlayerHandle);
	});
	AmbiguousBuildables.RemoveAll([&PlayerHandle](const TObjectPtr<AFGBuildable>& Buildable)
	{
		return IsValid(Buildable) && Buildable->GetBuiltBy().IsSameAccount(PlayerHandle);
	});

	MarkGoalsChanged();
	return true;
}

bool ASBCGoalSubsystem::SetGoalTargetCount(AFGCharacterPlayer* RequestingPlayer, FGuid GoalId, int32 TargetCount)
{
	const int32 GoalIndex = FindGoalIndex(GoalId);
	if (!HasAuthority() || GoalIndex == INDEX_NONE || !DoesPlayerOwnGoal(RequestingPlayer, Goals[GoalIndex]))
	{
		return false;
	}

	Goals[GoalIndex].TargetCount = FMath::Max(1, TargetCount);
	MarkGoalsChanged();
	return true;
}

bool ASBCGoalSubsystem::SetGoalManualCompletion(AFGCharacterPlayer* RequestingPlayer, FGuid GoalId, bool bCompleted)
{
	const int32 GoalIndex = FindGoalIndex(GoalId);
	if (!HasAuthority() || GoalIndex == INDEX_NONE || !DoesPlayerOwnGoal(RequestingPlayer, Goals[GoalIndex]))
	{
		return false;
	}

	Goals[GoalIndex].bManuallyCompleted = bCompleted;
	MarkGoalsChanged();
	return true;
}

bool ASBCGoalSubsystem::LinkExistingBuildable(AFGCharacterPlayer* RequestingPlayer, AFGBuildable* Buildable, FGuid GoalId)
{
	return ResolveAmbiguousBuildable(RequestingPlayer, Buildable, GoalId);
}

bool ASBCGoalSubsystem::ResolveAmbiguousBuildable(AFGCharacterPlayer* RequestingPlayer, AFGBuildable* Buildable, FGuid GoalId)
{
	const int32 GoalIndex = FindGoalIndex(GoalId);
	if (!HasAuthority() || !IsValid(Buildable) || GoalIndex == INDEX_NONE || !DoesPlayerOwnGoal(RequestingPlayer, Goals[GoalIndex]))
	{
		return false;
	}

	if (!DoesBuildableMatchGoal(Buildable, Goals[GoalIndex]))
	{
		return false;
	}

	FSBCTrackedBuildable* Existing = TrackedBuildables.FindByPredicate(
		[Buildable](const FSBCTrackedBuildable& Entry) { return Entry.Buildable == Buildable; });
	if (!Existing)
	{
		Existing = &TrackedBuildables.AddDefaulted_GetRef();
		Existing->Buildable = Buildable;
	}

	Existing->AssignedGoalId = GoalId;
	Existing->bUserSelectedGoal = true;
	NewBuildableCandidates.Remove(Buildable);
	AmbiguousBuildables.Remove(Buildable);
	RecalculateGoalCounts();
	return true;
}

TArray<FSBCBuildGoal> ASBCGoalSubsystem::GetGoalsForPlayer(AFGCharacterPlayer* Player) const
{
	TArray<FSBCBuildGoal> Result;
	if (!IsValid(Player))
	{
		return Result;
	}

	const FPlayerInfoHandle PlayerHandle = Player->GetPlayerInfoHandle();
	for (const FSBCBuildGoal& Goal : Goals)
	{
		if (Goal.Owner.IsSameAccount(PlayerHandle) && !Goal.IsComplete())
		{
			Result.Add(Goal);
		}
	}
	return Result;
}

TArray<AFGBuildable*> ASBCGoalSubsystem::GetAmbiguousBuildablesForPlayer(AFGCharacterPlayer* Player) const
{
	TArray<AFGBuildable*> Result;
	if (!IsValid(Player))
	{
		return Result;
	}

	const FPlayerInfoHandle PlayerHandle = Player->GetPlayerInfoHandle();
	for (AFGBuildable* Buildable : AmbiguousBuildables)
	{
		if (IsValid(Buildable) && Buildable->GetBuiltBy().IsSameAccount(PlayerHandle))
		{
			Result.Add(Buildable);
		}
	}
	return Result;
}

void ASBCGoalSubsystem::HandleBuildableConstructed(AFGBuildable* Buildable)
{
	if (!HasAuthority() || Goals.IsEmpty() || !IsValid(Buildable) || NewBuildableCandidates.Contains(Buildable))
	{
		return;
	}

	if (!TrackedBuildables.ContainsByPredicate([Buildable](const FSBCTrackedBuildable& Entry) { return Entry.Buildable == Buildable; }))
	{
		NewBuildableCandidates.Add(Buildable);
	}
	RefreshTracking();
}

void ASBCGoalSubsystem::HandleBuildableRemoved(AFGBuildable* Buildable)
{
	if (!HasAuthority() || !Buildable)
	{
		return;
	}

	NewBuildableCandidates.Remove(Buildable);
	AmbiguousBuildables.Remove(Buildable);
	TrackedBuildables.RemoveAll([Buildable](const FSBCTrackedBuildable& Entry) { return Entry.Buildable == Buildable; });
	RecalculateGoalCounts();
}

void ASBCGoalSubsystem::RefreshTracking()
{
	if (!HasAuthority())
	{
		return;
	}
	if (Goals.IsEmpty())
	{
		NewBuildableCandidates.Reset();
		TrackedBuildables.Reset();
		AmbiguousBuildables.Reset();
		return;
	}

	BindBuildableSubsystem();
	NewBuildableCandidates.RemoveAll([](const TObjectPtr<AFGBuildable>& Buildable) { return !IsValid(Buildable); });
	TrackedBuildables.RemoveAll([](const FSBCTrackedBuildable& Entry) { return !IsValid(Entry.Buildable); });
	AmbiguousBuildables.Reset();

	for (int32 Index = NewBuildableCandidates.Num() - 1; Index >= 0; --Index)
	{
		AFGBuildable* Buildable = NewBuildableCandidates[Index];
		const TArray<int32> Matches = FindMatchingGoalIndices(Buildable);
		if (Matches.Num() == 1)
		{
			FSBCTrackedBuildable& Tracked = TrackedBuildables.AddDefaulted_GetRef();
			Tracked.Buildable = Buildable;
			Tracked.AssignedGoalId = Goals[Matches[0]].GoalId;
			NewBuildableCandidates.RemoveAtSwap(Index);
		}
		else if (Matches.Num() > 1)
		{
			AmbiguousBuildables.AddUnique(Buildable);
			OnAmbiguousBuildableFound.Broadcast(Buildable);
		}
	}

	for (FSBCTrackedBuildable& Tracked : TrackedBuildables)
	{
		RefreshTrackedBuildable(Tracked);
	}
	RecalculateGoalCounts();
}

void ASBCGoalSubsystem::RefreshTrackedBuildable(FSBCTrackedBuildable& Tracked)
{
	if (!IsValid(Tracked.Buildable))
	{
		return;
	}

	const TArray<int32> Matches = FindMatchingGoalIndices(Tracked.Buildable);
	if (Matches.IsEmpty())
	{
		Tracked.AssignedGoalId.Invalidate();
		Tracked.bUserSelectedGoal = false;
		return;
	}

	if (Tracked.bUserSelectedGoal)
	{
		const bool bSelectedGoalStillMatches = Matches.ContainsByPredicate(
			[this, &Tracked](int32 Index) { return Goals[Index].GoalId == Tracked.AssignedGoalId; });
		if (bSelectedGoalStillMatches)
		{
			return;
		}
		Tracked.bUserSelectedGoal = false;
	}

	if (Matches.Num() == 1)
	{
		Tracked.AssignedGoalId = Goals[Matches[0]].GoalId;
		return;
	}

	Tracked.AssignedGoalId.Invalidate();
	AmbiguousBuildables.AddUnique(Tracked.Buildable);
	OnAmbiguousBuildableFound.Broadcast(Tracked.Buildable);
}

void ASBCGoalSubsystem::RecalculateGoalCounts()
{
	TMap<FGuid, int32> Counts;
	for (const FSBCTrackedBuildable& Tracked : TrackedBuildables)
	{
		if (IsValid(Tracked.Buildable) && Tracked.AssignedGoalId.IsValid())
		{
			Counts.FindOrAdd(Tracked.AssignedGoalId)++;
		}
	}

	bool bChanged = false;
	for (FSBCBuildGoal& Goal : Goals)
	{
		const int32 NewCount = Counts.FindRef(Goal.GoalId);
		if (Goal.CompletedCount != NewCount)
		{
			Goal.CompletedCount = NewCount;
			bChanged = true;
		}
	}

	if (bChanged)
	{
		MarkGoalsChanged();
	}
}

TArray<int32> ASBCGoalSubsystem::FindMatchingGoalIndices(AFGBuildable* Buildable) const
{
	TArray<int32> Matches;
	if (!IsValid(Buildable))
	{
		return Matches;
	}

	const FPlayerInfoHandle BuiltBy = Buildable->GetBuiltBy();
	for (int32 Index = 0; Index < Goals.Num(); ++Index)
	{
		if (Goals[Index].Owner.IsSameAccount(BuiltBy) && DoesBuildableMatchGoal(Buildable, Goals[Index]))
		{
			Matches.Add(Index);
		}
	}
	return Matches;
}

int32 ASBCGoalSubsystem::FindGoalIndex(FGuid GoalId) const
{
	return Goals.IndexOfByPredicate([GoalId](const FSBCBuildGoal& Goal) { return Goal.GoalId == GoalId; });
}

bool ASBCGoalSubsystem::DoesPlayerOwnGoal(const AFGCharacterPlayer* Player, const FSBCBuildGoal& Goal) const
{
	return IsValid(Player) && Goal.Owner.IsSameAccount(Player->GetPlayerInfoHandle());
}

bool ASBCGoalSubsystem::DoesBuildableMatchGoal(AFGBuildable* Buildable, const FSBCBuildGoal& Goal) const
{
	if (!IsValid(Buildable) || !Goal.BuildableClass || !Buildable->IsA(Goal.BuildableClass))
	{
		return false;
	}

	if (Goal.RecipeClass)
	{
		const AFGBuildableManufacturer* Manufacturer = Cast<AFGBuildableManufacturer>(Buildable);
		if (!Manufacturer || Manufacturer->GetCurrentRecipe() != Goal.RecipeClass)
		{
			return false;
		}
	}

	int32 PowerShards = 0;
	int32 Somersloops = 0;
	GetInstalledEnhancementCounts(Buildable, PowerShards, Somersloops);
	return PowerShards == Goal.RequiredPowerShards && Somersloops == Goal.RequiredSomersloops;
}

void ASBCGoalSubsystem::GetInstalledEnhancementCounts(AFGBuildable* Buildable, int32& OutPowerShards, int32& OutSomersloops)
{
	OutPowerShards = 0;
	OutSomersloops = 0;

	const AFGBuildableFactory* Factory = Cast<AFGBuildableFactory>(Buildable);
	const UFGInventoryComponent* Inventory = Factory ? Factory->GetPotentialInventory() : nullptr;
	if (!Inventory)
	{
		return;
	}

	for (int32 SlotIndex = 0; SlotIndex < Inventory->GetSizeLinear(); ++SlotIndex)
	{
		FInventoryStack Stack;
		if (!Inventory->GetStackFromIndex(SlotIndex, Stack) || !Stack.HasItems())
		{
			continue;
		}

		UClass* ItemClass = Stack.Item.GetItemClass();
		if (!ItemClass || !ItemClass->IsChildOf(UFGPowerShardDescriptor::StaticClass()))
		{
			continue;
		}

		const EPowerShardType ShardType = UFGPowerShardDescriptor::GetPowerShardType(
			TSubclassOf<UFGPowerShardDescriptor>(ItemClass));
		if (ShardType == EPowerShardType::PST_Overclock)
		{
			OutPowerShards += Stack.NumItems;
		}
		else if (ShardType == EPowerShardType::PST_ProductionBoost)
		{
			OutSomersloops += Stack.NumItems;
		}
	}
}

void ASBCGoalSubsystem::MarkGoalsChanged()
{
	ForceNetUpdate();
	OnGoalsChanged.Broadcast();
}

void ASBCGoalSubsystem::OnRep_Goals()
{
	OnGoalsChanged.Broadcast();
}

void ASBCGoalSubsystem::PostLoadGame_Implementation(int32 SaveVersion, int32 GameVersion)
{
	// Discard any goal state that may have been serialized by versions up to
	// 1.0.1. From this version onward all calculator goals are volatile.
	Goals.Reset();
	TrackedBuildables.Reset();
	NewBuildableCandidates.Reset();
	AmbiguousBuildables.Reset();
	MarkGoalsChanged();
}

void ASBCGoalSubsystem::GatherDependencies_Implementation(TArray<UObject*>& OutDependentObjects)
{
	// Volatile tracking never adds buildables to the save dependency graph.
}

void ASBCGoalSubsystem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ASBCGoalSubsystem, Goals);
}
