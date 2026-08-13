#include "AirHockeyPuck.h"
#include "AirHockeyPaddle.h"
#include "AirHockeyGameMode.h"
#include "AirHockeyGameState.h"
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

	// Ensure Puck is placed at Z = 35.0f plane and fully visible
	FVector CurrentLoc = GetActorLocation();
	CurrentLoc.Z = TableZHeight;
	SetActorLocation(CurrentLoc);

	// STEP 34.2: Instant Network Warmup & Pre-Buffer Flush Engine for Puck (0s Warmup Lag)
	float InitialTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	PuckSnapshotBuffer.Empty();
	PuckSnapshotBuffer.Add(FPuckWorldSnapshot(CurrentLoc, FVector::ZeroVector, InitialTime));
	PuckSnapshotBuffer.Add(FPuckWorldSnapshot(CurrentLoc, FVector::ZeroVector, InitialTime + 0.010f));
	PuckSnapshotBuffer.Add(FPuckWorldSnapshot(CurrentLoc, FVector::ZeroVector, InitialTime + 0.020f));
	PuckClientPlaybackTime = InitialTime;
	bIsPuckJitterBufferInitialized = true;

	if (PuckMesh)
	{
		PuckMesh->SetVisibility(true);
		if (!PuckMesh->GetStaticMesh())
		{
			UStaticMesh* MeshObj = GetEngineCylinderMesh();
			if (MeshObj)
			{
				PuckMesh->SetStaticMesh(MeshObj);
				PuckMesh->SetRelativeScale3D(FVector(0.8f, 0.8f, 0.25f));
			}
		}
	}
}

