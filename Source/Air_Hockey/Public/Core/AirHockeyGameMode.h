#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "AirHockeyGameMode.generated.h"

UCLASS()
class AIR_HOCKEY_API AAirHockeyGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AAirHockeyGameMode();

	virtual void PostLogin(APlayerController* NewPlayer) override;
	
	void OnGoalScored(int32 ScoringPlayerId);

protected:
	UPROPERTY(EditDefaultsOnly, Category = "AirHockey|Rules")
	int32 MaxScoreToWin = 10;
};
