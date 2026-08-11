#include "AirHockeyPaddle.h"
#include "AirHockeyPlayerController.h"
#include "AirHockeyGameState.h"
#include "AirHockeyPuck.h"
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
			UE_LOG(LogTemp, Warning, TEXT("[AIR HOCKEY] Loaded Engine Cylinder Mesh from: %s"), Path);
			return LoadedMesh;
		}
	}
	return nullptr;
}

AAirHockeyPaddle::AAirHockeyPaddle()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	CollisionSphere->InitSphereRadius(PaddleRadius);
	CollisionSphere->SetCollisionProfileName(TEXT("Pawn"));
	SetRootComponent(CollisionSphere);

	PaddleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PaddleMesh"));
	PaddleMesh->SetupAttachment(CollisionSphere);
	PaddleMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	UStaticMesh* MeshObj = GetEngineCylinderMesh();
	if (MeshObj)
	{
		PaddleMesh->SetStaticMesh(MeshObj);
		PaddleMesh->SetRelativeScale3D(FVector(1.2f, 1.2f, 0.4f));
	}
}

void AAirHockeyPaddle::BeginPlay()
{
	Super::BeginPlay();

	if (PaddleMesh)
	{
		if (!PaddleMesh->GetStaticMesh())
		{
			UStaticMesh* MeshObj = GetEngineCylinderMesh();
			if (MeshObj)
			{
				PaddleMesh->SetStaticMesh(MeshObj);
				PaddleMesh->SetRelativeScale3D(FVector(1.2f, 1.2f, 0.4f));
			}
		}

		UMaterialInterface* AssignedMat = (PlayerIndex == 1) ? Player1Material : Player2Material;
		if (AssignedMat)
		{
			PaddleMesh->SetMaterial(0, AssignedMat);
		}
		else
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
}

void AAirHockeyPaddle::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// STEP 1.2: Dynamic Bounds Clamped Mouse Movement (Pure Local Responsiveness)
	if (IsLocallyControlled())
	{
		APlayerController* PC = Cast<APlayerController>(GetController());
		if (PC)
		{
			if (LastValidTargetInput.IsZero())
			{
				LastValidTargetInput = GetActorLocation();
			}

			FVector TargetInput = LastValidTargetInput;
			FHitResult CursorHit;
			bool bFoundTarget = false;

			// Line Trace under cursor to project mouse position onto table surface
			if (PC->GetHitResultUnderCursorByChannel(TraceTypeQuery1, true, CursorHit))
			{
				LastValidTargetInput = CursorHit.Location;
				LastValidTargetInput.Z = TableZHeight;
				TargetInput = LastValidTargetInput;
				bFoundTarget = true;
			}
			else
			{
				FVector WorldLocation, WorldDirection;
				if (PC->DeprojectMousePositionToWorld(WorldLocation, WorldDirection))
				{
					if (!FMath::IsNearlyZero(WorldDirection.Z))
					{
						float T = (TableZHeight - WorldLocation.Z) / WorldDirection.Z;
						LastValidTargetInput = WorldLocation + WorldDirection * T;
						TargetInput = LastValidTargetInput;
						bFoundTarget = true;
					}
				}
			}

			// Calculate Clamped Position according to Dynamic Half-Table Formula
			FVector NewPos, NewVel;
			PerformMove(TargetInput, DeltaTime, NewPos, NewVel);

			// Fast smooth glide to target (Speed = 45.0f) to avoid teleports on mouse re-entry
			FVector FastSmoothPos = FMath::VInterpTo(GetActorLocation(), NewPos, DeltaTime, 45.0f);
			SetActorLocation(FastSmoothPos);
			CurrentVelocity = NewVel;

			// STEP 11.1: Uncapped Frame-Rate Mouse Streaming (120Hz / 144Hz / 240Hz)
			float DistFromLastSent = FVector::Dist2D(FastSmoothPos, LastSentPosition);
			if (NewVel.SizeSquared() > 10.0f || DistFromLastSent > 0.1f)
			{
				Server_SendPaddlePosition(FastSmoothPos, NewVel);
				LastSentPosition = FastSmoothPos;
			}

			// STEP 6.1: Local Client Immediate Impact Prediction (0ms Latency Launch)
			if (NewVel.SizeSquared() > 100.0f)
			{
				TArray<AActor*> FoundPucks;
				UGameplayStatics::GetAllActorsOfClass(GetWorld(), AAirHockeyPuck::StaticClass(), FoundPucks);
				for (AActor* PuckActor : FoundPucks)
				{
					AAirHockeyPuck* Puck = Cast<AAirHockeyPuck>(PuckActor);
					if (Puck)
					{
						float Dist2D = FVector::Dist2D(FastSmoothPos, Puck->GetActorLocation());
						if (Dist2D <= (PaddleRadius + 40.0f)) // 60 + 40 = 100cm
						{
							Puck->HandlePaddleHit(this, NewVel);
						}
					}
				}
			}
		}
	}
	else
	{
		// 0ms Latency Remote Client Interpolation with Cubic Spline & Dead Reckoning
		if (SnapshotBuffer.Num() >= 2)
		{
			const FPaddleSnapshot& Snap0 = SnapshotBuffer[SnapshotBuffer.Num() - 2];
			const FPaddleSnapshot& Snap1 = SnapshotBuffer[SnapshotBuffer.Num() - 1];

			float TimeGap = FMath::Max(Snap1.TimeStamp - Snap0.TimeStamp, 0.001f);

			FVector P0 = Snap0.Position;
			FVector T0 = Snap0.Velocity * TimeGap;
			FVector P1 = Snap1.Position;
			FVector T1 = Snap1.Velocity * TimeGap;

			FVector CurrentPos = GetActorLocation();
			FVector SmoothCurvedPos = FMath::CubicInterp(P0, T0, P1, T1, FMath::Clamp(DeltaTime * 35.0f, 0.0f, 1.0f));

			// STEP 12.1: Dead Reckoning Velocity Extrapolation (Zero Stutter Prediction)
			FVector DeadReckoningPos = Snap1.Position + (Snap1.Velocity * DeltaTime);
			FVector FinalTargetPos = FMath::VInterpTo(SmoothCurvedPos, DeadReckoningPos, DeltaTime, 15.0f);

			SetActorLocation(FinalTargetPos);
			CurrentVelocity = Snap1.Velocity;
		}
		else if (!ServerState.Position.IsZero())
		{
			FVector InterpPos = FMath::VInterpTo(GetActorLocation(), ServerState.Position, DeltaTime, 30.0f);
			SetActorLocation(InterpPos);
			CurrentVelocity = ServerState.Velocity;
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

void AAirHockeyPaddle::PerformMove(const FVector& TargetLocation, float DeltaTime, FVector& OutPosition, FVector& OutVelocity)
{
	AAirHockeyGameState* GS = GetWorld() ? GetWorld()->GetGameState<AAirHockeyGameState>() : nullptr;
	if (GS && GS->bIsGameOver)
	{
		OutPosition = GetActorLocation();
		OutVelocity = FVector::ZeroVector;
		return;
	}

	FVector ClampedTarget = TargetLocation;
	ClampedTarget.Z = TableZHeight;

	float HalfLength = TableLength / 2.0f;
	float HalfWidth = TableWidth / 2.0f;

	// STEP 1.2: Dynamic Half-Table Boundaries Formula
	if (PlayerIndex == 1)
	{
		// Player 1: Left Wall (-HalfLength + Radius) to Centerline (0 - Radius)
		ClampedTarget.X = FMath::Clamp(ClampedTarget.X, -HalfLength + PaddleRadius, 0.0f - PaddleRadius);
	}
	else
	{
		// Player 2: Centerline (0 + Radius) to Right Wall (HalfLength - Radius)
		ClampedTarget.X = FMath::Clamp(ClampedTarget.X, 0.0f + PaddleRadius, HalfLength - PaddleRadius);
	}
	ClampedTarget.Y = FMath::Clamp(ClampedTarget.Y, -HalfWidth + PaddleRadius, HalfWidth - PaddleRadius);

	FVector CurrentPos = GetActorLocation();
	CurrentPos.Z = TableZHeight;

	FVector InstantVel = (ClampedTarget - CurrentPos) / FMath::Max(DeltaTime, 0.0001f);
	InstantVel = InstantVel.GetClampedToMaxSize(MaxSpeed);

	OutPosition = ClampedTarget;
	OutVelocity = InstantVel;
}

void AAirHockeyPaddle::Server_SendPaddlePosition_Implementation(FVector ClampedPosition, FVector CalculatedVelocity)
{
	FVector ValidatedPos, ValidatedVel;
	PerformMove(ClampedPosition, 1.0f / 60.0f, ValidatedPos, ValidatedVel);

	SetActorLocation(ValidatedPos);

	ServerState.Position = ValidatedPos;
	ServerState.Velocity = CalculatedVelocity;

	// STEP 10.1: Direct Server-to-Client RPC Relay to the Opponent's possessed Paddle
	TArray<AActor*> FoundPaddles;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AAirHockeyPaddle::StaticClass(), FoundPaddles);
	for (AActor* PaddleActor : FoundPaddles)
	{
		AAirHockeyPaddle* OtherPaddle = Cast<AAirHockeyPaddle>(PaddleActor);
		if (OtherPaddle && OtherPaddle != this && OtherPaddle->GetPlayerIndex() != PlayerIndex)
		{
			OtherPaddle->Client_ReceiveOpponentPaddlePosition(PlayerIndex, ValidatedPos, ValidatedVel);
		}
	}

	// STEP 15.2: Server State Reconstruction & Lag Compensated Physics Launch
	if (CalculatedVelocity.SizeSquared() > 100.0f)
	{
		TArray<AActor*> FoundPucks;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), AAirHockeyPuck::StaticClass(), FoundPucks);
		for (AActor* PuckActor : FoundPucks)
		{
			AAirHockeyPuck* Puck = Cast<AAirHockeyPuck>(PuckActor);
			if (Puck)
			{
				float Dist2D = FVector::Dist2D(ValidatedPos, Puck->GetActorLocation());
				if (Dist2D <= (PaddleRadius + 40.0f)) // 60 + 40 = 100cm
				{
					float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
					Puck->HandlePaddleHitLagCompensated(this, CalculatedVelocity, CurrentTime);
				}
			}
		}
	}
}

