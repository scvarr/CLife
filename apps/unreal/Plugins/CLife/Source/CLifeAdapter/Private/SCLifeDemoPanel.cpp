#include "SCLifeDemoPanel.h"

#include "CLifeDemoHost.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#include <clife/presets/first_world.hpp>

#include <string_view>

namespace {

FString FromUtf8(std::string_view Text)
{
    const FUTF8ToTCHAR Converted{Text.data(), static_cast<int32>(Text.size())};
    return FString{Converted.Length(), Converted.Get()};
}

TSharedRef<SWidget> Section(const TCHAR* Heading, const FString& Body)
{
    return SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f)
          [SNew(STextBlock).Text(FText::FromString(Heading))]
        + SVerticalBox::Slot().AutoHeight()
          [SNew(STextBlock).Text(FText::FromString(Body)).AutoWrapText(true)];
}

} // namespace

void SCLifeDemoPanel::Construct(const FArguments& Arguments)
{
    Host = Arguments._Host;
    ChildSlot
    [
        SNew(SOverlay)
        + SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Top).Padding(20.0f, 12.0f)
          [SNew(STextBlock).Text(FText::FromString(TEXT("CLife / simulation controls")))]
        + SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill).Padding(20.0f, 52.0f, 20.0f, 105.0f)
          [
              SNew(SHorizontalBox)
              + SHorizontalBox::Slot().AutoWidth()
                [
                    SNew(SBox).WidthOverride(320.0f)
                    [
                        SNew(SBorder).Padding(12.0f)
                        [
                            SNew(SVerticalBox)
                            + SVerticalBox::Slot().AutoHeight()[Section(TEXT("World"), TEXT("First cellular preset\nObjectId -> Unreal component"))]
                            + SVerticalBox::Slot().AutoHeight()[Section(TEXT("Values"), FromUtf8(clife::presets::kValuesSummary))]
                            + SVerticalBox::Slot().AutoHeight()[Section(TEXT("Genome"), FromUtf8(clife::presets::kGenomeSummary))]
                            + SVerticalBox::Slot().AutoHeight()[Section(TEXT("World Rule"), FromUtf8(clife::presets::kWorldRuleSummary))]
                            + SVerticalBox::Slot().AutoHeight()[Section(TEXT("Bindings"), FromUtf8(clife::presets::kBindingSummary))]
                        ]
                    ]
                ]
              + SHorizontalBox::Slot().FillWidth(1.0f)[SNullWidget::NullWidget]
              + SHorizontalBox::Slot().AutoWidth()
                [
                    SNew(SBox).WidthOverride(290.0f)
                    [
                        SNew(SBorder).Padding(12.0f)
                        [
                            SNew(SVerticalBox)
                            + SVerticalBox::Slot().AutoHeight()[Section(TEXT("Inspector"), TEXT("Current CLife values"))]
                            + SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(this, &SCLifeDemoPanel::LightText)]
                            + SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(this, &SCLifeDemoPanel::EnergyText)]
                            + SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(this, &SCLifeDemoPanel::UsedEnergyText)]
                            + SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(this, &SCLifeDemoPanel::TemperatureText)]
                            + SVerticalBox::Slot().AutoHeight()[Section(TEXT("Visualization"), TEXT("scale = 1.0 + Temperature"))]
                        ]
                    ]
                ]
          ]
        + SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Bottom).Padding(20.0f, 18.0f)
          [
              SNew(SBorder).Padding(10.0f)
              [
                  SNew(SHorizontalBox)
                  + SHorizontalBox::Slot().AutoWidth().Padding(3.0f)[SNew(SButton).Text(FText::FromString(TEXT("Play"))).OnClicked(this, &SCLifeDemoPanel::OnPlay)]
                  + SHorizontalBox::Slot().AutoWidth().Padding(3.0f)[SNew(SButton).Text(FText::FromString(TEXT("Pause"))).OnClicked(this, &SCLifeDemoPanel::OnPause)]
                  + SHorizontalBox::Slot().AutoWidth().Padding(3.0f)[SNew(SButton).Text(FText::FromString(TEXT("Step"))).OnClicked(this, &SCLifeDemoPanel::OnStep)]
                  + SHorizontalBox::Slot().AutoWidth().Padding(3.0f)[SNew(SButton).Text(FText::FromString(TEXT("Reset"))).OnClicked(this, &SCLifeDemoPanel::OnReset)]
                  + SHorizontalBox::Slot().AutoWidth().Padding(15.0f, 3.0f)[SNew(STextBlock).Text(this, &SCLifeDemoPanel::TickText)]
                  + SHorizontalBox::Slot().AutoWidth().Padding(8.0f, 3.0f)[SNew(STextBlock).Text(FText::FromString(TEXT("Light")))]
                  + SHorizontalBox::Slot().FillWidth(1.0f).Padding(8.0f, 3.0f)
                    [SNew(SSlider).Value(this, &SCLifeDemoPanel::LightValue).OnValueChanged(this, &SCLifeDemoPanel::OnLightChanged)]
                  + SHorizontalBox::Slot().AutoWidth().Padding(15.0f, 3.0f)[SNew(STextBlock).Text(this, &SCLifeDemoPanel::StatusText)]
              ]
          ]
    ];
}

FReply SCLifeDemoPanel::OnPlay() { if (Host.IsValid()) Host->SetSimulationRunning(true); return FReply::Handled(); }
FReply SCLifeDemoPanel::OnPause() { if (Host.IsValid()) Host->SetSimulationRunning(false); return FReply::Handled(); }
FReply SCLifeDemoPanel::OnStep() { if (Host.IsValid()) Host->StepSimulation(); return FReply::Handled(); }
FReply SCLifeDemoPanel::OnReset() { if (Host.IsValid()) Host->ResetSimulation(); return FReply::Handled(); }
void SCLifeDemoPanel::OnLightChanged(float Value) { if (Host.IsValid()) Host->SetLightInput(Value * 2.0); }

FText SCLifeDemoPanel::TickText() const { return FText::FromString(FString::Printf(TEXT("Tick: %llu"), Host.IsValid() ? Host->GetSimulationTick() : 0)); }
FText SCLifeDemoPanel::StatusText() const { return FText::FromString(Host.IsValid() && Host->IsSimulationRunning() ? TEXT("Running") : TEXT("Paused")); }
FText SCLifeDemoPanel::LightText() const { return FText::FromString(FString::Printf(TEXT("Light: %.3f"), Host.IsValid() ? Host->GetLightInput() : 0.0)); }
FText SCLifeDemoPanel::EnergyText() const { return FText::FromString(FString::Printf(TEXT("Energy: %.3f"), Host.IsValid() ? Host->GetEnergy() : 0.0)); }
FText SCLifeDemoPanel::UsedEnergyText() const { return FText::FromString(FString::Printf(TEXT("UsedEnergy: %.3f"), Host.IsValid() ? Host->GetUsedEnergy() : 0.0)); }
FText SCLifeDemoPanel::TemperatureText() const { return FText::FromString(FString::Printf(TEXT("Temperature: %.3f"), Host.IsValid() ? Host->GetTemperature() : 0.0)); }
float SCLifeDemoPanel::LightValue() const { return Host.IsValid() ? static_cast<float>(Host->GetLightInput() / 2.0) : 0.5f; }
