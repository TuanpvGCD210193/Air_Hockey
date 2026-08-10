#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "AirHockeyHUD.generated.h"

UCLASS()
class AIR_HOCKEY_API AAirHockeyHUD : public AHUD
{
	GENERATED_BODY()

public:
	AAirHockeyHUD();

	virtual void DrawHUD() override;
};
