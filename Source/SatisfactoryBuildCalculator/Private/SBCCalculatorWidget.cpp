#include "SBCCalculatorWidget.h"

#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "SatisfactoryBuildCalculator.h"
#include "SBCProductionCalculator.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

FText USBCCalculatorWidget::Text(const TCHAR* Korean, const TCHAR* English) const
{
	return FText::FromString(bKorean ? Korean : English);
}

TSharedRef<SWidget> USBCCalculatorWidget::RebuildWidget()
{
	bKorean = FInternationalization::Get().GetCurrentCulture()->GetTwoLetterISOLanguageName() == TEXT("ko");

	TSharedRef<SWidget> Result =
		SNew(SBox)
		.WidthOverride(980.0f)
		.HeightOverride(700.0f)
		[
			SNew(SBorder)
			.Padding(16.0f)
			.BorderBackgroundColor(FLinearColor(0.025f, 0.055f, 0.07f, 0.96f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 12.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.0f)
					[
						SNew(STextBlock)
						.Text(Text(TEXT("세티스팩토리 빌드 계산기"), TEXT("Satisfactory Build Calculator")))
						.ColorAndOpacity(FLinearColor(1.0f, 0.62f, 0.14f))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 22))
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(Text(TEXT("F6 닫기"), TEXT("F6 Close")))
						.ColorAndOpacity(FLinearColor(0.62f, 0.72f, 0.77f))
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))
					]
				]
				+ SVerticalBox::Slot().FillHeight(1.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(0.36f).Padding(0.0f, 0.0f, 12.0f, 0.0f)
					[
						SNew(SBorder)
						.Padding(10.0f)
						.BorderBackgroundColor(FLinearColor(0.04f, 0.08f, 0.10f, 0.82f))
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
							[
								SAssignNew(SearchBox, SEditableTextBox)
								.HintText(Text(TEXT("제품 또는 발전기 검색"), TEXT("Search products or generators")))
								.OnTextChanged_Lambda([this](const FText& NewText)
								{
									SearchText = NewText.ToString();
									RefreshProductList();
								})
							]
							+ SVerticalBox::Slot().FillHeight(1.0f)
							[
								SNew(SScrollBox)
								+ SScrollBox::Slot()
								[
									SAssignNew(ProductListBox, SVerticalBox)
								]
							]
						]
					]
					+ SHorizontalBox::Slot().FillWidth(0.64f)
					[
						SNew(SBorder)
						.Padding(12.0f)
						.BorderBackgroundColor(FLinearColor(0.04f, 0.08f, 0.10f, 0.72f))
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 10.0f, 0.0f)
								[
									SNew(STextBlock)
									.Text(Text(TEXT("목표 생산량"), TEXT("Target rate")))
									.Font(FCoreStyle::GetDefaultFontStyle("Regular", 12))
								]
								+ SHorizontalBox::Slot().FillWidth(1.0f)
								[
									SNew(SSpinBox<double>)
									.MinValue(0.01)
									.MaxValue(1000000.0)
									.Value(TargetRate)
									.OnValueChanged_Lambda([this](double NewValue)
									{
										TargetRate = NewValue;
									})
								]
								+ SHorizontalBox::Slot().AutoWidth().Padding(10.0f, 0.0f, 0.0f, 0.0f)
								[
									SNew(SButton)
									.Text(Text(TEXT("계산"), TEXT("Calculate")))
									.OnClicked_Lambda([this]()
									{
										CalculateSelected();
										return FReply::Handled();
									})
								]
							]
							+ SVerticalBox::Slot().FillHeight(1.0f)
							[
								SNew(SScrollBox)
								+ SScrollBox::Slot()
								[
									SAssignNew(ResultBox, SVerticalBox)
								]
							]
						]
					]
				]
			]
		];

	RefreshProductList();
	if (SelectedItemId.IsEmpty())
	{
		SelectedItemId = TEXT("Desc_ComputerSuper_C");
	}
	CalculateSelected();
	return Result;
}

void USBCCalculatorWidget::FocusSearchBox()
{
	if (SearchBox.IsValid())
	{
		FSlateApplication::Get().SetKeyboardFocus(SearchBox, EFocusCause::SetDirectly);
	}
}

