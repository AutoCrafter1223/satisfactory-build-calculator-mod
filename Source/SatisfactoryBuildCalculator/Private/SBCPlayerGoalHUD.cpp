#include "SBCPlayerGoalHUD.h"

#include "Buildables/FGBuildable.h"
#include "Engine/World.h"
#include "FGCharacterPlayer.h"
#include "FGPlayerController.h"
#include "InputCoreTypes.h"
#include "SBCCalculatorWidget.h"
#include "SBCGoalHUDWidget.h"
#include "SBCGoalSubsystem.h"
#include "SBCRemoteCallObject.h"

ASBCPlayerGoalHUD::ASBCPlayerGoalHUD()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;
	bReplicates = false;
}

bool ASBCPlayerGoalHUD::EnsureLocalPlayer()
{
	if (!IsValid(PlayerController))
	{
		PlayerController = GetWorld() ? GetWorld()->GetFirstPlayerController<AFGPlayerController>() : nullptr;
	}
	if (!IsValid(PlayerController) || !PlayerController->IsLocalController())
	{
		return false;
	}

	PlayerCharacter = Cast<AFGCharacterPlayer>(PlayerController->GetPawn());
	if (!IsValid(PlayerCharacter))
	{
		return false;
	}

	if (!IsValid(GoalWidget))
	{
		GoalWidget = CreateWidget<USBCGoalHUDWidget>(PlayerController, USBCGoalHUDWidget::StaticClass());
		if (GoalWidget)
		{
			GoalWidget->SetObservedPlayer(PlayerCharacter);
			GoalWidget->SetPositionInViewport(FVector2D(28.0f, 190.0f), false);
			GoalWidget->AddToViewport(50);
		}
	}
	if (!IsValid(CalculatorWidget))
	{
		CalculatorWidget = CreateWidget<USBCCalculatorWidget>(PlayerController, USBCCalculatorWidget::StaticClass());
		if (CalculatorWidget)
		{
			CalculatorWidget->SetAnchorsInViewport(FAnchors(0.5f, 0.5f));
			CalculatorWidget->SetAlignmentInViewport(FVector2D(0.5f, 0.5f));
			CalculatorWidget->SetPositionInViewport(FVector2D::ZeroVector, false);
			CalculatorWidget->AddToViewport(100);
			CalculatorWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	return IsValid(GoalWidget);
}

void ASBCPlayerGoalHUD::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!EnsureLocalPlayer())
	{
		return;
	}

	if (PlayerController->WasInputKeyJustPressed(EKeys::F8)) ToggleWidget();
	if (PlayerController->WasInputKeyJustPressed(EKeys::F6)) ToggleCalculatorWidget();
	if (PlayerController->WasInputKeyJustPressed(EKeys::F7)) AddLookedAtGoal();
	if (PlayerController->WasInputKeyJustPressed(EKeys::Up)) ChangeSelection(-1);
	if (PlayerController->WasInputKeyJustPressed(EKeys::Down)) ChangeSelection(1);
	if (PlayerController->WasInputKeyJustPressed(EKeys::Add) || PlayerController->WasInputKeyJustPressed(EKeys::Equals)) ChangeTargetCount(1);
	if (PlayerController->WasInputKeyJustPressed(EKeys::Subtract) || PlayerController->WasInputKeyJustPressed(EKeys::Hyphen)) ChangeTargetCount(-1);
	if (PlayerController->WasInputKeyJustPressed(EKeys::Enter)) ToggleManualCompletion();
	if (PlayerController->WasInputKeyJustPressed(EKeys::Delete)) RemoveSelectedGoal();
	if (PlayerController->WasInputKeyJustPressed(EKeys::L)) LinkLookedAtBuildable();

	RefreshAccumulator += DeltaSeconds;
	if (RefreshAccumulator >= 0.25f)
	{
		RefreshAccumulator = 0.0f;
		GoalWidget->RefreshGoals();
	}
}

void ASBCPlayerGoalHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(GoalWidget))
	{
		GoalWidget->RemoveFromParent();
	}
	if (IsValid(CalculatorWidget))
	{
		CalculatorWidget->RemoveFromParent();
	}
	Super::EndPlay(EndPlayReason);
}

USBCRemoteCallObject* ASBCPlayerGoalHUD::GetRemoteCallObject() const
{
	return PlayerController ? PlayerController->GetRemoteCallObjectOfClass<USBCRemoteCallObject>() : nullptr;
}

AFGBuildable* ASBCPlayerGoalHUD::GetLookedAtBuildable() const
{
	if (!PlayerController || !GetWorld())
	{
		return nullptr;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SBCGoalTrace), true, PlayerCharacter);
	GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, ViewLocation + ViewRotation.Vector() * 20000.0f, ECC_Visibility, Params);
	return Cast<AFGBuildable>(Hit.GetActor());
}

