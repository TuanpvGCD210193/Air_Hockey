#include "AirHockeyPuck.h"
#include "AirHockeyPaddle.h"
#include "AirHockeyGameMode.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

AAirHockeyPuck::AAirHockeyPuck()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	SetRootComponent(RootSceneComponent);

	PuckMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PuckMesh"));
	PuckMesh->SetupAttachment(RootSceneComponent);
	PuckMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); // We handle custom physics tracing!

	static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (DefaultMesh.Succeeded())
	{
		PuckMesh->SetStaticMesh(DefaultMesh.Object);
		PuckMesh->SetRelativeScale3D(FVector(0.5f, 0.5f, 0.1f));
	}
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
	else
	{
		// Step 3.4: Smooth Puck Entity Interpolation on Client side
		FVector CurrentPos = GetActorLocation();
		FVector TargetPos = ServerPuckState.Position + ServerPuckState.Velocity * DeltaTime;

		FVector InterpolatedPos = FMath::VInterpTo(CurrentPos, TargetPos, DeltaTime, 15.0f);
		SetActorLocation(InterpolatedPos);
		CurrentVelocity = ServerPuckState.Velocity;
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

void AAirHockeyPuck::ResetPuck(const FVector& NewLocation, const FVector& InitialVel)
{
	SetActorLocation(NewLocation);
	LaunchPuck(InitialVel);
}

void AAirHockeyPuck::UpdatePuckPhysics(float DeltaTime)
{
	if (CurrentVelocity.IsNearlyZero()) return;

	FVector CurrentPos = GetActorLocation();
	CurrentPos.Z = TableZHeight;
	FVector NextPos = CurrentPos + CurrentVelocity * DeltaTime;
	NextPos.Z = TableZHeight;

	// Step 1.5: Goal Detection Check
	float HalfLength = TableLength / 2.0f;
	float HalfGoalWidth = GoalWidth / 2.0f;

	// Puck crosses left goal line X < -HalfLength -> Player 2 Scores!
	if (NextPos.X < -HalfLength)
	{
		if (FMath::Abs(NextPos.Y) <= HalfGoalWidth)
		{
			AAirHockeyGameMode* GM = GetWorld()->GetAuthGameMode<AAirHockeyGameMode>();
			if (GM)
			{
				GM->OnGoalScored(2);
			}
			CurrentVelocity = FVector::ZeroVector;
			return;
		}
	}
	// Puck crosses right goal line X > HalfLength -> Player 1 Scores!
	else if (NextPos.X > HalfLength)
	{
		if (FMath::Abs(NextPos.Y) <= HalfGoalWidth)
		{
			AAirHockeyGameMode* GM = GetWorld()->GetAuthGameMode<AAirHockeyGameMode>();
			if (GM)
			{
				GM->OnGoalScored(1);
			}
			CurrentVelocity = FVector::ZeroVector;
			return;
		}
	}

	// Steps 1.3 & 1.4: Sphere Sweep Trace for Collision with Table Walls and Paddles
	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	bool bHit = GetWorld()->SweepSingleByChannel(
		HitResult,
		CurrentPos,
		NextPos,
		FQuat::Identity,
		ECC_WorldDynamic, // Check against dynamic actors (Paddles) first
		FCollisionShape::MakeSphere(PuckRadius),
		QueryParams
	);
	if (!bHit)
	{
		bHit = GetWorld()->SweepSingleByChannel(
			HitResult,
			CurrentPos,
			NextPos,
			FQuat::Identity,
			ECC_WorldStatic, // Check against static table walls
			FCollisionShape::MakeSphere(PuckRadius),
			QueryParams
		);
	}
	if (bHit)
	{
		FVector ImpactLocation = HitResult.Location;
		ImpactLocation.Z = TableZHeight;
		SetActorLocation(ImpactLocation);

		AActor* HitActor = HitResult.GetActor();
		AAirHockeyPaddle* Paddle = Cast<AAirHockeyPaddle>(HitActor);

		if (Paddle)
		{
			// Step 1.4: Hit Paddle Physics
			HandlePaddleHit(Paddle, Paddle->GetPaddleVelocity());
		}
		else
		{
			// Step 1.3: Hit Wall Reflection Physics
			HandleWallBounce(HitResult.ImpactNormal);
		}
	}
	else
	{
		// Manual boundary checks for top/bottom walls if static mesh collision isn't blocking
		float HalfWidth = TableWidth / 2.0f;
		if (FMath::Abs(NextPos.Y) >= (HalfWidth - PuckRadius))
		{
			FVector Normal = (NextPos.Y > 0) ? FVector(0.0f, -1.0f, 0.0f) : FVector(0.0f, 1.0f, 0.0f);
			HandleWallBounce(Normal);
			NextPos.Y = FMath::Clamp(NextPos.Y, -HalfWidth + PuckRadius, HalfWidth - PuckRadius);
		}
		// End wall bounces (left and right walls outside goal area)
		if (FMath::Abs(NextPos.X) >= (HalfLength - PuckRadius) && FMath::Abs(NextPos.Y) > HalfGoalWidth)
		{
			FVector Normal = (NextPos.X > 0) ? FVector(-1.0f, 0.0f, 0.0f) : FVector(1.0f, 0.0f, 0.0f);
			HandleWallBounce(Normal);
			NextPos.X = FMath::Clamp(NextPos.X, -HalfLength + PuckRadius, HalfLength - PuckRadius);
		}
		SetActorLocation(NextPos);
	}
}

void AAirHockeyPuck::HandleWallBounce(const FVector& SurfaceNormal)
{
	// Step 1.3: Formula V_out = V_in - 2 * (V_in . N) * N
	FVector Normal2D = FVector(SurfaceNormal.X, SurfaceNormal.Y, 0.0f).GetSafeNormal();
	if (Normal2D.IsNearlyZero()) return;

	CurrentVelocity = CurrentVelocity - 2.0f * (FVector::DotProduct(CurrentVelocity, Normal2D)) * Normal2D;
	CurrentVelocity.Z = 0.0f;
}

void AAirHockeyPuck::HandlePaddleHit(AActor* PaddleActor, const FVector& PaddleVelocity)
{
	// Step 1.4: Lose 80% initial kinetic energy -> magnitude scaled by sqrt(0.2) ≈ 0.4472
	float ReducedSpeed = CurrentVelocity.Size() * FMath::Sqrt(0.2f);

	FVector HitDir = (GetActorLocation() - PaddleActor->GetActorLocation());
	HitDir.Z = 0.0f;
	if (HitDir.IsNearlyZero())
	{
		HitDir = FVector(1.0f, 0.0f, 0.0f);
	}
	else
	{
		HitDir.Normalize();
	}

	FVector NewVel = HitDir * ReducedSpeed;

	// Add paddle momentum transfer if paddle velocity is significant
	FVector PaddleVel2D = FVector(PaddleVelocity.X, PaddleVelocity.Y, 0.0f);
	if (PaddleVel2D.SizeSquared() > 100.0f)
	{
		NewVel += PaddleVel2D * 0.8f;
	}

	CurrentVelocity = NewVel;
	CurrentVelocity.Z = 0.0f;
}

void AAirHockeyPuck::OnRep_PuckState()
{
	if (!HasAuthority())
	{
		SetActorLocation(ServerPuckState.Position);
		CurrentVelocity = ServerPuckState.Velocity;
	}
}
