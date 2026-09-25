#include "SBCRemoteCallObject.h"

#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableManufacturer.h"
#include "FGCharacterPlayer.h"
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
