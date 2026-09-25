#include "SBCGameInstanceModule.h"

#include "SBCRemoteCallObject.h"

USBCGameInstanceModule::USBCGameInstanceModule()
{
	bRootModule = true;
	RemoteCallObjects.Add(USBCRemoteCallObject::StaticClass());
}
