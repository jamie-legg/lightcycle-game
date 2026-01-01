// ArmaCycle.cpp - Implementation of the complete lightcycle

#include "ArmaCycle.h"
#include "ArmaCycleMovement.h"
#include "ArmaWall.h"
#include "Core/ArmaGrid.h"
#include "Components/StaticMeshComponent.h"
#include "NiagaraComponent.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

// Static member initialization
float AArmaCycle::WallsStayUpDelay = ArmaPhysics::DefaultWallsStayUpDelay;
float AArmaCycle::WallsLength = ArmaPhysics::DefaultWallsLength;
float AArmaCycle::ExplosionRadius = ArmaPhysics::DefaultExplosionRadius;

//////////////////////////////////////////////////////////////////////////
// FCycleMemory Implementation
//////////////////////////////////////////////////////////////////////////

FCycleMemoryEntry* FCycleMemory::Remember(AArmaCycle* Cycle)
{
	if (!Cycle)
		return nullptr;

	// Find existing entry
	for (FCycleMemoryEntry& Entry : Entries)
	{
		if (Entry.Cycle.Get() == Cycle)
			return &Entry;
	}

	// Create new entry
	FCycleMemoryEntry NewEntry;
	NewEntry.Cycle = Cycle;
	NewEntry.Time = Cycle->GetWorld()->GetTimeSeconds();
	Entries.Add(NewEntry);
	
	return &Entries.Last();
}

FCycleMemoryEntry* FCycleMemory::Latest(int32 Side) const
{
	FCycleMemoryEntry* Result = nullptr;
	float LatestTime = -FLT_MAX;

	for (const FCycleMemoryEntry& Entry : Entries)
	{
		if (Entry.Side == Side && Entry.Time > LatestTime)
		{
			LatestTime = Entry.Time;
			Result = const_cast<FCycleMemoryEntry*>(&Entry);
		}
	}

	return Result;
}

FCycleMemoryEntry* FCycleMemory::Earliest(int32 Side) const
{
	FCycleMemoryEntry* Result = nullptr;
	float EarliestTime = FLT_MAX;

	for (const FCycleMemoryEntry& Entry : Entries)
	{
		if (Entry.Side == Side && Entry.Time < EarliestTime)
		{
			EarliestTime = Entry.Time;
			Result = const_cast<FCycleMemoryEntry*>(&Entry);
		}
	}

	return Result;
}

//////////////////////////////////////////////////////////////////////////
// AArmaCycle Implementation
//////////////////////////////////////////////////////////////////////////

AArmaCycle::AArmaCycle()
{
	PrimaryActorTick.bCanEverTick = true;

	// Create root component
	RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
	RootComponent = RootSceneComponent;

	// Create cycle body mesh
	CycleBodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CycleBody"));
	CycleBodyMesh->SetupAttachment(RootComponent);
	CycleBodyMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CycleBodyMesh->SetCollisionProfileName(TEXT("Pawn"));
	
	// Apply orientation correction - matching original gCycle.cpp behavior
	// Original uses: glScalef(.5f,.5f,.5f) and glTranslatef(-1.5,0,0)
	// The rotation matrix in gCycle.cpp line 4896 shows model front faces +X
	// The mesh may be imported with front facing +Y, so apply -90 degree yaw correction
	// This fixes the "horizontal pointing left and right" issue
	CycleBodyMesh->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	CycleBodyMesh->SetRelativeScale3D(FVector(0.5f, 0.5f, 0.5f));
	CycleBodyMesh->SetRelativeLocation(FVector(-1.5f * 50.0f, 0.0f, 0.0f)); // Scale offset by 50 (UE5 units)

	// Create wheel meshes - positions from original gCycle.cpp
	// Front wheel: glTranslatef(1.84,0,.43) in original
	// Rear wheel: glTranslatef(0,0,.73) in original
	// Note: these offsets are relative to the body mesh after body transforms
	FrontWheelMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FrontWheel"));
	FrontWheelMesh->SetupAttachment(CycleBodyMesh);
	// Original position: (1.84, 0, .43) - but body has -90 yaw, so adjust X/Y
	FrontWheelMesh->SetRelativeLocation(FVector(0.0f, 1.84f * 50.0f, 0.43f * 50.0f));

	RearWheelMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RearWheel"));
	RearWheelMesh->SetupAttachment(CycleBodyMesh);
	// Original position: (0, 0, .73)
	RearWheelMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 0.73f * 50.0f));

	// Create movement component
	CycleMovement = CreateDefaultSubobject<UArmaCycleMovementComponent>(TEXT("CycleMovement"));

	// Create trail particles (Niagara)
	TrailParticles = CreateDefaultSubobject<UNiagaraComponent>(TEXT("TrailParticles"));
	TrailParticles->SetupAttachment(RootComponent);
	TrailParticles->SetAutoActivate(true);

	// Create engine sound
	EngineSound = CreateDefaultSubobject<UAudioComponent>(TEXT("EngineSound"));
	EngineSound->SetupAttachment(RootComponent);
	EngineSound->SetAutoActivate(true);

	// Initialize state
	CurrentWall = nullptr;
	LastWall = nullptr;
	bBuildingWall = true;
	bDropWallRequested = false;
	SpawnTime = 0.0f;
	LastTimeAnim = 0.0f;
	Skew = 0.0f;
	SkewDot = 0.0f;
	CorrectDistanceSmooth = 0.0f;

	// Default colors
	CycleColor = FArmaColor::Red;
	TrailColor = FArmaColor::Red;

	TacticalPosition = ECycleTacticalPosition::Start;
	ClosestZoneID = -1;
}