void AAirHockeyPaddle::Client_ReceiveOpponentPaddlePosition_Implementation(int32 SenderPlayerIndex, FVector Position, FVector Velocity)
{
	// Forward snapshot to the local proxy paddle representing SenderPlayerIndex
	TArray<AActor*> FoundPaddles;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AAirHockeyPaddle::StaticClass(), FoundPaddles);
	for (AActor* PaddleActor : FoundPaddles)
	{
		AAirHockeyPaddle* RemoteProxy = Cast<AAirHockeyPaddle>(PaddleActor);
		if (RemoteProxy && !RemoteProxy->IsLocallyControlled() && RemoteProxy->GetPlayerIndex() == SenderPlayerIndex)
		{
			float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
			float TimeGap = (RemoteProxy->SnapshotBuffer.Num() > 0) ? (CurrentTime - RemoteProxy->SnapshotBuffer.Last().TimeStamp) : 0.016f;
			float PacketHz = (TimeGap > 0.0001f) ? (1.0f / TimeGap) : 0.0f;

			RemoteProxy->SnapshotBuffer.Add(FPaddleSnapshot(Position, Velocity, CurrentTime));

			if (RemoteProxy->SnapshotBuffer.Num() > 5)
			{
				RemoteProxy->SnapshotBuffer.RemoveAt(0);
			}
		}
	}
}

