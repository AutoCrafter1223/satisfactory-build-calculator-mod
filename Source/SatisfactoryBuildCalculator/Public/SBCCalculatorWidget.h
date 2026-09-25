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

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	TSharedPtr<SVerticalBox> ProductListBox;
	TSharedPtr<SVerticalBox> ResultBox;
	TSharedPtr<class SEditableTextBox> SearchBox;
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
