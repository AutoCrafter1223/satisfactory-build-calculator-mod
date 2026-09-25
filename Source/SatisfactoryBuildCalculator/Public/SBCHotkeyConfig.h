#pragma once

#include "Blueprint/UserWidget.h"
#include "Configuration/ModConfiguration.h"
#include "Configuration/Properties/ConfigPropertyString.h"
#include "InputCoreTypes.h"
#include "SBCHotkeyConfig.generated.h"

enum class ESBCHotkeyAction : uint8
{
	ManualCompletion,
	AddAimedBuilding,
	ToggleGoalHUD,
	ToggleCalculator
};

UCLASS(EditInlineNew)
class SATISFACTORYBUILDCALCULATOR_API USBCKeyConfigProperty final : public UConfigPropertyString
{
	GENERATED_BODY()

public:
	virtual UUserWidget* CreateEditorWidget_Implementation(UUserWidget* ParentWidget) const override;
};

UCLASS()
class SATISFACTORYBUILDCALCULATOR_API USBCKeyConfigEditorWidget final : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeProperty(USBCKeyConfigProperty* InProperty);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<USBCKeyConfigProperty> Property = nullptr;

	bool bCapturing = false;
	FReply BeginCapture();
	FText GetKeyButtonText() const;
};

UCLASS()
class SATISFACTORYBUILDCALCULATOR_API USBCModConfiguration final : public UModConfiguration
{
	GENERATED_BODY()

public:
	USBCModConfiguration();
};

namespace SBCHotkeys
{
	FKey Get(UObject* WorldContext, ESBCHotkeyAction Action);
	FText GetDisplayName(UObject* WorldContext, ESBCHotkeyAction Action);
}
