#include "AirHockeyPaddle.h"
#include "AirHockeyPlayerController.h"
#include "AirHockeyGameState.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

AAirHockeyPaddle::AAirHockeyPaddle()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	SetRootComponent(RootSceneComponent);

	PaddleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PaddleMesh"));
	PaddleMesh->SetupAttachment(RootSceneComponent);
	PaddleMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); // We handle collision manually via SweepTrace!

	static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (DefaultMesh.Succeeded())
	{
		PaddleMesh->SetStaticMesh(DefaultMesh.Object);
		PaddleMesh->SetRelativeScale3D(FVector(1.2f, 1.2f, 0.4f)); // Thicker and larger disc
	}
}

void AAirHockeyPaddle::BeginPlay()
{
	Super::BeginPlay();

	// Create dynamic material to color Player 1 Red and Player 2 Blue so they pop out on white table
	if (PaddleMesh)
	{
		UMaterialInterface* BaseMat = PaddleMesh->GetMaterial(0);
		if (BaseMat)
		{
			UMaterialInstanceDynamic* DynMat = PaddleMesh->CreateDynamicMaterialInstance(0, BaseMat);
			if (DynMat)
			{
				FLinearColor Color = (PlayerIndex == 1) ? FLinearColor::Red : FLinearColor::Blue;
				DynMat->SetVectorParameterValue(TEXT("Color"), Color);
				DynMat->SetVectorParameterValue(TEXT("BaseColor"), Color);
			}
		}
	}
}

void AAirHockeyPaddle::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Step 3.1: Perform Client-Side Prediction for locally controlled Paddle
	if (IsLocallyControlled())
	{
		APlayerController* PC = Cast<APlayerController>(GetController());
		if (PC)
		{
			FVector TargetInput = FVector::ZeroVector;
			FVector WorldLocation, WorldDirection;

			if (PC->DeprojectMousePositionToWorld(WorldLocation, WorldDirection))
			{
				if (!FMath::IsNearlyZero(WorldDirection.Z))
				{
					float T = (TableZHeight - WorldLocation.Z) / WorldDirection.Z;
					TargetInput = WorldLocation + WorldDirection * T;
				}
			}

			// 1. Predict position locally immediately
			FVector NewPos, NewVel;
			PerformSweepMove(TargetInput, DeltaTime, NewPos, NewVel);
			SetActorLocation(NewPos);

			// 2. Package move with incrementing sequence number and store in UnacknowledgedMoves buffer
			CurrentSequenceNumber++;
			FPaddleMove Move;
			Move.TimeStamp = GetWorld()->GetTimeSeconds();
			Move.TargetInputPosition = TargetInput;
			Move.CalculatedVelocity = NewVel;
			Move.SequenceNumber = CurrentSequenceNumber;

			UnacknowledgedMoves.Add(Move);

			// 3. Send RPC to Server for validation
			Server_SendMove(Move);

			if (GEngine)
			{
				FString DebugMsg = FString::Printf(TEXT("[PADDLE TICK] P%d Pos: %s | MouseTarget: %s"), 
					PlayerIndex, *NewPos.ToCompactString(), *TargetInput.ToCompactString());
				GEngine->AddOnScreenDebugMessage(100 + PlayerIndex, 0.0f, FColor::Green, DebugMsg);
			}
		}
		else
		{
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(100, 0.0f, FColor::Red, TEXT("[WARNING] Paddle Has NO Controller!"));
			}
		}
	}
}

void AAirHockeyPaddle::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

void AAirHockeyPaddle::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAirHockeyPaddle, ServerState);
	DOREPLIFETIME(AAirHockeyPaddle, PlayerIndex);
}

