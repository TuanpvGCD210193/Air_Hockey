#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "AirHockeyGameState.generated.h"

UCLASS()
class AIR_HOCKEY_API AAirHockeyGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	AAirHockeyGameState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void AddScore(int32 PlayerId, int32 Amount);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_OnGoalScored(int32 ScoringPlayerId, int32 NewP1Score, int32 NewP2Score);

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "AirHockey|Score")
	int32 Player1Score = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "AirHockey|Score")
	int32 Player2Score = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "AirHockey|Score")
	int32 WinningPlayerId = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "AirHockey|State")
	bool bIsGameOver = false;
};
