#include "SBCPlayerGoalHUD.h"

#include "Buildables/FGBuildable.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/InputComponent.h"
#include "Engine/World.h"
#include "FGCharacterPlayer.h"
#include "FGPlayerController.h"
#include "FGRecipe.h"
#include "FGRecipeManager.h"
#include "InputCoreTypes.h"
#include "SBCCalculatorWidget.h"
#include "SBCGoalHUDWidget.h"
#include "SBCGoalSubsystem.h"
#include "SBCHotkeyConfig.h"
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
			FInputKeyBinding& EscapeBinding = InputComponent->BindKey(
				EKeys::Escape, IE_Pressed, this, &ASBCPlayerGoalHUD::CloseCalculatorWidget);
			EscapeBinding.bConsumeInput = false;
			CalculatorEscapeBinding = &EscapeBinding;
			InputComponent->Priority = 1000;
			bCalculatorToggleBound = true;
		}
	}
	if (!bGoalsDelegateBound)
	{
		if (ASBCGoalSubsystem* Goals = ASBCGoalSubsystem::Get(this))
		{
			Goals->OnGoalsChanged.AddUniqueDynamic(this, &ASBCPlayerGoalHUD::HandleGoalsChanged);
			bGoalsDelegateBound = true;
		}
	}
	if (USBCRemoteCallObject* RCO = GetRemoteCallObject(); RCO && BoundGoalBatchRCO.Get() != RCO)
	{
		if (USBCRemoteCallObject* PreviousRCO = BoundGoalBatchRCO.Get())
		{
			PreviousRCO->OnGoalBatchResult.RemoveAll(this);
		}
		RCO->OnGoalBatchResult.AddUObject(this, &ASBCPlayerGoalHUD::HandleGoalBatchResult);
		BoundGoalBatchRCO = RCO;
		bGoalBatchDelegateBound = true;
	}

	if (!IsValid(GoalWidget))
	{
		GoalWidget = CreateWidget<USBCGoalHUDWidget>(PlayerController, USBCGoalHUDWidget::StaticClass());
		if (GoalWidget)
		{
			GoalWidget->SetObservedPlayer(PlayerCharacter);
			GoalWidget->SetPositionInViewport(FVector2D(28.0f, 190.0f), false);
			// Keep the compact tracker below Satisfactory's construction and menu layers.
			GoalWidget->AddToViewport(-10);
		}
	}
	if (!IsValid(CalculatorWidget))
	{
		CalculatorWidget = CreateWidget<USBCCalculatorWidget>(PlayerController, USBCCalculatorWidget::StaticClass());
		if (CalculatorWidget)
		{
			CalculatorWidget->SetOnRequestClose(FSimpleDelegate::CreateUObject(this, &ASBCPlayerGoalHUD::CloseCalculatorWidget));
			CalculatorWidget->SetOnRequestAddGoals(FSimpleDelegate::CreateUObject(this, &ASBCPlayerGoalHUD::AddCalculatorPlanToGoals));
			CalculatorWidget->SetOnRequestClearGoals(FSimpleDelegate::CreateUObject(this, &ASBCPlayerGoalHUD::ClearPlayerGoals));
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

void ASBCPlayerGoalHUD::AddCalculatorPlanToGoals()
{
	if (!CalculatorWidget) return;
	USBCRemoteCallObject* RCO = GetRemoteCallObject();
	AFGRecipeManager* RecipeManager = AFGRecipeManager::Get(this);
	if (!RCO || !RecipeManager)
	{
		CalculatorWidget->SetGoalAddResult(1, 0, {TEXT("Recipe manager or network object")});
		return;
	}

	const TArray<FSBCCalculatedGoalRequest> Plan = CalculatorWidget->GetGoalPlan();
	if (Plan.IsEmpty())
	{
		CalculatorWidget->SetGoalAddResult(1, 0, {TEXT("No production facility cards")});
		return;
	}

	auto NormalizeClassId = [](FString Value)
	{
		Value.ReplaceInline(TEXT("Default__"), TEXT(""), ESearchCase::IgnoreCase);
		int32 Separator = INDEX_NONE;
		if (Value.FindLastChar(TEXT('.'), Separator)) Value = Value.Mid(Separator + 1);
		if (Value.FindLastChar(TEXT('/'), Separator)) Value = Value.Mid(Separator + 1);
		return Value.ToLower();
	};

	TMap<FString, TSubclassOf<UFGRecipe>> RecipesById;
	for (const TSubclassOf<UFGRecipe>& RecipeClass : RecipeManager->GetAllRecipes())
	{
		if (RecipeClass) RecipesById.Add(NormalizeClassId(RecipeClass->GetName()), RecipeClass);
	}
	TMap<FString, TSubclassOf<AFGBuildable>> BuildingsById;
	for (const TSubclassOf<AFGBuildable>& BuildableClass : RecipeManager->GetAvailableBuildingsOfType<AFGBuildable>())
	{
		if (BuildableClass) BuildingsById.Add(NormalizeClassId(BuildableClass->GetName()), BuildableClass);
	}
	// Some valid producers are not returned by the "available buildings" list in
	// every game state. Index producer classes from recipes as a second source.
	for (const TPair<FString, TSubclassOf<UFGRecipe>>& RecipePair : RecipesById)
	{
		for (const TSubclassOf<UObject>& Producer : UFGRecipe::GetProducedIn(RecipePair.Value))
		{
			UClass* ProducerClass = Producer.Get();
			if (ProducerClass && ProducerClass->IsChildOf(AFGBuildable::StaticClass()))
			{
				BuildingsById.FindOrAdd(NormalizeClassId(ProducerClass->GetName())) = ProducerClass;
			}
		}
	}

	const FGuid GoalGroupId = FGuid::NewGuid();
	TArray<FSBCGoalBatchEntry> Batch;
	TArray<FString> ResolutionFailures;
	Batch.Reserve(Plan.Num());
	for (const FSBCCalculatedGoalRequest& Request : Plan)
	{
		const TSubclassOf<UFGRecipe>* RecipeClass = Request.bRequiresRecipe
			? RecipesById.Find(NormalizeClassId(Request.RecipeId))
			: nullptr;
		const TSubclassOf<AFGBuildable>* BuildableClass = BuildingsById.Find(NormalizeClassId(Request.BuildingId));
		if ((Request.bRequiresRecipe && !RecipeClass) || !BuildableClass)
		{
			ResolutionFailures.Add(FString::Printf(TEXT("%s / %s"), *Request.RecipeId, *Request.BuildingId));
			continue;
		}

		FSBCGoalBatchEntry& Entry = Batch.AddDefaulted_GetRef();
		Entry.RecipeClass = RecipeClass ? *RecipeClass : nullptr;
		Entry.BuildableClass = *BuildableClass;
		Entry.TargetCount = Request.TargetCount;
		Entry.PowerShards = Request.PowerShards;
		Entry.Somersloops = Request.Somersloops;
		Entry.HierarchyKey = Request.HierarchyKey;
		Entry.ParentHierarchyKey = Request.ParentHierarchyKey;
		Entry.HierarchyDepth = Request.HierarchyDepth;
		Entry.HierarchyOrder = Request.HierarchyOrder;
	}

	if (Batch.Num() != Plan.Num() || !ResolutionFailures.IsEmpty())
	{
		CalculatorWidget->SetGoalAddResult(Plan.Num(), 0, ResolutionFailures);
		return;
	}

	RCO->ServerAddGoalBatch(Batch, GoalGroupId);
}

void ASBCPlayerGoalHUD::ClearPlayerGoals()
{
	if (USBCRemoteCallObject* RCO = GetRemoteCallObject())
	{
		RCO->ServerClearGoals();
	}
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
	if (bCalculatorClosedByWidget)
	{
		bCalculatorClosedByWidget = false;
		return;
	}

	const FKey CalculatorKey = SBCHotkeys::Get(this, ESBCHotkeyAction::ToggleCalculator);
	const bool bCalculatorOpen = CalculatorWidget && CalculatorWidget->GetVisibility() == ESlateVisibility::Visible;
	if (PlayerController->WasInputKeyJustPressed(CalculatorKey))
	{
		ToggleCalculatorWidget();
		return;
	}
	if (bCalculatorOpen)
	{
		UpdateCalculatorLayout();
		return;
	}

	const FKey GoalHudKey = SBCHotkeys::Get(this, ESBCHotkeyAction::ToggleGoalHUD);
	const FKey AddAimedKey = SBCHotkeys::Get(this, ESBCHotkeyAction::AddAimedBuilding);
	const FKey ManualKey = SBCHotkeys::Get(this, ESBCHotkeyAction::ManualCompletion);
	if (PlayerController->WasInputKeyJustPressed(GoalHudKey))
	{
		ToggleWidget();
	}
	else if (PlayerController->WasInputKeyJustPressed(AddAimedKey))
	{
		AddLookedAtGoal();
	}

	const bool bGoalHudVisible = GoalWidget && GoalWidget->GetVisibility() != ESlateVisibility::Collapsed;
	if (bGoalHudVisible)
	{
		if (PlayerController->WasInputKeyJustPressed(ManualKey)) ToggleManualCompletion();
		if (PlayerController->WasInputKeyJustPressed(EKeys::Up)) ChangeSelection(-1);
		if (PlayerController->WasInputKeyJustPressed(EKeys::Down)) ChangeSelection(1);
		if (PlayerController->WasInputKeyJustPressed(EKeys::Add) || PlayerController->WasInputKeyJustPressed(EKeys::Equals)) ChangeTargetCount(1);
		if (PlayerController->WasInputKeyJustPressed(EKeys::Subtract) || PlayerController->WasInputKeyJustPressed(EKeys::Hyphen)) ChangeTargetCount(-1);
		if (PlayerController->WasInputKeyJustPressed(EKeys::Delete)) RemoveSelectedGoal();
		if (PlayerController->WasInputKeyJustPressed(EKeys::L)) LinkLookedAtBuildable();
	}
}

void ASBCPlayerGoalHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (USBCRemoteCallObject* RCO = BoundGoalBatchRCO.Get())
	{
		RCO->OnGoalBatchResult.RemoveAll(this);
	}
	BoundGoalBatchRCO.Reset();
	bGoalBatchDelegateBound = false;
	if (bGoalsDelegateBound)
	{
		if (ASBCGoalSubsystem* Goals = ASBCGoalSubsystem::Get(this))
		{
			Goals->OnGoalsChanged.RemoveDynamic(this, &ASBCPlayerGoalHUD::HandleGoalsChanged);
		}
		bGoalsDelegateBound = false;
	}
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

void ASBCPlayerGoalHUD::HandleGoalBatchResult(
	int32 ExpectedCount,
	int32 AddedCount,
	const TArray<FString>& FailedEntries)
{
	if (CalculatorWidget)
	{
		CalculatorWidget->SetGoalAddResult(ExpectedCount, AddedCount, FailedEntries);
	}
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
		bCalculatorClosedByWidget = true;
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

void ASBCPlayerGoalHUD::HandleGoalsChanged()
{
	ASBCGoalSubsystem* Subsystem = ASBCGoalSubsystem::Get(this);
	const int32 GoalCount = Subsystem ? Subsystem->GetGoalsForPlayer(PlayerCharacter).Num() : 0;
	SelectedGoalIndex = GoalCount > 0 ? FMath::Clamp(SelectedGoalIndex, 0, GoalCount - 1) : 0;
	if (GoalWidget)
	{
		GoalWidget->SetSelectedGoalIndex(SelectedGoalIndex);
	}
}
