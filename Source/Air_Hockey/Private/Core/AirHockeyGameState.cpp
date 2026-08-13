#include "AirHockeyGameState.h"
#include "Net/UnrealNetwork.h"

AAirHockeyGameState::AAirHockeyGameState()
{
	bReplicates = true;
}

void AAirHockeyGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AAirHockeyGameState, Player1Score);
	DOREPLIFETIME(AAirHockeyGameState, Player2Score);
	DOREPLIFETIME(AAirHockeyGameState, WinningPlayerId);
	DOREPLIFETIME(AAirHockeyGameState, bIsGameOver);
}

void AAirHockeyGameState::AddScore(int32 PlayerId, int32 Amount)
{
	if (HasAuthority())
	{
		if (PlayerId == 1)
		{
			Player1Score += Amount;
			if (Player1Score >= 10)
			{
				bIsGameOver = true;
				WinningPlayerId = 1;
			}
		}
		else if (PlayerId == 2)
		{
			Player2Score += Amount;
			if (Player2Score >= 10)
			{
				bIsGameOver = true;
				WinningPlayerId = 2;
			}
		}
	}
}

void AAirHockeyGameState::Multicast_OnGoalScored_Implementation(int32 ScoringPlayerId, int32 NewP1Score, int32 NewP2Score)
{
	Player1Score = NewP1Score;
	Player2Score = NewP2Score;
}
