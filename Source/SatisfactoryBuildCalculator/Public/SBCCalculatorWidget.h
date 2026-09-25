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

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	TSharedPtr<SVerticalBox> ProductListBox;
	TSharedPtr<SVerticalBox> ResultBox;
	TSharedPtr<class SBox> RootSizeBox;
	TSharedPtr<class SEditableTextBox> SearchBox;
	FVector2D PanelSize = FVector2D(1000.0f, 650.0f);
	FString SearchText;
	FString SelectedItemId;
	double TargetRate = 10.0;
	bool bKorean = true;

	void RefreshProductList();
	void SelectProduct(const FString& ItemId);
	void CalculateSelected();
	void AddResultNode(const TSharedPtr<FSBCProductionNode>& Node, int32 Depth);
	FText Text(const TCHAR* Korean, const TCHAR* English) const;
};
