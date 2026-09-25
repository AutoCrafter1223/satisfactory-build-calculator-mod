#pragma once

#include "CoreMinimal.h"
#include "FGRemoteCallObject.h"
#include "SBCRemoteCallObject.generated.h"

class AFGBuildable;

UCLASS(NotBlueprintable)
class SATISFACTORYBUILDCALCULATOR_API USBCRemoteCallObject final : public UFGRemoteCallObject
{
	GENERATED_BODY()

public:
	USBCRemoteCallObject();

	UFUNCTION(BlueprintCallable, Server, Reliable)
	void ServerAddGoalFromBuildable(AFGBuildable* TemplateBuildable, int32 TargetCount);

	UFUNCTION(BlueprintCallable, Server, Reliable)
	void ServerRemoveGoal(FGuid GoalId);

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