void AAirHockeyPuck::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (HasAuthority())
	{
		UpdatePuckPhysics(DeltaTime);

		float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
		WorldHistoryBuffer.Add(FPuckWorldSnapshot(GetActorLocation(), CurrentVelocity, CurrentTime));
		if (WorldHistoryBuffer.Num() > 60)
		{
			WorldHistoryBuffer.RemoveAt(0);
		}

		ServerPuckState.Position = GetActorLocation();
		ServerPuckState.Velocity = CurrentVelocity;
		ServerPuckState.TimeStamp = CurrentTime;

		PuckSampleTimer += DeltaTime;
		if (PuckSampleTimer >= 0.010f)
		{
			PuckSampleTimer = 0.0f;
			LocalPuckSampleBuffer.Add(FPuck10msSample(GetActorLocation(), CurrentVelocity, CurrentTime));
			if (LocalPuckSampleBuffer.Num() > 4)
			{
				LocalPuckSampleBuffer.RemoveAt(0);
			}

			FPuck10msPacket Packet;
			Packet.RedundantSamples = LocalPuckSampleBuffer;

			Client_ReceivePuck10msPacket(Packet);
		}
	}
	else
	{
		if (PuckSnapshotBuffer.Num() >= 2)
		{
			if (!bIsPuckJitterBufferInitialized)
			{
				PuckClientPlaybackTime = PuckSnapshotBuffer[0].TimeStamp;
				bIsPuckJitterBufferInitialized = true;
			}

			float LatestTime = PuckSnapshotBuffer.Last().TimeStamp;
			float CurrentBufferDelay = LatestTime - PuckClientPlaybackTime;

			// STEP 29.1: Hard Resync Gate - If delay exceeds 200ms (idle time or hit jump), hard resync playback clock instantly!
			if (CurrentBufferDelay > 0.200f || CurrentBufferDelay < 0.0f)
			{
				PuckClientPlaybackTime = LatestTime - TargetPuckJitterDelay;
				CurrentBufferDelay = TargetPuckJitterDelay;
			}

			if (CurrentBufferDelay > (TargetPuckJitterDelay + 0.03f))
			{
				PuckAdaptivePlaybackRate = 1.10f;
			}
			else if (CurrentBufferDelay < (TargetPuckJitterDelay - 0.03f))
			{
				PuckAdaptivePlaybackRate = 0.90f;
			}
			else
			{
				PuckAdaptivePlaybackRate = 1.0f;
			}

			PuckClientPlaybackTime += DeltaTime * PuckAdaptivePlaybackRate;

			int32 SnapIdx0 = -1;
			int32 SnapIdx1 = -1;

			for (int32 i = 0; i < PuckSnapshotBuffer.Num() - 1; ++i)
			{
				if (PuckSnapshotBuffer[i].TimeStamp <= PuckClientPlaybackTime && PuckSnapshotBuffer[i + 1].TimeStamp >= PuckClientPlaybackTime)
				{
					SnapIdx0 = i;
					SnapIdx1 = i + 1;
					break;
				}
			}

			if (SnapIdx0 != -1 && SnapIdx1 != -1)
			{
				const FPuckWorldSnapshot& Snap0 = PuckSnapshotBuffer[SnapIdx0];
				const FPuckWorldSnapshot& Snap1 = PuckSnapshotBuffer[SnapIdx1];

				float TimeGap = FMath::Max(Snap1.TimeStamp - Snap0.TimeStamp, 0.001f);
				float Alpha = FMath::Clamp((PuckClientPlaybackTime - Snap0.TimeStamp) / TimeGap, 0.0f, 1.0f);

				FVector P0 = Snap0.Position;
				FVector T0 = Snap0.Velocity * TimeGap;
				FVector P1 = Snap1.Position;
				FVector T1 = Snap1.Velocity * TimeGap;

				FVector SmoothCurvedPos = FMath::CubicInterp(P0, T0, P1, T1, Alpha);
				FVector UltraSmoothPos = FMath::VInterpTo(GetActorLocation(), SmoothCurvedPos, DeltaTime, 45.0f);

				SetActorLocation(UltraSmoothPos);
				CurrentVelocity = FMath::Lerp(Snap0.Velocity, Snap1.Velocity, Alpha);
			}
			else if (PuckClientPlaybackTime > LatestTime)
			{
				const FPuckWorldSnapshot& LatestSnap = PuckSnapshotBuffer.Last();
				FVector ExtrapolatedPos = LatestSnap.Position + LatestSnap.Velocity * (PuckClientPlaybackTime - LatestTime);
				FVector SmoothExtrapolated = FMath::VInterpTo(GetActorLocation(), ExtrapolatedPos, DeltaTime, 15.0f);

				SetActorLocation(SmoothExtrapolated);
				CurrentVelocity = LatestSnap.Velocity;
			}

			PuckSnapshotBuffer.RemoveAll([this](const FPuckWorldSnapshot& Snap) {
				return Snap.TimeStamp < (PuckClientPlaybackTime - 1.0f);
			});
		}
		else if (!ServerPuckState.Position.IsZero())
		{
			FVector InterpPos = FMath::VInterpTo(GetActorLocation(), ServerPuckState.Position, DeltaTime, 45.0f);
			SetActorLocation(InterpPos);
			CurrentVelocity = ServerPuckState.Velocity;
		}
	}
}

void AAirHockeyPuck::StartClientPrediction()
{
	bIsClientPredictingPuck = true;
	ClientPredictionTimer = 0.0f;
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

	// STEP 29.1: Reset Puck Snapshot Buffer to synchronize clients instantly on goal / reset
	PuckSnapshotBuffer.Empty();
	bIsPuckJitterBufferInitialized = false;
	bIsClientPredictingPuck = false;
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
			if (HasAuthority())
			{
				AAirHockeyGameMode* GM = GetWorld()->GetAuthGameMode<AAirHockeyGameMode>();
				if (GM)
				{
					GM->OnGoalScored(2);
				}
			}
			else
			{
				// STEP 34.1: Option B - Client 0ms Local Goal Prediction
				AAirHockeyGameState* GS = GetWorld() ? GetWorld()->GetGameState<AAirHockeyGameState>() : nullptr;
				if (GS)
				{
					GS->Player2Score += 1;
				}
			}
			CurrentVelocity = FVector::ZeroVector;
			return;
		}
	}
	else if (NextPos.X > HalfLength)
	{
		if (FMath::Abs(NextPos.Y) <= HalfGoalWidth)
		{
			if (HasAuthority())
			{
				AAirHockeyGameMode* GM = GetWorld()->GetAuthGameMode<AAirHockeyGameMode>();
				if (GM)
				{
					GM->OnGoalScored(1);
				}
			}
			else
			{
				// STEP 34.1: Option B - Client 0ms Local Goal Prediction
				AAirHockeyGameState* GS = GetWorld() ? GetWorld()->GetGameState<AAirHockeyGameState>() : nullptr;
				if (GS)
				{
					GS->Player1Score += 1;
				}
			}
			CurrentVelocity = FVector::ZeroVector;
			return;
		}
	}

	// STEP 32.1: Universal Paddle Collision Check (Moving Swings & Stationary Blocks)
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