void ASBCPlayerGoalHUD::ToggleWidget()
{
	if (GoalWidget)
	{
		GoalWidget->SetVisibility(GoalWidget->IsVisible() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

void ASBCPlayerGoalHUD::ToggleCalculatorWidget()
{
	if (!CalculatorWidget || !PlayerController) return;
	const bool bOpen = CalculatorWidget->GetVisibility() == ESlateVisibility::Collapsed;
	CalculatorWidget->SetVisibility(bOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	PlayerController->SetShowMouseCursor(bOpen);
	if (bOpen)
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetWidgetToFocus(CalculatorWidget->TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PlayerController->SetInputMode(InputMode);
		CalculatorWidget->FocusSearchBox();
	}
	else
	{
		PlayerController->SetInputMode(FInputModeGameOnly());
	}
}

void ASBCPlayerGoalHUD::AddLookedAtGoal()
{
	if (USBCRemoteCallObject* RCO = GetRemoteCallObject())
	{
		if (AFGBuildable* Buildable = GetLookedAtBuildable())
		{
			RCO->ServerAddGoalFromBuildable(Buildable, 1);
		}
	}
}

void ASBCPlayerGoalHUD::ChangeSelection(int32 Delta)
{
	ASBCGoalSubsystem* Goals = ASBCGoalSubsystem::Get(this);
	const int32 GoalCount = Goals ? Goals->GetGoalsForPlayer(PlayerCharacter).Num() : 0;
	if (GoalCount > 0)
	{
		SelectedGoalIndex = (SelectedGoalIndex + Delta + GoalCount) % GoalCount;
		GoalWidget->SetSelectedGoalIndex(SelectedGoalIndex);
	}
}

void ASBCPlayerGoalHUD::ChangeTargetCount(int32 Delta)
{
	ASBCGoalSubsystem* Subsystem = ASBCGoalSubsystem::Get(this);
	const TArray<FSBCBuildGoal> Goals = Subsystem ? Subsystem->GetGoalsForPlayer(PlayerCharacter) : TArray<FSBCBuildGoal>();
	if (Goals.IsValidIndex(SelectedGoalIndex))
	{
		if (USBCRemoteCallObject* RCO = GetRemoteCallObject())
		{
			RCO->ServerSetGoalTargetCount(Goals[SelectedGoalIndex].GoalId, Goals[SelectedGoalIndex].TargetCount + Delta);
		}
	}
}

void ASBCPlayerGoalHUD::ToggleManualCompletion()
{
	ASBCGoalSubsystem* Subsystem = ASBCGoalSubsystem::Get(this);
	const TArray<FSBCBuildGoal> Goals = Subsystem ? Subsystem->GetGoalsForPlayer(PlayerCharacter) : TArray<FSBCBuildGoal>();
	if (Goals.IsValidIndex(SelectedGoalIndex))
	{
		if (USBCRemoteCallObject* RCO = GetRemoteCallObject())
		{
			RCO->ServerSetGoalManualCompletion(Goals[SelectedGoalIndex].GoalId, !Goals[SelectedGoalIndex].bManuallyCompleted);
		}
	}
}

void ASBCPlayerGoalHUD::RemoveSelectedGoal()
{
	ASBCGoalSubsystem* Subsystem = ASBCGoalSubsystem::Get(this);
	const TArray<FSBCBuildGoal> Goals = Subsystem ? Subsystem->GetGoalsForPlayer(PlayerCharacter) : TArray<FSBCBuildGoal>();
	if (Goals.IsValidIndex(SelectedGoalIndex))
	{
		if (USBCRemoteCallObject* RCO = GetRemoteCallObject())
		{
			RCO->ServerRemoveGoal(Goals[SelectedGoalIndex].GoalId);
			SelectedGoalIndex = FMath::Max(0, SelectedGoalIndex - 1);
			GoalWidget->SetSelectedGoalIndex(SelectedGoalIndex);
		}
	}
}

void ASBCPlayerGoalHUD::LinkLookedAtBuildable()
{
	ASBCGoalSubsystem* Subsystem = ASBCGoalSubsystem::Get(this);
	const TArray<FSBCBuildGoal> Goals = Subsystem ? Subsystem->GetGoalsForPlayer(PlayerCharacter) : TArray<FSBCBuildGoal>();
	if (Goals.IsValidIndex(SelectedGoalIndex))
	{
		if (USBCRemoteCallObject* RCO = GetRemoteCallObject())
		{
			if (AFGBuildable* Buildable = GetLookedAtBuildable())
			{
				RCO->ServerLinkExistingBuildable(Buildable, Goals[SelectedGoalIndex].GoalId);
			}
		}
	}
}
