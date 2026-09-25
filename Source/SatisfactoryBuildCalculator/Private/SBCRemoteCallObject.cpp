#include "SBCRemoteCallObject.h"

#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableManufacturer.h"
#include "FGCharacterPlayer.h"
#include "FGRecipe.h"
#include "Net/UnrealNetwork.h"
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
	Goals->AddGoal(Player, TemplateBuildable->GetClass(), ProductionRecipe, TargetCount, PowerShards, Somersloops);
}

void USBCRemoteCallObject::ServerAddGoalFromRecipe_Implementation(
	TSubclassOf<UFGRecipe> RecipeClass,
	int32 TargetCount,
	int32 PowerShards,
	int32 Somersloops)
{
	AFGCharacterPlayer* Player = GetOwnerPlayerCharacter();
	ASBCGoalSubsystem* Goals = ASBCGoalSubsystem::Get(this);
	if (!IsValid(Player) || !RecipeClass || !IsValid(Goals)) return;

	TSubclassOf<AFGBuildable> BuildableClass;
	for (const TSubclassOf<UObject>& Producer : UFGRecipe::GetProducedIn(RecipeClass))
	{
		if (Producer && Producer->IsChildOf(AFGBuildable::StaticClass()))
		{
			BuildableClass = TSubclassOf<AFGBuildable>(Producer.Get());
			break;
		}
	}
	if (BuildableClass)
	{
		Goals->AddGoal(Player, BuildableClass, RecipeClass, TargetCount, PowerShards, Somersloops);
	}
}

void USBCRemoteCallObject::ServerAddGoalFromClass_Implementation(TSubclassOf<AFGBuildable> BuildableClass, int32 TargetCount)
{
	AFGCharacterPlayer* Player = GetOwnerPlayerCharacter();
	ASBCGoalSubsystem* Goals = ASBCGoalSubsystem::Get(this);
	if (IsValid(Player) && BuildableClass && IsValid(Goals))
	{
		Goals->AddGoal(Player, BuildableClass, nullptr, TargetCount, 0, 0);
	}
}

void USBCRemoteCallObject::ServerRemoveGoal_Implementation(FGuid GoalId)
{
	if (ASBCGoalSubsystem* Goals = ASBCGoalSubsystem::Get(this))
	{
		Goals->RemoveGoal(GetOwnerPlayerCharacter(), GoalId);
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