void AArmaCycle::BeginPlay()
{
	Super::BeginPlay();

	SpawnTime = GetWorld()->GetTimeSeconds();
	LastTimeAnim = SpawnTime;

	// Bind to movement component events
	if (CycleMovement)
	{
		CycleMovement->OnDeath.AddDynamic(this, &AArmaCycle::OnMovementDeath);
		CycleMovement->OnTurn.AddDynamic(this, &AArmaCycle::OnMovementTurn);
	}

	// Store initial position
	LastGoodPosition = FArmaCoord(GetActorLocation().X, GetActorLocation().Y);

	// Spawn initial wall
	if (bBuildingWall)
	{
		SpawnNewWall();
	}

	// Broadcast spawn event
	OnSpawn.Broadcast();
}

void AArmaCycle::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!IsAlive())
		return;

	// Update wall building
	UpdateWallBuilding(DeltaTime);

	// Update wheel animation
	UpdateWheelAnimation(DeltaTime);

	// Update skew (leaning during turns)
	UpdateSkew(DeltaTime);

	// Update audio pitch based on speed
	if (EngineSound && CycleMovement)
	{
		float SpeedRatio = CycleMovement->GetSpeed() / UArmaCycleMovementComponent::GetMaximalSpeed();
		EngineSound->SetPitchMultiplier(0.8f + SpeedRatio * 0.4f);
	}
}

void AArmaCycle::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Bind actions
	PlayerInputComponent->BindAction("TurnLeft", IE_Pressed, this, &AArmaCycle::TurnLeft);
	PlayerInputComponent->BindAction("TurnRight", IE_Pressed, this, &AArmaCycle::TurnRight);
	PlayerInputComponent->BindAction("Brake", IE_Pressed, this, &AArmaCycle::StartBrake);
	PlayerInputComponent->BindAction("Brake", IE_Released, this, &AArmaCycle::StopBrake);
}

//////////////////////////////////////////////////////////////////////////
// Wall Building
//////////////////////////////////////////////////////////////////////////

void AArmaCycle::SetCurrentWall(AArmaWall* Wall)
{
	LastWall = CurrentWall;
	CurrentWall = Wall;
}

void AArmaCycle::DropWall(bool bBuildNew)
{
	if (CurrentWall)
	{
		// Finalize current wall
		CurrentWall->Finalize();
		LastWall = CurrentWall;
		CurrentWall = nullptr;
	}

	if (bBuildNew && bBuildingWall)
	{
		SpawnNewWall();
	}
}

void AArmaCycle::SetWallBuilding(bool bBuild)
{
	if (bBuild == bBuildingWall)
		return;

	bBuildingWall = bBuild;

	if (bBuildingWall && !CurrentWall)
	{
		SpawnNewWall();
	}
	else if (!bBuildingWall && CurrentWall)
	{
		DropWall(false);
	}
}

