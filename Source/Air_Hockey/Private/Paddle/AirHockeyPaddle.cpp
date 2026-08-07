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
	FVector CurrentPos = GetActorLocation();
	FVector DesiredDelta = TargetLocation - CurrentPos;
	DesiredDelta.Z = 0.0f; // Keep on table plane

	FVector TargetVel = DesiredDelta / FMath::Max(DeltaTime, 0.0001f);
	TargetVel = TargetVel.GetClampedToMaxSize(MaxSpeed);

	// Smoothing (exponential interpolation to eliminate mouse jitter)
	CurrentVelocity = FMath::VInterpTo(CurrentVelocity, TargetVel, DeltaTime, SmoothingSpeed);

	FVector NewPos = CurrentPos + CurrentVelocity * DeltaTime;

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
