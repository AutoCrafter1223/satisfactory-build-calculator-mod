#include "SatisfactoryBuildCalculator.h"

#include "SBCProductionCalculator.h"

DEFINE_LOG_CATEGORY(LogSatisfactoryBuildCalculator);

void FSatisfactoryBuildCalculatorModule::StartupModule()
{
	ProductionData = MakeShared<FSBCProductionData>();
	FString DataError;
	if (ProductionData->Load(DataError))
	{
		UE_LOG(LogSatisfactoryBuildCalculator, Log,
			TEXT("Satisfactory Build Calculator loaded %d items, %d recipes, and %d buildings."),
			ProductionData->GetItems().Num(), ProductionData->GetRecipeCount(), ProductionData->GetBuildingCount());
	}
	else
	{
		UE_LOG(LogSatisfactoryBuildCalculator, Error, TEXT("Calculator data failed to load: %s"), *DataError);
	}
}

void FSatisfactoryBuildCalculatorModule::ShutdownModule()
{
	ProductionData.Reset();
	UE_LOG(LogSatisfactoryBuildCalculator, Log, TEXT("Satisfactory Build Calculator module unloaded."));
}

IMPLEMENT_MODULE(FSatisfactoryBuildCalculatorModule, SatisfactoryBuildCalculator)
