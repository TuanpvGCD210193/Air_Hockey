#include "AirHockeyGameMode.h"
#include "AirHockeyGameState.h"
#include "AirHockeyPaddle.h"
#include "AirHockeyPuck.h"
#include "UI/AirHockeyHUD.h"
#include "Kismet/GameplayStatics.h"

AAirHockeyGameMode::AAirHockeyGameMode()
{
	DefaultPawnClass = nullptr; // Prevent UE from spawning default pawns with unneeded camera components
	GameStateClass = AAirHockeyGameState::StaticClass();
	HUDClass = AAirHockeyHUD::StaticClass();
	PaddleClass = AAirHockeyPaddle::StaticClass();
	PuckClass = AAirHockeyPuck::StaticClass();
}

void AAirHockeyGameMode::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogTemp, Warning, TEXT("[AIR HOCKEY GAMEMODE] GameMode Active & BeginPlay Running!"));

	// Spawn Puck at table center if it doesn't exist yet
	if (!ActivePuck && PuckClass)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ActivePuck = GetWorld()->SpawnActor<AAirHockeyPuck>(PuckClass, FVector(0.0f, 0.0f, 35.0f), FRotator::ZeroRotator, SpawnParams);

		if (ActivePuck)
		{
			UE_LOG(LogTemp, Warning, TEXT("[AIR HOCKEY GAMEMODE] ActivePuck Successfully Spawned at (0, 0, 35)!"));
		}
	}

	// Spawn Paddles for any PlayerControllers already present at BeginPlay
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (PC && !PC->GetPawn())
		{
			SpawnPaddleForPlayer(PC);
		}
	}
}

void AAirHockeyGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	if (NewPlayer && !NewPlayer->GetPawn())
	{
		SpawnPaddleForPlayer(NewPlayer);
	}
}

void AAirHockeyGameMode::SpawnPaddleForPlayer(APlayerController* NewPlayer)
{
	ConnectedPlayersCount++;
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

			UE_LOG(LogTemp, Warning, TEXT("[AIR HOCKEY GAMEMODE] Player %d Paddle Spawned at %s and Possessed!"), PlayerIdx, *SpawnLoc.ToString());
		}
	}
}

void AAirHockeyGameMode::OnGoalScored(int32 ScoringPlayerId)
{
	AAirHockeyGameState* GS = GetGameState<AAirHockeyGameState>();
	if (GS && !GS->bIsGameOver)
	{
		GS->AddScore(ScoringPlayerId, 1);

		UE_LOG(LogTemp, Warning, TEXT("[STEP 4.1 DEBUG] GOAL SCORED BY PLAYER %d! Score: Player 1 [%d] - [%d] Player 2"), 
			ScoringPlayerId, GS->Player1Score, GS->Player2Score);

		if (GS->bIsGameOver)
		{
			// STEP 4.2: Game Over reached! Stop Puck at center and declare Champion
			if (ActivePuck)
			{
				ActivePuck->ResetPuck(FVector(0.0f, 0.0f, 35.0f), FVector::ZeroVector);
			}

			UE_LOG(LogTemp, Warning, TEXT("[STEP 4.2 DEBUG] MATCH OVER! PLAYER %d IS THE CHAMPION! Final Score: Player 1 [%d] - [%d] Player 2"), 
				GS->WinningPlayerId, GS->Player1Score, GS->Player2Score);
			return;
		}

		// STEP 4.1: Reset Puck to table center (0, 0, 35) and launch service towards victim (player who was scored ON)
		if (ActivePuck)
		{
			float LaunchX = (ScoringPlayerId == 1) ? 500.0f : -500.0f;
			float LaunchY = FMath::RandRange(-150.0f, 150.0f);
			FVector LaunchVel = FVector(LaunchX, LaunchY, 0.0f);

			ActivePuck->ResetPuck(FVector(0.0f, 0.0f, 35.0f), LaunchVel);
		}
	}
}