bool AAirHockeyPaddle::Server_SendPaddlePosition_Validate(FVector ClampedPosition, FVector CalculatedVelocity)
{
	return true;
}

void AAirHockeyPaddle::Server_SendMove_Implementation(FPaddleMove Move)
{
	FVector NewPosition, NewVelocity;
	PerformMove(Move.TargetInputPosition, 1.0f / 60.0f, NewPosition, NewVelocity);
	SetActorLocation(NewPosition, true);

	ServerState.Position = NewPosition;
	ServerState.Velocity = NewVelocity;
	ServerState.LastProcessedSequenceNumber = Move.SequenceNumber;

	Client_ReconcileState(ServerState);
}

bool AAirHockeyPaddle::Server_SendMove_Validate(FPaddleMove Move)
{
	return true;
}

void AAirHockeyPaddle::RollbackAndResimulate(const FPaddleRollbackState& AuthoritativeState)
{
	if (IsLocallyControlled())
	{
		// 1. Filter out all acknowledged inputs from local pending buffer
		UnacknowledgedMoves.RemoveAll([AuthoritativeState](const FPaddleMove& Move) {
			return Move.SequenceNumber <= AuthoritativeState.SequenceNumber;
		});

		// 2. Rollback local position to Authoritative Server position
		SetActorLocation(AuthoritativeState.Position, true);
		CurrentVelocity = AuthoritativeState.Velocity;

		// 3. Fast-forward re-simulate all remaining unacknowledged inputs in loop (< 0.1ms)
		for (const FPaddleMove& Move : UnacknowledgedMoves)
		{
			FVector ResimPos, ResimVel;
			PerformMove(Move.TargetInputPosition, 1.0f / 60.0f, ResimPos, ResimVel);
			SetActorLocation(ResimPos, true);
		}
	}
}

void AAirHockeyPaddle::Client_ReconcileState_Implementation(FPaddleState AuthoritativeState)
{
	FPaddleRollbackState RollbackState(AuthoritativeState.LastProcessedSequenceNumber, AuthoritativeState.Position, AuthoritativeState.Velocity, GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f);
	RollbackAndResimulate(RollbackState);
}

void AAirHockeyPaddle::OnRep_ServerState()
{
	if (!IsLocallyControlled())
	{
		float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
		SnapshotBuffer.Add(FPaddleSnapshot(ServerState.Position, ServerState.Velocity, CurrentTime));

		if (SnapshotBuffer.Num() > 5)
		{
			SnapshotBuffer.RemoveAt(0);
		}
	}
}
