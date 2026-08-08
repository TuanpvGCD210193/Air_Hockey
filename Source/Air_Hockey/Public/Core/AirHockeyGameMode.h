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

	virtual void BeginPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	
	void SpawnPaddleForPlayer(APlayerController* NewPlayer);
	void OnGoalScored(int32 ScoringPlayerId);

protected:
	UPROPERTY(EditDefaultsOnly, Category = "AirHockey|Classes")
	TSubclassOf<class AAirHockeyPaddle> PaddleClass;

	UPROPERTY(EditDefaultsOnly, Category = "AirHockey|Classes")
	TSubclassOf<class AAirHockeyPuck> PuckClass;

	UPROPERTY(EditDefaultsOnly, Category = "AirHockey|Rules")
	int32 MaxScoreToWin = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AirHockey|Spawning")
	FVector Player1SpawnLocation = FVector(-400.0f, 0.0f, 35.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AirHockey|Spawning")
	FVector Player2SpawnLocation = FVector(400.0f, 0.0f, 35.0f);

	UPROPERTY(Transient)
	class AAirHockeyPuck* ActivePuck;

	UPROPERTY(Transient)
	int32 ConnectedPlayersCount = 0;
};
