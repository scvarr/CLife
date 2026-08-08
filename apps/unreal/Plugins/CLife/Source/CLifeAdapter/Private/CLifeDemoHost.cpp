#include "CLifeDemoHost.h"

#include "Camera/CameraComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "UObject/ConstructorHelpers.h"
#include "Widgets/SWidget.h"

#include "SCLifeDemoPanel.h"

namespace {

constexpr double VisualScaleBase = 1.0;
DEFINE_LOG_CATEGORY_STATIC(LogCLifeAdapter, Log, All);

} // namespace

ACLifeDemoHost::ACLifeDemoHost()
{
    PrimaryActorTick.bCanEverTick = true;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    CellMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Cell"));
    CellMesh->SetupAttachment(SceneRoot);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (SphereMesh.Succeeded()) {
        CellMesh->SetStaticMesh(SphereMesh.Object);
    }

    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(SceneRoot);
    Camera->SetRelativeLocation(FVector(500.0, 0.0, 0.0));
    Camera->SetRelativeRotation(FRotator(0.0, 180.0, 0.0));
    Camera->bAutoActivate = true;

    KeyLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("KeyLight"));
    KeyLight->SetupAttachment(SceneRoot);
    KeyLight->SetRelativeRotation(FRotator(-35.0, -35.0, 0.0));
    KeyLight->Intensity = 6.0f;
}

ACLifeDemoHost::~ACLifeDemoHost() = default;

void ACLifeDemoHost::BeginPlay()
{
    Super::BeginPlay();

    Camera->Activate(true);
    if (UWorld* World = GetWorld()) {
        for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator) {
            APlayerController* PlayerController = Iterator->Get();
            if (PlayerController != nullptr && PlayerController->IsLocalController()) {
                PlayerController->SetViewTarget(this);
                break;
            }
        }
    }

    Impl = MakeUnique<FImpl>();
    Impl->ObjectViews.emplace(Impl->Session.cell_object_id(), CellMesh);
    UE_LOG(LogCLifeAdapter, Display, TEXT("Created Cell ObjectId=%llu at tick=%llu Temperature=%.3f"),
           Impl->Session.cell_object_id().value, Impl->Session.tick(), Impl->Session.temperature());
    CreateDemoUI();
    ApplyRuntimeToViews();
}

void ACLifeDemoHost::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (DemoWidget.IsValid() && GEngine != nullptr && GEngine->GameViewport != nullptr) {
        GEngine->GameViewport->RemoveViewportWidgetContent(DemoWidget.ToSharedRef());
    }
    DemoWidget.Reset();
    Impl.Reset();
    Super::EndPlay(EndPlayReason);
}

void ACLifeDemoHost::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (Impl) {
        Impl->Session.advance_time(static_cast<double>(DeltaSeconds));
        ApplyRuntimeToViews();
    }
}

void ACLifeDemoHost::SetSimulationRunning(bool bRunning) { Impl->Session.set_running(bRunning); }
void ACLifeDemoHost::StepSimulation()
{
    Impl->Session.step();
    ApplyRuntimeToViews();
}
void ACLifeDemoHost::ResetSimulation()
{
    Impl->Session.reset();
    Impl->ObjectViews.clear();
    Impl->ObjectViews.emplace(Impl->Session.cell_object_id(), CellMesh);
    ApplyRuntimeToViews();
}
void ACLifeDemoHost::SetLightInput(double Value) { Impl->Session.set_light(Value); }
bool ACLifeDemoHost::IsSimulationRunning() const { return Impl && Impl->Session.running(); }
uint64 ACLifeDemoHost::GetSimulationTick() const { return Impl ? Impl->Session.tick() : 0; }
double ACLifeDemoHost::GetLightInput() const { return Impl ? Impl->Session.light() : 1.0; }
double ACLifeDemoHost::GetEnergy() const { return Impl ? Impl->Session.energy() : 0.0; }
double ACLifeDemoHost::GetUsedEnergy() const { return Impl ? Impl->Session.used_energy() : 0.0; }
double ACLifeDemoHost::GetTemperature() const { return Impl ? Impl->Session.temperature() : 0.2; }

void ACLifeDemoHost::ApplyRuntimeToViews()
{
    const float Scale = static_cast<float>(VisualScaleBase + Impl->Session.temperature());
    for (const auto& Entry : Impl->ObjectViews) {
        if (USceneComponent* View = Entry.second.Get()) {
            View->SetWorldScale3D(FVector(Scale));
        }
    }
}

void ACLifeDemoHost::CreateDemoUI()
{
    if (GEngine == nullptr || GEngine->GameViewport == nullptr) {
        return;
    }
    DemoWidget = SNew(SCLifeDemoPanel).Host(this);
    GEngine->GameViewport->AddViewportWidgetContent(DemoWidget.ToSharedRef(), 100);
}