float AArmaCycle::GetMaxWallsLength() const
{
	// Could include rubber growth bonus
	return WallsLength;
}

float AArmaCycle::GetThisWallsLength() const
{
	// Could include rubber shrink penalty
	return WallsLength;
}

float AArmaCycle::GetWallEndSpeed() const
{
	// Speed at which the wall end is receding
	if (!CycleMovement)
		return 0.0f;

	float Distance = CycleMovement->GetDistance();
	float MaxLength = GetMaxWallsLength();

	if (Distance > MaxLength)
	{
		return CycleMovement->GetSpeed();  // Wall receding at cycle speed
	}

	return 0.0f;
}

void AArmaCycle::SpawnNewWall()
{
	if (!GetWorld())
		return;

	FVector SpawnLoc = GetActorLocation();
	FRotator SpawnRot = GetActorRotation();

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AArmaWall* NewWall = GetWorld()->SpawnActor<AArmaWall>(AArmaWall::StaticClass(), SpawnLoc, SpawnRot, SpawnParams);
	
	if (NewWall)
	{
		NewWall->Initialize(this, TrailColor);
		SetCurrentWall(NewWall);
	}
}

void AArmaCycle::UpdateWallBuilding(float DeltaTime)
{
	if (!CurrentWall || !CycleMovement)
		return;

	// Update wall end position
	FVector CurrentPos = GetActorLocation();
	CurrentWall->UpdateEnd(FArmaCoord(CurrentPos.X, CurrentPos.Y), GetWorld()->GetTimeSeconds());

	// Check if we should drop wall on turn
	if (bDropWallRequested)
	{
		DropWall(true);
		bDropWallRequested = false;
	}
}

void AArmaCycle::UpdateWheelAnimation(float DeltaTime)
{
	if (!CycleMovement)
		return;

	float Speed = CycleMovement->GetSpeed();
	float WheelRotation = Speed * DeltaTime * 360.0f / (2.0f * PI * 0.3f);  // Assuming 0.3m wheel radius

	// Rotate wheels
	if (FrontWheelMesh)
	{
		FRotator WheelRot = FrontWheelMesh->GetRelativeRotation();
		WheelRot.Pitch += WheelRotation;
		FrontWheelMesh->SetRelativeRotation(WheelRot);
	}

	if (RearWheelMesh)
	{
		FRotator WheelRot = RearWheelMesh->GetRelativeRotation();
		WheelRot.Pitch += WheelRotation;
		RearWheelMesh->SetRelativeRotation(WheelRot);
	}
}

void AArmaCycle::UpdateSkew(float DeltaTime)
{
	// Skew smoothing
	float TargetSkew = 0.0f;
	
	// Could be influenced by turning

	float SkewAccel = (TargetSkew - Skew) * 10.0f - SkewDot * 5.0f;
	SkewDot += SkewAccel * DeltaTime;
	Skew += SkewDot * DeltaTime;

	// Apply skew as roll
	FRotator CurrentRot = CycleBodyMesh->GetRelativeRotation();
	CurrentRot.Roll = Skew * 30.0f;  // 30 degrees max lean
	CycleBodyMesh->SetRelativeRotation(CurrentRot);
}

//////////////////////////////////////////////////////////////////////////
// Death System
//////////////////////////////////////////////////////////////////////////

void AArmaCycle::Kill()
{
	KillWithReason(TEXT("Unknown"));
}

void AArmaCycle::KillWithReason(const FString& Reason)
{
	if (!IsAlive())
		return;

	DeathReason = Reason;

	if (CycleMovement)
	{
		CycleMovement->Die(GetWorld()->GetTimeSeconds());
	}

	// Drop wall
	if (CurrentWall)
	{
		DropWall(false);
	}

	// Spawn explosion
	// TODO: Spawn explosion actor/particles

	// Broadcast death event
	OnDeath.Broadcast(Reason);
}

void AArmaCycle::KillAt(const FArmaCoord& Position)
{
	// Move to death position
	SetActorLocation(FVector(Position.X, Position.Y, GetActorLocation().Z));
	
	KillWithReason(TEXT("Collision"));
}

