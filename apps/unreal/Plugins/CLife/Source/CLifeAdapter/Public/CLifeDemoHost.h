#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include <clife/presets/demo_session.hpp>

#include <map>

#include "CLifeDemoHost.generated.h"

class SWidget;
class UCameraComponent;
class UDirectionalLightComponent;
class USceneComponent;
class UStaticMeshComponent;

UCLASS()
class CLIFEADAPTER_API ACLifeDemoHost final : public AActor
{
    GENERATED_BODY()

public:
    ACLifeDemoHost();
    ~ACLifeDemoHost() override;

    void SetSimulationRunning(bool bRunning);
    void StepSimulation();
    void ResetSimulation();
    void SetLightInput(double Value);

    [[nodiscard]] bool IsSimulationRunning() const;
    [[nodiscard]] uint64 GetSimulationTick() const;
    [[nodiscard]] double GetLightInput() const;
    [[nodiscard]] double GetEnergy() const;
    [[nodiscard]] double GetUsedEnergy() const;
    [[nodiscard]] double GetTemperature() const;

protected:
    void BeginPlay() override;
    void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    void Tick(float DeltaSeconds) override;

private:
    struct FImpl final
    {
        clife::presets::DemoSession Session;
        std::map<clife::world::ObjectId, TWeakObjectPtr<USceneComponent>> ObjectViews;
    };

    void ApplyRuntimeToViews();
    void CreateDemoUI();

    UPROPERTY()
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> CellMesh;

    UPROPERTY()
    TObjectPtr<UCameraComponent> Camera;

    UPROPERTY()
    TObjectPtr<UDirectionalLightComponent> KeyLight;

    TUniquePtr<FImpl> Impl;
    TSharedPtr<SWidget> DemoWidget;
};
