#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SBCLocalization.h"
#include "SBCProductionCalculator.h"
#include "SBCCalculatorWidget.generated.h"

class SVerticalBox;
class SSBCPanGraph;
struct FSBCProductionNode;

struct FSBCCalculatedGoalRequest
{
	FString RecipeId;
	FString BuildingId;
	int32 TargetCount = 0;
	int32 PowerShards = 0;
	int32 Somersloops = 0;
	bool bRequiresRecipe = true;
};

UCLASS()
class SATISFACTORYBUILDCALCULATOR_API USBCCalculatorWidget final : public UUserWidget
{
	GENERATED_BODY()

public:
	void FocusSearchBox();
	void SetPanelSize(const FVector2D& NewSize);
	void RefreshGameState();
	void SetOnRequestClose(FSimpleDelegate InDelegate) { OnRequestClose = MoveTemp(InDelegate); }
	void SetOnRequestAddGoals(FSimpleDelegate InDelegate) { OnRequestAddGoals = MoveTemp(InDelegate); }
	TArray<FSBCCalculatedGoalRequest> GetGoalPlan() const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	TSharedPtr<SVerticalBox> ProductListBox;
	TSharedPtr<SSBCPanGraph> ResultGraph;
	TSharedPtr<SVerticalBox> SummaryBox;
	TSharedPtr<class SBox> RootSizeBox;
	TSharedPtr<class SEditableTextBox> SearchBox;
	FVector2D PanelSize = FVector2D(1000.0f, 650.0f);
	FString SearchText;
	FString SelectedItemId;
	TSet<FString> UnlockedRecipeIds;
	TSet<FString> UnlockedItemIds;
	TSet<FString> UnlockedBuildingIds;
	TMap<FString, FString> SelectedRecipes;
	TMap<FString, FSBCMachineSettings> MachineSettings;
	TSet<FString> CollapsedNodeIds;
	FString SelectedGoalNodeId;
	double TargetRate = 10.0;
	int32 DefaultPowerShards = 0;
	ESBCLanguage Language = ESBCLanguage::English;
	bool bRecipeStateReady = false;
	bool bCompactView = false;
	FSimpleDelegate OnRequestClose;
	FSimpleDelegate OnRequestAddGoals;
	TSharedPtr<FSBCProductionNode> LastCalculatedRoot;

	void RefreshProductList();
	bool IsRecipeUnlocked(const struct FSBCRecipeDefinition& Recipe) const;
	bool IsProductUnlocked(const FString& ItemId) const;
	FText GetRecipeSyncStatus() const;
	void SelectProduct(const FString& ItemId);
	void CalculateSelected();
	TSharedRef<SWidget> BuildRecipeMenu(FString NodeId, FString ItemId);
	void SelectRecipeForNode(const FString& NodeId, const FString& RecipeId);
	void SetNodePowerShards(const FString& NodeId, int32 Count);
	void SetNodeSomersloops(const FString& NodeId, int32 Count);
	void SetDefaultPowerShards(int32 Count);
	void RefreshSummary(const TSharedPtr<FSBCProductionNode>& Root);
	void BuildResultGraph(const TSharedPtr<FSBCProductionNode>& Root);
	TSharedRef<SWidget> BuildResultCard(const TSharedPtr<FSBCProductionNode>& Node, const FString& ParentName);
	void ToggleNodeCollapsed(const FString& NodeId);
	void SelectGoalNode(const FString& NodeId);
	FLinearColor GetBuildingColor(const TSharedPtr<FSBCProductionNode>& Node) const;
	FText Text(const TCHAR* Korean, const TCHAR* English, const TCHAR* Chinese = nullptr, const TCHAR* German = nullptr) const;
};
