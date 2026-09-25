#include "SBCGameWorldModule.h"

#include "SBCGoalSubsystem.h"
#include "SBCPlayerGoalHUD.h"

USBCGameWorldModule::USBCGameWorldModule()
{
	bRootModule = true;
	ModSubsystems.Add(ASBCGoalSubsystem::StaticClass());
}

void USBCGameWorldModule::DispatchLifecycleEvent(ELifecyclePhase Phase)
{
	Super::DispatchLifecycleEvent(Phase);

	if (Phase == ELifecyclePhase::POST_INITIALIZATION && GetWorld() && GetWorld()->GetNetMode() != NM_DedicatedServer)
	{
		PlayerGoalHUD = GetWorld()->SpawnActor<ASBCPlayerGoalHUD>();
	}
}
