#include "Modules/ModuleManager.h"

#include "CLifeDemoHost.h"
#include "Engine/World.h"
#include "EngineUtils.h"

class FCLifeAdapterModule final : public IModuleInterface
{
public:
    void StartupModule() override
    {
        WorldInitializedHandle = FWorldDelegates::OnPostWorldInitialization.AddLambda(
            [](UWorld* World, const UWorld::InitializationValues) {
                if (World == nullptr || !World->IsGameWorld()) {
                    return;
                }
                for (TActorIterator<ACLifeDemoHost> Existing(World); Existing; ++Existing) {
                    return;
                }
                World->SpawnActor<ACLifeDemoHost>();
            });
    }

    void ShutdownModule() override
    {
        FWorldDelegates::OnPostWorldInitialization.Remove(WorldInitializedHandle);
    }

private:
    FDelegateHandle WorldInitializedHandle;
};

IMPLEMENT_MODULE(FCLifeAdapterModule, CLifeAdapter)