static void LogEventHitDebug(UWorld* World, const FString& HitType, const FVector& HitLocation, AActor* HittingPaddleActor = nullptr)
{
	if (!World) return;

	float HitTimeMs = World->GetTimeSeconds() * 1000.0f;

	FVector P1Pos = FVector::ZeroVector;
	FVector P2Pos = FVector::ZeroVector;

	TArray<AActor*> FoundPaddles;
	UGameplayStatics::GetAllActorsOfClass(World, AAirHockeyPaddle::StaticClass(), FoundPaddles);
	for (AActor* PaddleActor : FoundPaddles)
	{
		AAirHockeyPaddle* Paddle = Cast<AAirHockeyPaddle>(PaddleActor);
		if (Paddle)
		{
			if (Paddle->GetPlayerIndex() == 1) P1Pos = Paddle->GetActorLocation();
			else if (Paddle->GetPlayerIndex() == 2) P2Pos = Paddle->GetActorLocation();
		}
	}

	FString HittingPaddleStr = HittingPaddleActor ? HittingPaddleActor->GetName() : TEXT("N/A (Wall)");
	FVector HittingPaddlePos = HittingPaddleActor ? HittingPaddleActor->GetActorLocation() : FVector::ZeroVector;

	UE_LOG(LogTemp, Warning, TEXT("================================================================================"));
	UE_LOG(LogTemp, Warning, TEXT("💥 [EVENT HIT LOG] Time: %.2f ms | Role: %s"), HitTimeMs, World->IsNetMode(NM_DedicatedServer) ? TEXT("SERVER") : TEXT("CLIENT"));
	UE_LOG(LogTemp, Warning, TEXT("▶ Hit Type          : %s"), *HitType);
	UE_LOG(LogTemp, Warning, TEXT("▶ Puck Hit Location : X=%.2f Y=%.2f Z=%.2f"), HitLocation.X, HitLocation.Y, HitLocation.Z);
	if (HittingPaddleActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("▶ Hitting Paddle Pos: X=%.2f Y=%.2f Z=%.2f (%s)"), HittingPaddlePos.X, HittingPaddlePos.Y, HittingPaddlePos.Z, *HittingPaddleStr);
	}
	UE_LOG(LogTemp, Warning, TEXT("▶ Player 1 Paddle   : X=%.2f Y=%.2f Z=%.2f"), P1Pos.X, P1Pos.Y, P1Pos.Z);
	UE_LOG(LogTemp, Warning, TEXT("▶ Player 2 Paddle   : X=%.2f Y=%.2f Z=%.2f"), P2Pos.X, P2Pos.Y, P2Pos.Z);
	UE_LOG(LogTemp, Warning, TEXT("================================================================================"));
}

