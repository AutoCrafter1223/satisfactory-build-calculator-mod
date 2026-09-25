#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogSatisfactoryBuildCalculator, Log, All);

class FSBCProductionData;

class FSatisfactoryBuildCalculatorModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	const TSharedPtr<FSBCProductionData>& GetProductionData() const { return ProductionData; }

private:
	TSharedPtr<FSBCProductionData> ProductionData;
};