void AAirHockeyPaddle::PerformSweepMove(const FVector& TargetLocation, float DeltaTime, FVector& OutPosition, FVector& OutVelocity)
{
	// Freeze Paddle movement if Game Over has been reached
	AAirHockeyGameState* GS = GetWorld() ? GetWorld()->GetGameState<AAirHockeyGameState>() : nullptr;
	if (GS && GS->bIsGameOver)
	{
		OutPosition = GetActorLocation();
		OutVelocity = FVector::ZeroVector;
		return;
	}
	FVector ClampedTarget = TargetLocation;
	ClampedTarget.Z = TableZHeight;

	// Step 1.1: Clamp Target Position within Player's side of the table
	float HalfLength = TableLength / 2.0f;
	float HalfWidth = TableWidth / 2.0f;

	if (PlayerIndex == 1)
	{
		// Player 1 restricted to Left Side (X <= 0)
		ClampedTarget.X = FMath::Clamp(ClampedTarget.X, -HalfLength + PaddleRadius, 0.0f - PaddleRadius);
	}
	else
	{
		// Player 2 restricted to Right Side (X >= 0)
		ClampedTarget.X = FMath::Clamp(ClampedTarget.X, 0.0f + PaddleRadius, HalfLength - PaddleRadius);
	}
	ClampedTarget.Y = FMath::Clamp(ClampedTarget.Y, -HalfWidth + PaddleRadius, HalfWidth - PaddleRadius);

	FVector CurrentPos = GetActorLocation();
	CurrentPos.Z = TableZHeight;

	FVector DesiredDelta = ClampedTarget - CurrentPos;
	DesiredDelta.Z = 0.0f; // Keep on table plane

	FVector TargetVel = DesiredDelta / FMath::Max(DeltaTime, 0.0001f);
	TargetVel = TargetVel.GetClampedToMaxSize(MaxSpeed);

	// Step 1.2: Exponential velocity smoothing (eliminates mouse jitter)
	CurrentVelocity = FMath::VInterpTo(CurrentVelocity, TargetVel, DeltaTime, SmoothingSpeed);

	FVector NewPos = CurrentPos + CurrentVelocity * DeltaTime;

	// Clamp NewPos again to guarantee bounds
	if (PlayerIndex == 1)
	{
		NewPos.X = FMath::Clamp(NewPos.X, -HalfLength + PaddleRadius, 0.0f - PaddleRadius);
	}
	else
	{
		NewPos.X = FMath::Clamp(NewPos.X, 0.0f + PaddleRadius, HalfLength - PaddleRadius);
	}
	NewPos.Y = FMath::Clamp(NewPos.Y, -HalfWidth + PaddleRadius, HalfWidth - PaddleRadius);
	NewPos.Z = TableZHeight;

	// Perform SweepTrace against table boundary walls
	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	bool bHit = GetWorld()->SweepSingleByChannel(
		HitResult,
		CurrentPos,
		NewPos,
		FQuat::Identity,
		ECC_WorldStatic,
		FCollisionShape::MakeSphere(PaddleRadius),
		QueryParams
	);

	// Only process wall hits (ignore horizontal floor surface hits where ImpactNormal.Z is high)
	if (bHit && FMath::Abs(HitResult.ImpactNormal.Z) < 0.5f)
	{
		NewPos = HitResult.Location;
		NewPos.Z = TableZHeight;
		CurrentVelocity = FVector::ZeroVector;
	}

	OutPosition = NewPos;
	OutVelocity = CurrentVelocity;
}

void AAirHockeyPaddle::Server_SendMove_Implementation(FPaddleMove Move)
{
	FVector NewPosition, NewVelocity;
	PerformSweepMove(Move.TargetInputPosition, 1.0f / 60.0f, NewPosition, NewVelocity);
	SetActorLocation(NewPosition);

	ServerState.Position = NewPosition;
	ServerState.Velocity = NewVelocity;
	ServerState.LastProcessedSequenceNumber = Move.SequenceNumber;

	Client_ReconcileState(ServerState);
}

bool AAirHockeyPaddle::Server_SendMove_Validate(FPaddleMove Move)
{
	return true;
}

void AAirHockeyPaddle::Client_ReconcileState_Implementation(FPaddleState AuthoritativeState)
{
	if (IsLocallyControlled())
	{
		UnacknowledgedMoves.RemoveAll([AuthoritativeState](const FPaddleMove& Move) {
			return Move.SequenceNumber <= AuthoritativeState.LastProcessedSequenceNumber;
		});

		SetActorLocation(AuthoritativeState.Position);
		CurrentVelocity = AuthoritativeState.Velocity;

		// Re-simulate remaining unacknowledged moves
		for (const FPaddleMove& Move : UnacknowledgedMoves)
		{
			FVector Pos, Vel;
			PerformSweepMove(Move.TargetInputPosition, 1.0f / 60.0f, Pos, Vel);
			SetActorLocation(Pos);
		}
	}
}

void AAirHockeyPaddle::OnRep_ServerState()
{
	if (!IsLocallyControlled())
	{
		SetActorLocation(ServerState.Position);
	}
}
