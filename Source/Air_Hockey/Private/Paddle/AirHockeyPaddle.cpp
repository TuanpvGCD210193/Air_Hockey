#include "AirHockeyPaddle.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"

AAirHockeyPaddle::AAirHockeyPaddle()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	SetRootComponent(RootSceneComponent);

	PaddleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PaddleMesh"));
	PaddleMesh->SetupAttachment(RootSceneComponent);
	PaddleMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); // We handle collision manually via SweepTrace!
}

void AAirHockeyPaddle::BeginPlay()
{
	Super::BeginPlay();
}

void AAirHockeyPaddle::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AAirHockeyPaddle::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

void AAirHockeyPaddle::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAirHockeyPaddle, ServerState);
}

void AAirHockeyPaddle::PerformSweepMove(const FVector& TargetLocation, float DeltaTime, FVector& OutPosition, FVector& OutVelocity)
{
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

	if (bHit)
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