void AAirHockeyPuck::HandleWallBounce(const FVector& SurfaceNormal)
{
	FVector Normal2D = FVector(SurfaceNormal.X, SurfaceNormal.Y, 0.0f).GetSafeNormal();
	if (Normal2D.IsNearlyZero()) return;

	CurrentVelocity = CurrentVelocity - 2.0f * (FVector::DotProduct(CurrentVelocity, Normal2D)) * Normal2D;
	CurrentVelocity.Z = 0.0f;

	FString HitTypeStr = FString::Printf(TEXT("WALL BOUNCE (Normal: X=%.1f, Y=%.1f)"), Normal2D.X, Normal2D.Y);
	LogEventHitDebug(GetWorld(), HitTypeStr, GetActorLocation(), nullptr);
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
	FString HitTypeStr;

	if (PaddleVel2D.SizeSquared() > 100.0f)
	{
		float SwingSpeed = PaddleVel2D.Size();
		CurrentVelocity = HitDir * (250.0f + SwingSpeed * 0.85f);
		HitTypeStr = FString::Printf(TEXT("PADDLE HIT (Active Swing - Speed: %.1f)"), SwingSpeed);
	}
	else
	{
		FVector SurfaceNormal = HitDir;
		FVector ReflectVel = CurrentVelocity - 2.0f * (FVector::DotProduct(CurrentVelocity, SurfaceNormal)) * SurfaceNormal;
		float BounceSpeed = FMath::Max(ReflectVel.Size() * 0.85f, 200.0f);
		CurrentVelocity = SurfaceNormal * BounceSpeed;
		HitTypeStr = TEXT("PADDLE HIT (Stationary Block Bounce)");
	}

	CurrentVelocity.Z = 0.0f;
	LogEventHitDebug(GetWorld(), HitTypeStr, GetActorLocation(), PaddleActor);
}

void AAirHockeyPuck::HandlePaddleHitLagCompensated(AActor* PaddleActor, const FVector& PaddleVelocity, float HitAge)
{
	if (!HasAuthority() || !PaddleActor) return;

	CurrentVelocity = PaddleVelocity;
	CurrentVelocity.Z = 0.0f;

	ServerPuckState.Position = GetActorLocation();
	ServerPuckState.Velocity = CurrentVelocity;

	FString HitTypeStr = FString::Printf(TEXT("SERVER PUCK LAUNCH (HitAge: %.1f ms)"), HitAge * 1000.0f);
	LogEventHitDebug(GetWorld(), HitTypeStr, GetActorLocation(), PaddleActor);
}

void AAirHockeyPuck::Client_ReceivePuck10msPacket_Implementation(FPuck10msPacket Packet)
{
	if (!HasAuthority())
	{
		for (const FPuck10msSample& Sample : Packet.RedundantSamples)
		{
			bool bAlreadyExists = false;
			for (const FPuckWorldSnapshot& ExistingSnap : PuckSnapshotBuffer)
			{
				if (FMath::IsNearlyEqual(ExistingSnap.TimeStamp, Sample.TimeStamp, 0.0001f))
				{
					bAlreadyExists = true;
					break;
				}
			}

			if (!bAlreadyExists)
			{
				PuckSnapshotBuffer.Add(FPuckWorldSnapshot(Sample.Position, Sample.Velocity, Sample.TimeStamp));
			}
		}

		PuckSnapshotBuffer.Sort([](const FPuckWorldSnapshot& A, const FPuckWorldSnapshot& B) {
			return A.TimeStamp < B.TimeStamp;
		});

		if (!bIsPuckJitterBufferInitialized && PuckSnapshotBuffer.Num() >= 3)
		{
			PuckClientPlaybackTime = PuckSnapshotBuffer[0].TimeStamp;
			bIsPuckJitterBufferInitialized = true;
		}

		while (PuckSnapshotBuffer.Num() > 15)
		{
			PuckSnapshotBuffer.RemoveAt(0);
		}
	}
}

void AAirHockeyPuck::OnRep_PuckState()
{
	if (!HasAuthority())
	{
		if (PuckSnapshotBuffer.Num() < 2 || GetActorLocation().Z < 30.0f)
		{
			SetActorLocation(ServerPuckState.Position);
			CurrentVelocity = ServerPuckState.Velocity;
		}
	}
}