#include "SBCGameInstanceModule.h"

#include "SBCHotkeyConfig.h"
#include "SBCRemoteCallObject.h"

USBCGameInstanceModule::USBCGameInstanceModule()
{
	bRootModule = true;
	ModConfigurations.Add(USBCModConfiguration::StaticClass());
	RemoteCallObjects.Add(USBCRemoteCallObject::StaticClass());
}
