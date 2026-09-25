#pragma once

#include "CoreMinimal.h"
#include "Module/GameWorldModule.h"
#include "SBCGameWorldModule.generated.h"

class ASBCPlayerGoalHUD;

UCLASS()
class SATISFACTORYBUILDCALCULATOR_API USBCGameWorldModule final : public UGameWorldModule
{
	GENERATED_BODY()

public:
	USBCGameWorldModule();
	virtual void DispatchLifecycleEvent(ELifecyclePhase Phase) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<ASBCPlayerGoalHUD> PlayerGoalHUD = nullptr;
};
