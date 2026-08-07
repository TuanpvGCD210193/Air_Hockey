#include "AirHockeyGameMode.h"
#include "AirHockeyGameState.h"

AAirHockeyGameMode::AAirHockeyGameMode()
{
	GameStateClass = AAirHockeyGameState::StaticClass();
}

void AAirHockeyGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
}

void AAirHockeyGameMode::OnGoalScored(int32 ScoringPlayerId)
{
	AAirHockeyGameState* GS = GetGameState<AAirHockeyGameState>();
	if (GS)
	{
		GS->AddScore(ScoringPlayerId, 1);
	}
}
