#include "AirHockeyPlayerController.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Kismet/GameplayStatics.h"

AAirHockeyPlayerController::AAirHockeyPlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
	bAutoManageActiveCameraTarget = false; // Prevent UE from automatically resetting ViewTarget to Pawn!
}

void AAirHockeyPlayerController::BeginPlay()
{
	Super::BeginPlay();
	SetupTopDownCamera();
}

void AAirHockeyPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	SetupTopDownCamera();
}

void AAirHockeyPlayerController::ClientRestart_Implementation(APawn* NewPawn)
{
	Super::ClientRestart_Implementation(NewPawn);
	SetupTopDownCamera();
}

void AAirHockeyPlayerController::SetupTopDownCamera()
{
	if (!IsLocalController()) return;

	if (!TopDownCameraActor)
	{
		// Find CameraActor placed directly in the Level by user
		AActor* FoundCamera = UGameplayStatics::GetActorOfClass(GetWorld(), ACameraActor::StaticClass());
		TopDownCameraActor = Cast<ACameraActor>(FoundCamera);

		// Fallback: spawn one if not found in level
		if (!TopDownCameraActor)
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			TopDownCameraActor = GetWorld()->SpawnActor<ACameraActor>(CameraLocation, CameraRotation, SpawnParams);
		}
	}

	if (TopDownCameraActor)
	{
		SetViewTarget(TopDownCameraActor);
	}
}

void AAirHockeyPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
}

void AAirHockeyPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
}

FVector AAirHockeyPlayerController::GetMouseWorldPositionOnTablePlane() const
{
	FVector WorldLocation, WorldDirection;
	if (DeprojectMousePositionToWorld(WorldLocation, WorldDirection))
	{
		if (!FMath::IsNearlyZero(WorldDirection.Z))
		{
			float T = (TableZHeight - WorldLocation.Z) / WorldDirection.Z;
			return WorldLocation + WorldDirection * T;
		}
	}
	return FVector::ZeroVector;
}
