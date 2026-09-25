#include "SatisfactoryBuildCalculator.h"

DEFINE_LOG_CATEGORY(LogSatisfactoryBuildCalculator);

void FSatisfactoryBuildCalculatorModule::StartupModule()
{
	UE_LOG(LogSatisfactoryBuildCalculator, Log, TEXT("Satisfactory Build Calculator module loaded."));
}

void FSatisfactoryBuildCalculatorModule::ShutdownModule()
{
	UE_LOG(LogSatisfactoryBuildCalculator, Log, TEXT("Satisfactory Build Calculator module unloaded."));
}

IMPLEMENT_MODULE(FSatisfactoryBuildCalculatorModule, SatisfactoryBuildCalculator)
