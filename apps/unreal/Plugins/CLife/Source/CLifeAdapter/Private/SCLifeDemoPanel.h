#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class ACLifeDemoHost;

class SCLifeDemoPanel final : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCLifeDemoPanel) {}
        SLATE_ARGUMENT(TWeakObjectPtr<ACLifeDemoHost>, Host)
    SLATE_END_ARGS()

    void Construct(const FArguments& Arguments);

private:
    FReply OnPlay();
    FReply OnPause();
    FReply OnStep();
    FReply OnReset();
    void OnLightChanged(float Value);

    [[nodiscard]] FText TickText() const;
    [[nodiscard]] FText StatusText() const;
    [[nodiscard]] FText LightText() const;
    [[nodiscard]] FText EnergyText() const;
    [[nodiscard]] FText UsedEnergyText() const;
    [[nodiscard]] FText TemperatureText() const;
    [[nodiscard]] float LightValue() const;

    TWeakObjectPtr<ACLifeDemoHost> Host;
};