void AArmaCycle::Killed(AArmaCycle* Killer, int32 Type)
{
	if (Killer && Killer != this)
	{
		// Notify killer
		Killer->OnKill.Broadcast(this, Type);
	}

	FString Reason;
	switch (Type)
	{
	case 0:
		Reason = FString::Printf(TEXT("Killed by %s"), *Killer->GetName());
		break;
	case 1:
		Reason = TEXT("Suicide");
		break;
	default:
		Reason = TEXT("Unknown");
	}

	KillWithReason(Reason);
}

bool AArmaCycle::IsAlive() const
{
	return CycleMovement && CycleMovement->IsAlive();
}

bool AArmaCycle::IsVulnerable() const
{
	if (!IsAlive())
		return false;

	// Grace period after spawn
	float TimeSinceSpawn = GetWorld()->GetTimeSeconds() - SpawnTime;
	if (TimeSinceSpawn < 0.5f)
		return false;

	return CycleMovement && CycleMovement->IsVulnerable();
}

void AArmaCycle::OnMovementDeath(float Time)
{
	// Movement component died, cleanup
	OnRemoveFromGame();
}

void AArmaCycle::OnMovementTurn(int32 Direction)
{
	// Request wall drop on turn to start new segment
	bDropWallRequested = true;

	// Apply skew on turn
	SkewDot = Direction * 20.0f;
}

void AArmaCycle::OnRemoveFromGame()
{
	// Cleanup
	if (CurrentWall)
	{
		DropWall(false);
	}

	// Hide cycle
	SetActorHiddenInGame(true);
}

void AArmaCycle::OnRoundEnd()
{
	// Called when round ends
}

void AArmaCycle::RightBeforeDeath(int32 NumTries)
{
	// Last chance to escape
}

//////////////////////////////////////////////////////////////////////////
// Collision
//////////////////////////////////////////////////////////////////////////

bool AArmaCycle::EdgeIsDangerous(AArmaWall* Wall, float Time, float Alpha) const
{
	if (!Wall)
		return false;

	return CycleMovement && CycleMovement->EdgeIsDangerous(Wall, Time, Alpha);
}

void AArmaCycle::PassEdge(AArmaWall* Wall, float Time, float Alpha, int32 Recursion)
{
	if (CycleMovement)
	{
		CycleMovement->PassEdge(Wall, Time, Alpha, Recursion);
	}
}

bool AArmaCycle::IsMe(AActor* Other) const
{
	if (Other == this)
		return true;

	// Check if it's our wall
	if (AArmaWall* Wall = Cast<AArmaWall>(Other))
	{
		return Wall->GetOwnerCycle() == this;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
// Camera
//////////////////////////////////////////////////////////////////////////

FVector AArmaCycle::GetCameraPosition() const
{
	return GetActorLocation() + FVector(0, 0, 100.0f);
}

FVector AArmaCycle::GetPredictPosition() const
{
	if (!CycleMovement)
		return GetActorLocation();

	// Predict position based on current velocity
	float Speed = CycleMovement->GetSpeed();
	FArmaCoord Dir = CycleMovement->GetDirection();
	float PredictTime = 0.1f;

	FVector CurrentPos = GetActorLocation();
	return CurrentPos + FVector(Dir.X * Speed * PredictTime, Dir.Y * Speed * PredictTime, 0.0f);
}

FVector AArmaCycle::GetCameraTop() const
{
	return FVector(0, 0, 1);
}

//////////////////////////////////////////////////////////////////////////
// Input
//////////////////////////////////////////////////////////////////////////

void AArmaCycle::TurnLeft()
{
	if (CycleMovement)
	{
		CycleMovement->Turn(1);
	}
}

void AArmaCycle::TurnRight()
{
	if (CycleMovement)
	{
		CycleMovement->Turn(-1);
	}
}

void AArmaCycle::StartBrake()
{
	if (CycleMovement)
	{
		CycleMovement->SetBraking(true);
	}
}

void AArmaCycle::StopBrake()
{
	if (CycleMovement)
	{
		CycleMovement->SetBraking(false);
	}
}

