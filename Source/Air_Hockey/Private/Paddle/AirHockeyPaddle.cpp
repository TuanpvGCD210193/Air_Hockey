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

			// STEP 18.1: 10ms Fixed High-Precision Sampler (100 Hz Mouse Collector)
			SampleTimer += DeltaTime;
			if (SampleTimer >= 0.010f)
			{
				SampleTimer = 0.0f;
				float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
				LocalSampleBuffer.Add(FPaddle10msSample(FastSmoothPos, NewVel, LocalSequenceCounter++, CurrentTime));
				if (LocalSampleBuffer.Num() > 4)
				{
					LocalSampleBuffer.RemoveAt(0);
				}

				FPaddle10msPacket Packet;
				Packet.PlayerIndex = PlayerIndex;
				Packet.RedundantSamples = LocalSampleBuffer;

				Server_Send10msPacket(Packet);
			}

			// STEP 6.1: Local Client Immediate Impact Prediction & STEP 27.1: Rocket League Prediction Start
			if (NewVel.SizeSquared() > 100.0f)
			{
				float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
				if (CurrentTime - LastPuckHitTime >= PuckHitCooldown)
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
								LastPuckHitTime = CurrentTime;
								Puck->HandlePaddleHit(this, NewVel);

								// STEP 31.1: Send exact computed 2D launch velocity vector to Server for 100Hz Hermite Spline relay
								Server_RequestPuckHit(Puck, Puck->GetPuckVelocity(), 0.0f);
								break;
							}
						}
					}
				}
			}
		}
	}
	else
	{
		// STEP 19.2: Adaptive Jitter Buffer & Playback Clock Engine (Zero-Teleport 100ms-500ms Ping Smoothing)
		if (SnapshotBuffer.Num() >= 2)
		{
			if (!bIsJitterBufferInitialized)
			{
				ClientPlaybackTime = SnapshotBuffer[0].TimeStamp;
				bIsJitterBufferInitialized = true;
			}

			float LatestTime = SnapshotBuffer.Last().TimeStamp;
			float CurrentBufferDelay = LatestTime - ClientPlaybackTime;

			// Adaptive Time Dilation: Speed up or slow down clock smoothly to maintain ~120ms target buffer delay
			if (CurrentBufferDelay > (TargetJitterDelay + 0.05f))
			{
				AdaptivePlaybackRate = 1.10f; // Speed up slightly to catch up
			}
			else if (CurrentBufferDelay < (TargetJitterDelay - 0.03f))
			{
				AdaptivePlaybackRate = 0.90f; // Slow down slightly to wait for network
			}
			else
			{
				AdaptivePlaybackRate = 1.0f;
			}

			ClientPlaybackTime += DeltaTime * AdaptivePlaybackRate;

			// Find bounding snapshots around ClientPlaybackTime
			int32 SnapIdx0 = -1;
			int32 SnapIdx1 = -1;

			for (int32 i = 0; i < SnapshotBuffer.Num() - 1; ++i)
			{
				if (SnapshotBuffer[i].TimeStamp <= ClientPlaybackTime && SnapshotBuffer[i + 1].TimeStamp >= ClientPlaybackTime)
				{
					SnapIdx0 = i;
					SnapIdx1 = i + 1;
					break;
				}
			}

			if (SnapIdx0 != -1 && SnapIdx1 != -1)
			{
				const FPaddleSnapshot& Snap0 = SnapshotBuffer[SnapIdx0];
				const FPaddleSnapshot& Snap1 = SnapshotBuffer[SnapIdx1];

				float TimeGap = FMath::Max(Snap1.TimeStamp - Snap0.TimeStamp, 0.001f);
				float Alpha = FMath::Clamp((ClientPlaybackTime - Snap0.TimeStamp) / TimeGap, 0.0f, 1.0f);

				FVector P0 = Snap0.Position;
				FVector T0 = Snap0.Velocity * TimeGap;
				FVector P1 = Snap1.Position;
				FVector T1 = Snap1.Velocity * TimeGap;

				FVector SmoothCurvedPos = FMath::CubicInterp(P0, T0, P1, T1, Alpha);
				FVector UltraSmoothPos = FMath::VInterpTo(GetActorLocation(), SmoothCurvedPos, DeltaTime, 45.0f);

				SetActorLocation(UltraSmoothPos);
				CurrentVelocity = FMath::Lerp(Snap0.Velocity, Snap1.Velocity, Alpha);
			}
			else if (ClientPlaybackTime > LatestTime)
			{
				// Extrapolate smoothly using dead reckoning if network stalls
				const FPaddleSnapshot& LatestSnap = SnapshotBuffer.Last();
				FVector ExtrapolatedPos = LatestSnap.Position + LatestSnap.Velocity * (ClientPlaybackTime - LatestTime);
				FVector SmoothExtrapolated = FMath::VInterpTo(GetActorLocation(), ExtrapolatedPos, DeltaTime, 15.0f);

				SetActorLocation(SmoothExtrapolated);
				CurrentVelocity = LatestSnap.Velocity;
			}

			// Clean up old snapshots older than 1 second from current playback time
			SnapshotBuffer.RemoveAll([this](const FPaddleSnapshot& Snap) {
				return Snap.TimeStamp < (ClientPlaybackTime - 1.0f);
			});
		}
		else if (!ServerState.Position.IsZero())
		{
			FVector InterpPos = FMath::VInterpTo(GetActorLocation(), ServerState.Position, DeltaTime, 45.0f);
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

void AAirHockeyPaddle::Server_Send10msPacket_Implementation(FPaddle10msPacket Packet)
{
	// STEP 35 (OPTION B): Synchronize Server's possessed Paddle Actor Location in real-time
	// WITHOUT modifying ServerState or triggering OnRep_ServerState replication to Client 2!
	if (HasAuthority() && Packet.RedundantSamples.Num() > 0)
	{
		const FPaddle10msSample& LatestSample = Packet.RedundantSamples.Last();
		SetActorLocation(LatestSample.Position);
		CurrentVelocity = LatestSample.Velocity;
	}

	// STEP 26.1: Attach Server's latest Puck 10ms samples to relay over possessed PlayerController channel
	TArray<AActor*> FoundPucks;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AAirHockeyPuck::StaticClass(), FoundPucks);
	if (FoundPucks.Num() > 0)
	{
		AAirHockeyPuck* Puck = Cast<AAirHockeyPuck>(FoundPucks[0]);
		if (Puck)
		{
			Packet.RedundantPuckSamples = Puck->GetLocalPuckSampleBuffer();
		}
	}

	// Relay the 10ms redundant packet to all client paddles
	TArray<AActor*> FoundPaddles;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AAirHockeyPaddle::StaticClass(), FoundPaddles);
	for (AActor* PaddleActor : FoundPaddles)
	{
		AAirHockeyPaddle* TargetPaddle = Cast<AAirHockeyPaddle>(PaddleActor);
		if (TargetPaddle)
		{
			TargetPaddle->Client_Receive10msPacket(Packet);
		}
	}
}

