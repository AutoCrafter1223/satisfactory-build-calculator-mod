#include "SBCHotkeyConfig.h"

#include "Configuration/ConfigManager.h"
#include "Configuration/Properties/ConfigPropertySection.h"
#include "Engine/GameInstance.h"
#include "SBCLocalization.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	const FConfigId HotkeyConfigId{TEXT("SatisfactoryBuildCalculator"), TEXT("")};

	const TCHAR* PropertyName(ESBCHotkeyAction Action)
	{
		switch (Action)
		{
		case ESBCHotkeyAction::ManualCompletion: return TEXT("ManualCompletionKey");
		case ESBCHotkeyAction::AddAimedBuilding: return TEXT("AddAimedBuildingKey");
		case ESBCHotkeyAction::ToggleGoalHUD: return TEXT("ToggleGoalHUDKey");
		case ESBCHotkeyAction::ToggleCalculator: return TEXT("ToggleCalculatorKey");
		default: return TEXT("");
		}
	}

	FKey DefaultKey(ESBCHotkeyAction Action)
	{
		switch (Action)
		{
		case ESBCHotkeyAction::ManualCompletion: return EKeys::F5;
		case ESBCHotkeyAction::AddAimedBuilding: return EKeys::F6;
		case ESBCHotkeyAction::ToggleGoalHUD: return EKeys::F7;
		case ESBCHotkeyAction::ToggleCalculator: return EKeys::F8;
		default: return EKeys::Invalid;
		}
	}

	void ConfigureKeyProperty(
		UConfigPropertySection* Root,
		USBCKeyConfigProperty* Property,
		const TCHAR* PropertyName,
		const TCHAR* KeyName,
		const TCHAR* KoreanName,
		const TCHAR* EnglishName,
		const TCHAR* ChineseName,
		const TCHAR* GermanName)
	{
		const ESBCLanguage Language = SBCLocalization::GetCurrentLanguage();
		Property->DisplayName = SBCLocalization::Text(Language, KoreanName, EnglishName, ChineseName, GermanName);
		Property->Tooltip = SBCLocalization::Text(Language,
			TEXT("버튼을 누른 뒤 사용할 키를 입력하세요."),
			TEXT("Press the button, then press the key to use."),
			TEXT("点击按钮，然后按下要使用的按键。"),
			TEXT("Klicke auf die Schaltfläche und drücke anschließend die gewünschte Taste."));
		Property->DefaultValue = KeyName;
		Property->Value = KeyName;
		Root->SectionProperties.Add(PropertyName, Property);
	}
}

UUserWidget* USBCKeyConfigProperty::CreateEditorWidget_Implementation(UUserWidget* ParentWidget) const
{
	if (!ParentWidget)
	{
		return nullptr;
	}
	USBCKeyConfigEditorWidget* Widget = CreateWidget<USBCKeyConfigEditorWidget>(
		ParentWidget->GetOwningPlayer(), USBCKeyConfigEditorWidget::StaticClass());
	if (!Widget)
	{
		Widget = NewObject<USBCKeyConfigEditorWidget>(ParentWidget);
	}
	Widget->InitializeProperty(const_cast<USBCKeyConfigProperty*>(this));
	return Widget;
}

void USBCKeyConfigEditorWidget::InitializeProperty(USBCKeyConfigProperty* InProperty)
{
	Property = InProperty;
	SetIsFocusable(true);
}

TSharedRef<SWidget> USBCKeyConfigEditorWidget::RebuildWidget()
{
	return SNew(SBorder)
		.Padding(FMargin(8.0f, 5.0f))
		.BorderBackgroundColor(FLinearColor(0.04f, 0.06f, 0.06f, 0.65f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(Property ? Property->DisplayName : FText::GetEmpty())
				.ToolTipText(Property ? Property->Tooltip : FText::GetEmpty())
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 12))
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SButton)
				.ContentPadding(FMargin(18.0f, 5.0f))
				.Text_Lambda([this]() { return GetKeyButtonText(); })
				.OnClicked_UObject(this, &USBCKeyConfigEditorWidget::BeginCapture)
			]
		];
}

FReply USBCKeyConfigEditorWidget::BeginCapture()
{
	bCapturing = true;
	SetKeyboardFocus();
	return FReply::Handled();
}

