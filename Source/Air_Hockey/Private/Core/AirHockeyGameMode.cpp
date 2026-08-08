#include "AirHockeyGameMode.h"
#include "AirHockeyGameState.h"
#include "AirHockeyPaddle.h"
#include "AirHockeyPuck.h"
#include "Kismet/GameplayStatics.h"

AAirHockeyGameMode::AAirHockeyGameMode()
{
	DefaultPawnClass = nullptr; // Prevent UE from spawning default pawns with unneeded camera components
	GameStateClass = AAirHockeyGameState::StaticClass();
	PaddleClass = AAirHockeyPaddle::StaticClass();
	PuckClass = AAirHockeyPuck::StaticClass();
}

void AAirHockeyGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Green, TEXT("--> AirHockeyGameMode Active & BeginPlay Running!"));
	}

	// Spawn Puck at table center if it doesn't exist yet
	if (!ActivePuck && PuckClass)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ActivePuck = GetWorld()->SpawnActor<AAirHockeyPuck>(PuckClass, FVector(0.0f, 0.0f, 35.0f), FRotator::ZeroRotator, SpawnParams);

		if (ActivePuck && GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Cyan, TEXT("--> ActivePuck Successfully Spawned at (0,0,15)!"));
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

			if (GEngine)
			{
				FString Msg = FString::Printf(TEXT("--> Player %d Paddle Spawned at %s and Possessed!"), PlayerIdx, *SpawnLoc.ToString());
				GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Yellow, Msg);
			}
		}
	}
}

void AAirHockeyGameMode::OnGoalScored(int32 ScoringPlayerId)
{
	AAirHockeyGameState* GS = GetGameState<AAirHockeyGameState>();
	if (GS && !GS->bIsGameOver)
	{
		GS->AddScore(ScoringPlayerId, 1);

		if (GS->bIsGameOver)
		{
			// Game Over reached! Stop Puck at center
			if (ActivePuck)
			{
				ActivePuck->ResetPuck(FVector::ZeroVector, FVector::ZeroVector);
			}
			return;
		}

		// Step 2.2: Reset Puck to center and launch service towards player who was scored on
		if (ActivePuck)
		{
			float LaunchX = (ScoringPlayerId == 1) ? 600.0f : -600.0f;
			float LaunchY = FMath::RandRange(-200.0f, 200.0f);
			FVector LaunchVel = FVector(LaunchX, LaunchY, 0.0f);

			ActivePuck->ResetPuck(FVector::ZeroVector, LaunchVel);
		}
	}
}
