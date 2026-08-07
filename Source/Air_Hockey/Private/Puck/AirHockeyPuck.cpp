#include "AirHockeyPuck.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"

AAirHockeyPuck::AAirHockeyPuck()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	SetRootComponent(RootSceneComponent);

	PuckMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PuckMesh"));
	PuckMesh->SetupAttachment(RootSceneComponent);
	PuckMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); // We handle custom physics tracing!
}

void AAirHockeyPuck::BeginPlay()
{
	Super::BeginPlay();
}

void AAirHockeyPuck::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (HasAuthority())
	{
		UpdatePuckPhysics(DeltaTime);

		ServerPuckState.Position = GetActorLocation();
		ServerPuckState.Velocity = CurrentVelocity;
		ServerPuckState.TimeStamp = GetWorld()->GetTimeSeconds();
	}
}

void AAirHockeyPuck::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAirHockeyPuck, ServerPuckState);
}

void AAirHockeyPuck::LaunchPuck(const FVector& InitialVelocity)
{
	CurrentVelocity = InitialVelocity;
	CurrentVelocity.Z = 0.0f;
}

void AAirHockeyPuck::UpdatePuckPhysics(float DeltaTime)
{
	if (CurrentVelocity.IsNearlyZero()) return;

	FVector CurrentPos = GetActorLocation();
	FVector NextPos = CurrentPos + CurrentVelocity * DeltaTime;

	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	// Sweep sphere trace to detect wall or paddle collision
	bool bHit = GetWorld()->SweepSingleByChannel(
		HitResult,
		CurrentPos,
		NextPos,
		FQuat::Identity,
		ECC_WorldStatic,
		FCollisionShape::MakeSphere(PuckRadius),
		QueryParams
	);

	if (bHit)
	{
		// Set position to impact point
		SetActorLocation(HitResult.Location);

		// If hit wall: bounce with angle of incidence V_out = V_in - 2*(V_in . N)*N
		HandleWallBounce(HitResult.ImpactNormal);
	}
	else
	{
		SetActorLocation(NextPos);
	}
}

void AAirHockeyPuck::HandleWallBounce(const FVector& SurfaceNormal)
{
	// Formula: V_out = V_in - 2 * (V_in . N) * N
	FVector Normal2D = SurfaceNormal;
	Normal2D.Z = 0.0f;
	Normal2D.Normalize();

	CurrentVelocity = CurrentVelocity - 2.0f * (FVector::DotProduct(CurrentVelocity, Normal2D)) * Normal2D;
}

void AAirHockeyPuck::HandlePaddleHit(AActor* PaddleActor, const FVector& PaddleVelocity)
{
	// Requirement: Lose 80% initial kinetic energy -> magnitude scaled by sqrt(0.2) ≈ 0.447
	float ReducedSpeed = CurrentVelocity.Size() * FMath::Sqrt(0.2f);

	FVector HitDir = (GetActorLocation() - PaddleActor->GetActorLocation());
	HitDir.Z = 0.0f;
	HitDir.Normalize();

	FVector NewVel = HitDir * ReducedSpeed;

	// Add paddle momentum transfer if paddle velocity is significant
	if (PaddleVelocity.SizeSquared() > 100.0f)
	{
		NewVel += PaddleVelocity * 0.8f;
	}

	CurrentVelocity = NewVel;
}

void AAirHockeyPuck::OnRep_PuckState()
{
	if (!HasAuthority())
	{
		// Client linear / hermite interpolation for smooth puck visualization
		SetActorLocation(ServerPuckState.Position);
		CurrentVelocity = ServerPuckState.Velocity;
	}
}