FText USBCKeyConfigEditorWidget::GetKeyButtonText() const
{
	if (bCapturing)
	{
		return SBCLocalization::Text(SBCLocalization::GetCurrentLanguage(), TEXT("키를 누르세요"), TEXT("Press a key"), TEXT("请按键"), TEXT("Taste drücken"));
	}
	if (!Property)
	{
		return FText::FromString(TEXT("-"));
	}
	const FKey Key(*Property->Value);
	return Key.IsValid() ? Key.GetDisplayName() : FText::FromString(Property->Value);
}

FReply USBCKeyConfigEditorWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (!bCapturing || !Property)
	{
		return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
	}
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Escape)
	{
		bCapturing = false;
		return FReply::Handled();
	}
	if (!Key.IsValid() || Key.IsModifierKey())
	{
		return FReply::Handled();
	}
	Property->Value = Key.GetFName().ToString();
	Property->MarkDirty();
	bCapturing = false;
	return FReply::Handled();
}

USBCModConfiguration::USBCModConfiguration()
{
	ConfigId = HotkeyConfigId;
	const ESBCLanguage Language = SBCLocalization::GetCurrentLanguage();
	DisplayName = SBCLocalization::Text(Language, TEXT("단축키"), TEXT("Hotkeys"), TEXT("快捷键"), TEXT("Tastenkürzel"));
	Description = SBCLocalization::Text(Language,
		TEXT("빌드 계산기와 건설 목표 단축키를 설정합니다."),
		TEXT("Configure calculator and build-goal hotkeys."),
		TEXT("设置建造计算器和建造目标快捷键。"),
		TEXT("Tastenkürzel für Rechner und Bauziele festlegen."));
	RootSection = CreateDefaultSubobject<UConfigPropertySection>(TEXT("RootSection"));
	RootSection->DisplayName = DisplayName;
	USBCKeyConfigProperty* Manual = CreateDefaultSubobject<USBCKeyConfigProperty>(TEXT("ManualCompletionKey"));
	USBCKeyConfigProperty* AddAimed = CreateDefaultSubobject<USBCKeyConfigProperty>(TEXT("AddAimedBuildingKey"));
	USBCKeyConfigProperty* ToggleGoals = CreateDefaultSubobject<USBCKeyConfigProperty>(TEXT("ToggleGoalHUDKey"));
	USBCKeyConfigProperty* ToggleCalculator = CreateDefaultSubobject<USBCKeyConfigProperty>(TEXT("ToggleCalculatorKey"));
	ConfigureKeyProperty(RootSection, Manual, TEXT("ManualCompletionKey"), TEXT("F5"), TEXT("수동 완료/취소"), TEXT("Complete/undo goal"), TEXT("完成/撤销目标"), TEXT("Ziel fertig/zurück"));
	ConfigureKeyProperty(RootSection, AddAimed, TEXT("AddAimedBuildingKey"), TEXT("F6"), TEXT("바라보는 시설 추가"), TEXT("Add aimed building"), TEXT("添加瞄准的建筑"), TEXT("Anvisiertes Gebäude hinzufügen"));
	ConfigureKeyProperty(RootSection, ToggleGoals, TEXT("ToggleGoalHUDKey"), TEXT("F7"), TEXT("건설 목표 창"), TEXT("Toggle goal HUD"), TEXT("建造目标窗口"), TEXT("Bauzielanzeige"));
	ConfigureKeyProperty(RootSection, ToggleCalculator, TEXT("ToggleCalculatorKey"), TEXT("F8"), TEXT("빌드 계산기 창"), TEXT("Toggle calculator"), TEXT("建造计算器窗口"), TEXT("Baurechner"));
}

FKey SBCHotkeys::Get(UObject* WorldContext, ESBCHotkeyAction Action)
{
	if (WorldContext)
	{
		if (UWorld* World = WorldContext->GetWorld())
		{
			if (UGameInstance* GameInstance = World->GetGameInstance())
			{
				if (UConfigManager* Manager = GameInstance->GetSubsystem<UConfigManager>())
				{
					if (UConfigPropertySection* Root = Manager->GetConfigurationRootSection(HotkeyConfigId))
					{
						if (const TObjectPtr<UConfigProperty>* Found = Root->SectionProperties.Find(PropertyName(Action)))
						{
							if (const UConfigPropertyString* Property = Cast<UConfigPropertyString>(*Found))
							{
								const FKey ConfiguredKey(*Property->Value);
								if (ConfiguredKey.IsValid()) return ConfiguredKey;
							}
						}
					}
				}
			}
		}
	}
	return DefaultKey(Action);
}

FText SBCHotkeys::GetDisplayName(UObject* WorldContext, ESBCHotkeyAction Action)
{
	return Get(WorldContext, Action).GetDisplayName();
}
