#include "AirHockeyPuck.h"
#include "AirHockeyPaddle.h"
#include "AirHockeyGameMode.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/GameplayStatics.h"

static UStaticMesh* GetEngineCylinderMesh()
{
	static UStaticMesh* LoadedMesh = nullptr;
	if (LoadedMesh) return LoadedMesh;

	const TCHAR* Paths[] = {
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"),
		TEXT("/Engine/EngineMeshes/Cylinder.Cylinder"),
		TEXT("/Engine/EditorMeshes/EditorShapes/Shape_Cylinder.Shape_Cylinder"),
		TEXT("/Engine/BasicShapes/Cylinder")
	};

	for (const TCHAR* Path : Paths)
	{
		LoadedMesh = LoadObject<UStaticMesh>(nullptr, Path);
		if (LoadedMesh)
		{
			UE_LOG(LogTemp, Warning, TEXT("[AIR HOCKEY PUCK] Loaded Engine Cylinder Mesh from: %s"), Path);
			return LoadedMesh;
		}
	}
	return nullptr;
}

AAirHockeyPuck::AAirHockeyPuck()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	CollisionSphere->InitSphereRadius(PuckRadius);
	CollisionSphere->SetCollisionProfileName(TEXT("PhysicsActor"));
	SetRootComponent(CollisionSphere);

	PuckMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PuckMesh"));
	PuckMesh->SetupAttachment(CollisionSphere);
	PuckMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	UStaticMesh* MeshObj = GetEngineCylinderMesh();
	if (MeshObj)
	{
		PuckMesh->SetStaticMesh(MeshObj);
		PuckMesh->SetRelativeScale3D(FVector(0.8f, 0.8f, 0.25f));
	}
}

void AAirHockeyPuck::BeginPlay()
{
	Super::BeginPlay();

	if (PuckMesh && !PuckMesh->GetStaticMesh())
	{
		UStaticMesh* MeshObj = GetEngineCylinderMesh();
		if (MeshObj)
		{
			PuckMesh->SetStaticMesh(MeshObj);
			PuckMesh->SetRelativeScale3D(FVector(0.8f, 0.8f, 0.25f));
		}
	}
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
		// STEP 6.1: Run local client predicted physics for instant 0ms local response
		UpdatePuckPhysics(DeltaTime);

		// STEP 6.2: Smoothly reconcile local prediction toward Server State
		FVector CurrentPos = GetActorLocation();
		FVector TargetPos = ServerPuckState.Position + ServerPuckState.Velocity * DeltaTime;

		FVector InterpolatedPos = FMath::VInterpTo(CurrentPos, TargetPos, DeltaTime, 15.0f);
		SetActorLocation(InterpolatedPos);
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

	float HalfLength = TableLength / 2.0f;
	float HalfGoalWidth = GoalWidth / 2.0f;

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

	// Check 2D radius distance against all Paddles (handles both moving and stationary standing paddles)
	TArray<AActor*> FoundPaddles;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AAirHockeyPaddle::StaticClass(), FoundPaddles);
	for (AActor* PaddleActor : FoundPaddles)
	{
		AAirHockeyPaddle* Paddle = Cast<AAirHockeyPaddle>(PaddleActor);
		if (Paddle)
		{
			float Dist2D = FVector::Dist2D(NextPos, Paddle->GetActorLocation());
			if (Dist2D <= (PuckRadius + 60.0f)) // 40 + 60 = 100cm
			{
				HandlePaddleHit(Paddle, Paddle->GetPaddleVelocity());

				FVector HitDir = (NextPos - Paddle->GetActorLocation()).GetSafeNormal2D();
				if (HitDir.IsNearlyZero()) HitDir = FVector(1.0f, 0.0f, 0.0f);
				NextPos = Paddle->GetActorLocation() + HitDir * (PuckRadius + 60.0f + 1.0f);
				break;
			}
		}
	}

	float HalfWidth = TableWidth / 2.0f;
	if (FMath::Abs(NextPos.Y) >= (HalfWidth - PuckRadius))
	{
		FVector Normal = (NextPos.Y > 0) ? FVector(0.0f, -1.0f, 0.0f) : FVector(0.0f, 1.0f, 0.0f);
		HandleWallBounce(Normal);
		NextPos.Y = FMath::Clamp(NextPos.Y, -HalfWidth + PuckRadius, HalfWidth - PuckRadius);
	}
	if (FMath::Abs(NextPos.X) >= (HalfLength - PuckRadius) && FMath::Abs(NextPos.Y) > HalfGoalWidth)
	{
		FVector Normal = (NextPos.X > 0) ? FVector(-1.0f, 0.0f, 0.0f) : FVector(1.0f, 0.0f, 0.0f);
		HandleWallBounce(Normal);
		NextPos.X = FMath::Clamp(NextPos.X, -HalfLength + PuckRadius, HalfLength - PuckRadius);
	}
	SetActorLocation(NextPos);
}

void AAirHockeyPuck::HandleWallBounce(const FVector& SurfaceNormal)
{
	FVector Normal2D = FVector(SurfaceNormal.X, SurfaceNormal.Y, 0.0f).GetSafeNormal();
	if (Normal2D.IsNearlyZero()) return;

	CurrentVelocity = CurrentVelocity - 2.0f * (FVector::DotProduct(CurrentVelocity, Normal2D)) * Normal2D;
	CurrentVelocity.Z = 0.0f;
}

void AAirHockeyPuck::HandlePaddleHit(AActor* PaddleActor, const FVector& PaddleVelocity)
{
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

	FVector PaddleVel2D = FVector(PaddleVelocity.X, PaddleVelocity.Y, 0.0f);

	if (PaddleVel2D.SizeSquared() > 100.0f)
	{
		// Active Swing: Momentum transfer from mouse swing + directional impulse
		float SwingSpeed = PaddleVel2D.Size();
		CurrentVelocity = HitDir * (250.0f + SwingSpeed * 0.85f);
	}
	else
	{
		// Idle Bumper: Soft bounce off standing paddle (50% energy loss)
		float BounceSpeed = FMath::Max(CurrentVelocity.Size() * 0.5f, 150.0f);
		CurrentVelocity = HitDir * BounceSpeed;
	}

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
