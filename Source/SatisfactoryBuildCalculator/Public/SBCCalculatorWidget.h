#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SBCCalculatorWidget.generated.h"

class SVerticalBox;
struct FSBCProductionNode;

UCLASS()
class SATISFACTORYBUILDCALCULATOR_API USBCCalculatorWidget final : public UUserWidget
{
	GENERATED_BODY()

public:
	void FocusSearchBox();
	void SetPanelSize(const FVector2D& NewSize);
	void RefreshGameState();
	void SetOnRequestClose(FSimpleDelegate InDelegate) { OnRequestClose = MoveTemp(InDelegate); }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	TSharedPtr<SVerticalBox> ProductListBox;
	TSharedPtr<SVerticalBox> ResultBox;
	TSharedPtr<class SBox> RootSizeBox;
	TSharedPtr<class SEditableTextBox> SearchBox;
	FVector2D PanelSize = FVector2D(1000.0f, 650.0f);
	FString SearchText;
	FString SelectedItemId;
	TSet<FString> UnlockedRecipeIds;
	TSet<FString> UnlockedItemIds;
	TSet<FString> UnlockedBuildingIds;
	double TargetRate = 10.0;
	bool bKorean = true;
	bool bRecipeStateReady = false;
	bool bCompactView = false;
	FSimpleDelegate OnRequestClose;

	void RefreshProductList();
	bool IsRecipeUnlocked(const struct FSBCRecipeDefinition& Recipe) const;
	bool IsProductUnlocked(const FString& ItemId) const;
	FText GetRecipeSyncStatus() const;
	void SelectProduct(const FString& ItemId);
	void CalculateSelected();
	void AddResultNode(const TSharedPtr<FSBCProductionNode>& Node, int32 Depth);
	FText Text(const TCHAR* Korean, const TCHAR* English) const;
};