bool AAirHockeyPaddle::Server_Send10msPacket_Validate(FPaddle10msPacket Packet)
{
	return true;
}

void AAirHockeyPaddle::Client_Receive10msPacket_Implementation(FPaddle10msPacket Packet)
{
	// 1. Unpack all redundant 10ms paddle samples into SnapshotBuffer for remote proxy paddle
	TArray<AActor*> FoundPaddles;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AAirHockeyPaddle::StaticClass(), FoundPaddles);
	for (AActor* PaddleActor : FoundPaddles)
	{
		AAirHockeyPaddle* RemoteProxy = Cast<AAirHockeyPaddle>(PaddleActor);
		if (RemoteProxy && !RemoteProxy->IsLocallyControlled() && RemoteProxy->GetPlayerIndex() == Packet.PlayerIndex)
		{
			for (const FPaddle10msSample& Sample : Packet.RedundantSamples)
			{
				bool bAlreadyExists = false;
				for (const FPaddleSnapshot& ExistingSnap : RemoteProxy->SnapshotBuffer)
				{
					if (FMath::IsNearlyEqual(ExistingSnap.TimeStamp, Sample.TimeStamp, 0.0001f))
					{
						bAlreadyExists = true;
						break;
					}
				}

				if (!bAlreadyExists)
				{
					RemoteProxy->SnapshotBuffer.Add(FPaddleSnapshot(Sample.Position, Sample.Velocity, Sample.TimeStamp));
				}
			}

			// Keep SnapshotBuffer sorted chronologically by TimeStamp
			RemoteProxy->SnapshotBuffer.Sort([](const FPaddleSnapshot& A, const FPaddleSnapshot& B) {
				return A.TimeStamp < B.TimeStamp;
			});

			if (!RemoteProxy->bIsJitterBufferInitialized && RemoteProxy->SnapshotBuffer.Num() >= 3)
			{
				RemoteProxy->ClientPlaybackTime = RemoteProxy->SnapshotBuffer[0].TimeStamp;
				RemoteProxy->bIsJitterBufferInitialized = true;
			}

			while (RemoteProxy->SnapshotBuffer.Num() > 15)
			{
				RemoteProxy->SnapshotBuffer.RemoveAt(0);
			}
		}
	}

	// 2. STEP 26.1: Unpack RedundantPuckSamples into local AAirHockeyPuck actor's 10ms Jitter Buffer
	if (!HasAuthority() && Packet.RedundantPuckSamples.Num() > 0)
	{
		TArray<AActor*> FoundPucks;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), AAirHockeyPuck::StaticClass(), FoundPucks);
		if (FoundPucks.Num() > 0)
		{
			AAirHockeyPuck* Puck = Cast<AAirHockeyPuck>(FoundPucks[0]);
			if (Puck)
			{
				FPuck10msPacket PuckPacket;
				PuckPacket.RedundantSamples = Packet.RedundantPuckSamples;
				Puck->Client_ReceivePuck10msPacket_Implementation(PuckPacket);
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

void AAirHockeyPaddle::Server_RequestPuckHit_Implementation(AAirHockeyPuck* Puck, FVector HitVelocity, float HitAge)
{
	if (Puck)
	{
		Puck->HandlePaddleHitLagCompensated(this, HitVelocity, HitAge);
	}
}

bool AAirHockeyPaddle::Server_RequestPuckHit_Validate(AAirHockeyPuck* Puck, FVector HitVelocity, float HitAge)
{
	return true;
}