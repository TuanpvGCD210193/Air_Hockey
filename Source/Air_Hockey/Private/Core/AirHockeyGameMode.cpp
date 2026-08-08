#include "AirHockeyGameMode.h"
#include "AirHockeyGameState.h"
#include "AirHockeyPaddle.h"
#include "AirHockeyPuck.h"
#include "Kismet/GameplayStatics.h"

AAirHockeyGameMode::AAirHockeyGameMode()
{
	GameStateClass = AAirHockeyGameState::StaticClass();
	PaddleClass = AAirHockeyPaddle::StaticClass();
	PuckClass = AAirHockeyPuck::StaticClass();
}

void AAirHockeyGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	ConnectedPlayersCount++;

	// Determine spawn location and PlayerIndex based on join order
	int32 PlayerIdx = ConnectedPlayersCount;
	FVector SpawnLoc = (PlayerIdx == 1) ? Player1SpawnLocation : Player2SpawnLocation;

	if (PaddleClass)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AAirHockeyPaddle* NewPaddle = GetWorld()->SpawnActor<AAirHockeyPaddle>(PaddleClass, SpawnLoc, FRotator::ZeroRotator, SpawnParams);
		if (NewPaddle)
		{
			NewPaddle->SetPlayerIndex(PlayerIdx);
			NewPlayer->Possess(NewPaddle);
		}
	}

	// Spawn Puck at table center if it doesn't exist yet
	if (!ActivePuck && PuckClass)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ActivePuck = GetWorld()->SpawnActor<AAirHockeyPuck>(PuckClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	}
}

void AAirHockeyGameMode::OnGoalScored(int32 ScoringPlayerId)
{
	AAirHockeyGameState* GS = GetGameState<AAirHockeyGameState>();
	if (GS)
	{
		GS->AddScore(ScoringPlayerId, 1);
	}
}
