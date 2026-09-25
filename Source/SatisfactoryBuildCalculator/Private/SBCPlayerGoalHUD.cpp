#include "SBCPlayerGoalHUD.h"

#include "Buildables/FGBuildable.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/InputComponent.h"
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
	// Input must be sampled every frame. A 50 ms interval can miss a quick key press.
	PrimaryActorTick.TickInterval = 0.0f;
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
	if (!bCalculatorToggleBound)
	{
		EnableInput(PlayerController);
		if (InputComponent)
		{
			InputComponent->BindKey(EKeys::F6, IE_Pressed, this, &ASBCPlayerGoalHUD::ToggleCalculatorWidget);
			FInputKeyBinding& EscapeBinding = InputComponent->BindKey(
				EKeys::Escape, IE_Pressed, this, &ASBCPlayerGoalHUD::CloseCalculatorWidget);
			EscapeBinding.bConsumeInput = false;
			CalculatorEscapeBinding = &EscapeBinding;
			InputComponent->Priority = 1000;
			bCalculatorToggleBound = true;
		}
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
			CalculatorWidget->SetOnRequestClose(FSimpleDelegate::CreateUObject(this, &ASBCPlayerGoalHUD::CloseCalculatorWidget));
			// The viewport slot does not exist until AddToViewport. Use an absolute
			// viewport center below instead of combining a center anchor with (0, 0),
			// which places half of the widget outside the top-left of the screen.
			CalculatorWidget->AddToViewport(100);
			CalculatorWidget->SetAnchorsInViewport(FAnchors(0.0f, 0.0f));
			CalculatorWidget->SetAlignmentInViewport(FVector2D(0.5f, 0.5f));
			UpdateCalculatorLayout();
			CalculatorWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	return IsValid(GoalWidget);
}

void ASBCPlayerGoalHUD::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bReleaseEscapeCapture && CalculatorEscapeBinding)
	{
		CalculatorEscapeBinding->bConsumeInput = false;
		bReleaseEscapeCapture = false;
	}
	if (!EnsureLocalPlayer())
	{
		return;
	}

	if (PlayerController->WasInputKeyJustPressed(EKeys::F8)) ToggleWidget();
	if (PlayerController->WasInputKeyJustPressed(EKeys::F7)) AddLookedAtGoal();
	if (PlayerController->WasInputKeyJustPressed(EKeys::Up)) ChangeSelection(-1);
	if (PlayerController->WasInputKeyJustPressed(EKeys::Down)) ChangeSelection(1);
	if (PlayerController->WasInputKeyJustPressed(EKeys::Add) || PlayerController->WasInputKeyJustPressed(EKeys::Equals)) ChangeTargetCount(1);
	if (PlayerController->WasInputKeyJustPressed(EKeys::Subtract) || PlayerController->WasInputKeyJustPressed(EKeys::Hyphen)) ChangeTargetCount(-1);
	if (PlayerController->WasInputKeyJustPressed(EKeys::Enter)) ToggleManualCompletion();
	if (PlayerController->WasInputKeyJustPressed(EKeys::Delete)) RemoveSelectedGoal();
	if (PlayerController->WasInputKeyJustPressed(EKeys::L)) LinkLookedAtBuildable();
	if (CalculatorWidget && CalculatorWidget->GetVisibility() == ESlateVisibility::Visible)
	{
		UpdateCalculatorLayout();
	}

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
	if (CalculatorEscapeBinding)
	{
		if (bOpen)
		{
			CalculatorEscapeBinding->bConsumeInput = true;
		}
		else
		{
			// Keep Escape consumed for the frame that closes the calculator. Release
			// it on the next tick so the pause menu does not open underneath it.
			bReleaseEscapeCapture = true;
		}
	}
	if (bOpen)
	{
		CalculatorWidget->RefreshGameState();
		UpdateCalculatorLayout();
	}
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

void ASBCPlayerGoalHUD::CloseCalculatorWidget()
{
	if (CalculatorWidget && CalculatorWidget->GetVisibility() == ESlateVisibility::Visible)
	{
		ToggleCalculatorWidget();
	}
}

void ASBCPlayerGoalHUD::UpdateCalculatorLayout()
{
	if (!CalculatorWidget || !PlayerController) return;
	int32 ViewportWidth = 0;
	int32 ViewportHeight = 0;
	PlayerController->GetViewportSize(ViewportWidth, ViewportHeight);
	if (ViewportWidth <= 0 || ViewportHeight <= 0) return;

	const float ViewportScale = FMath::Max(0.1f, UWidgetLayoutLibrary::GetViewportScale(this));
	const float LogicalWidth = ViewportWidth / ViewportScale;
	const float LogicalHeight = ViewportHeight / ViewportScale;
	const FVector2D ResponsiveSize(
		FMath::Clamp(LogicalWidth * 0.88f, 700.0f, 1500.0f),
		FMath::Clamp(LogicalHeight * 0.82f, 460.0f, 900.0f));
	CalculatorWidget->SetPanelSize(ResponsiveSize);
	CalculatorWidget->SetPositionInViewport(
		FVector2D(ViewportWidth * 0.5f, ViewportHeight * 0.5f), true);
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
