// Tolk - Unreal Engine plugin: module startup/shutdown.
#include "Modules/ModuleManager.h"

#include "Tolk.h"

class FTolkModule : public IModuleInterface
{
public:
  virtual void StartupModule() override
  {
    if (!Tolk_IsLoaded())
    {
      Tolk_Load();
    }
  }

  virtual void ShutdownModule() override
  {
    if (Tolk_IsLoaded())
    {
      Tolk_Unload();
    }
  }
};

IMPLEMENT_MODULE(FTolkModule, Tolk)