void USBCCalculatorWidget::RefreshProductList()
{
	if (!ProductListBox.IsValid()) return;
	ProductListBox->ClearChildren();
	FSatisfactoryBuildCalculatorModule* Module = FModuleManager::GetModulePtr<FSatisfactoryBuildCalculatorModule>(TEXT("SatisfactoryBuildCalculator"));
	const TSharedPtr<FSBCProductionData> Data = Module ? Module->GetProductionData() : nullptr;
	if (!Data || !Data->IsLoaded()) return;

	TArray<const FSBCItemDefinition*> Matches;
	for (const TPair<FString, FSBCItemDefinition>& Pair : Data->GetItems())
	{
		const FSBCItemDefinition& Item = Pair.Value;
		if (!Data->FindRecipesForItem(Item.Id)) continue;
		const FString Name = bKorean ? Item.NameKo : Item.NameEn;
		if (SearchText.IsEmpty() || Name.Contains(SearchText, ESearchCase::IgnoreCase))
		{
			Matches.Add(&Item);
		}
	}
	Matches.Sort([this](const FSBCItemDefinition& A, const FSBCItemDefinition& B)
	{
		return (bKorean ? A.NameKo : A.NameEn) < (bKorean ? B.NameKo : B.NameEn);
	});

	for (const FSBCItemDefinition* Item : Matches)
	{
		const FString ItemId = Item->Id;
		const FText Name = FText::FromString(bKorean ? Item->NameKo : Item->NameEn);
		ProductListBox->AddSlot().AutoHeight().Padding(0.0f, 1.0f)
		[
			SNew(SButton)
			.ContentPadding(FMargin(8.0f, 5.0f))
			.OnClicked_Lambda([this, ItemId]()
			{
				SelectProduct(ItemId);
				return FReply::Handled();
			})
			[
				SNew(STextBlock)
				.Text(Name)
				.Font(FCoreStyle::GetDefaultFontStyle(SelectedItemId == ItemId ? "Bold" : "Regular", 11))
			]
		];
	}
}

void USBCCalculatorWidget::SelectProduct(const FString& ItemId)
{
	SelectedItemId = ItemId;
	RefreshProductList();
	CalculateSelected();
}

void USBCCalculatorWidget::CalculateSelected()
{
	if (!ResultBox.IsValid()) return;
	ResultBox->ClearChildren();
	FSatisfactoryBuildCalculatorModule* Module = FModuleManager::GetModulePtr<FSatisfactoryBuildCalculatorModule>(TEXT("SatisfactoryBuildCalculator"));
	const TSharedPtr<FSBCProductionData> Data = Module ? Module->GetProductionData() : nullptr;
	if (!Data || !Data->IsLoaded())
	{
		ResultBox->AddSlot().AutoHeight()[SNew(STextBlock).Text(Text(TEXT("계산 데이터를 불러오지 못했습니다."), TEXT("Calculator data could not be loaded.")))];
		return;
	}
	if (SelectedItemId.IsEmpty())
	{
		ResultBox->AddSlot().AutoHeight()[SNew(STextBlock).Text(Text(TEXT("왼쪽에서 제품을 선택하세요."), TEXT("Select a product on the left.")))];
		return;
	}

	FSBCProductionCalculator Calculator(*Data);
	FString Error;
	const TSharedPtr<FSBCProductionNode> Root = Calculator.Calculate(SelectedItemId, TargetRate, {}, {}, 0, bKorean, 0.0, Error);
	if (!Root)
	{
		ResultBox->AddSlot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(Error)).ColorAndOpacity(FLinearColor(1.0f, 0.35f, 0.25f))];
		return;
	}
	AddResultNode(Root, 0);
}

void USBCCalculatorWidget::AddResultNode(const TSharedPtr<FSBCProductionNode>& Node, int32 Depth)
{
	if (!Node || !ResultBox.IsValid()) return;
	const FString RateUnit = Node->Unit == TEXT("m3") ? TEXT("m³/min") : (Node->bGenerator ? TEXT("MW") : TEXT("개/min"));
	FString Detail;
	if (Node->bRawResource)
	{
		Detail = bKorean ? TEXT("원자재 · 외부 입력") : TEXT("Raw resource · external input");
	}
	else if (Node->bGenerator)
	{
		Detail = bKorean
			? FString::Printf(TEXT("%s · 설치 %d기"), *Node->BuildingName, Node->InstalledBuildingCount)
			: FString::Printf(TEXT("%s · %d installed"), *Node->BuildingName, Node->InstalledBuildingCount);
	}
	else
	{
		Detail = bKorean
			? FString::Printf(TEXT("%s · 계산 %.2f대 · 설치 %d대 · %.1f MW"),
				*Node->BuildingName, Node->ExactBuildingCount, Node->InstalledBuildingCount, Node->PowerMW)
			: FString::Printf(TEXT("%s · %.2f calculated · %d installed · %.1f MW"),
				*Node->BuildingName, Node->ExactBuildingCount, Node->InstalledBuildingCount, Node->PowerMW);
	}

	ResultBox->AddSlot().AutoHeight().Padding(Depth * 18.0f, 2.0f, 0.0f, 2.0f)
	[
		SNew(SBorder)
		.Padding(FMargin(9.0f, 6.0f))
		.BorderBackgroundColor(Depth == 0 ? FLinearColor(0.12f, 0.18f, 0.20f, 0.92f) : FLinearColor(0.055f, 0.09f, 0.11f, 0.74f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::FromString(FString::Printf(TEXT("%s  %.2f %s"), *Node->ItemName, Node->RequiredRate, *RateUnit)))
				.ColorAndOpacity(Depth == 0 ? FLinearColor(1.0f, 0.68f, 0.22f) : FLinearColor::White)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", Depth == 0 ? 14 : 12))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Detail))
				.ColorAndOpacity(FLinearColor(0.58f, 0.72f, 0.78f))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
			]
		]
	];
	for (const TSharedPtr<FSBCProductionNode>& Child : Node->Children)
	{
		AddResultNode(Child, Depth + 1);
	}
}
