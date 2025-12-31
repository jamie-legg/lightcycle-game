// ArmaCycleMovement.cpp - Implementation of cycle physics

#include "ArmaCycleMovement.h"
#include "ArmaCycle.h"
#include "ArmaWall.h"
#include "Core/ArmaGrid.h"
#include "Kismet/GameplayStatics.h"

// Static member initialization
float UArmaCycleMovementComponent::SpeedMultiplier = 1.0f;
float UArmaCycleMovementComponent::RubberSpeed = ArmaPhysics::DefaultRubberSpeed;

// Global turn speed factor
float GetTurnSpeedFactor()
{
	return UArmaCycleMovementComponent::GetSpeedMultiplier();
}

//////////////////////////////////////////////////////////////////////////
// UArmaCycleMovementComponent
//////////////////////////////////////////////////////////////////////////

UArmaCycleMovementComponent::UArmaCycleMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;

	// Initialize default values
	AliveState = 1;
	CurrentSpeed = ArmaPhysics::DefaultSpeed;
	VerletSpeed = ArmaPhysics::DefaultSpeed;
	Acceleration = 0.0f;
	LastTimestep = 0.0f;
	Distance = 0.0f;
	TurnCount = 0;
	WindingNumber = 0;
	WindingNumberWrapped = 0;
	LastTurnTimeRight = -1000.0f;
	LastTurnTimeLeft = -1000.0f;
	LastTimeAlive = 0.0f;

	bBraking = false;
	BrakingReservoir = ArmaPhysics::DefaultBrakingReservoir;
	BrakeUsage = 0.0f;

	Rubber = ArmaPhysics::DefaultRubber;
	RubberMalus = 0.0f;
	RubberSpeedFactor = 1.0f;
	RubberDepleteTime = 0.0f;
	RubberUsage = 0.0f;

	bRefreshSpaceAhead = true;
	CachedMaxSpaceAhead = 1000.0f;
	MaxSpaceMaxCast = 1000.0f;

	Gap[0] = Gap[1] = 0.0f;
	bKeepLookingForGap[0] = bKeepLookingForGap[1] = false;
	bGapIsBackdoor[0] = bGapIsBackdoor[1] = false;

	DirDrive = FArmaCoord::UnitX;
	LastDirDrive = FArmaCoord::UnitX;
	LastTurnPos = FArmaCoord::Zero;

	CurrentDestination = nullptr;
	LastDestination = nullptr;
	OwnerCycle = nullptr;
	GridSubsystem = nullptr;
}

void UArmaCycleMovementComponent::BeginPlay()
{
	Super::BeginPlay();

	InitializeMovement();

	// Get grid subsystem
	GridSubsystem = GetWorld()->GetSubsystem<UArmaGridSubsystem>();

	// Get owner cycle
	OwnerCycle = Cast<AArmaCycle>(GetOwner());
}

void UArmaCycleMovementComponent::InitializeMovement()
{
	if (AActor* Owner = GetOwner())
	{
		// Initialize position and direction from actor
		FVector Loc = Owner->GetActorLocation();
		FVector Forward = Owner->GetActorForwardVector();
		
		LastTurnPos = FArmaCoord(Loc.X, Loc.Y);
		DirDrive = FArmaCoord(Forward.X, Forward.Y).Normalized();
		LastDirDrive = DirDrive;

		// Get winding number from grid
		if (GridSubsystem)
		{
			WindingNumber = GridSubsystem->GetDirectionWinding(DirDrive);
			WindingNumberWrapped = WindingNumber;
		}
	}
}

void UArmaCycleMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!IsAlive())
		return;

	float CurrentTime = GetWorld()->GetTimeSeconds();
	
	// Execute pending turns
	while (PendingTurns.Num() > 0 && CanMakeTurn(PendingTurns[0]))
	{
		DoTurn(PendingTurns[0]);
		PendingTurns.RemoveAt(0);
	}

	// Perform timestep
	TimestepCore(CurrentTime, true);

	// Update Unreal transform
	if (AActor* Owner = GetOwner())
	{
		FVector NewLocation = FVector(
			Owner->GetActorLocation().X + DirDrive.X * CurrentSpeed * DeltaTime,
			Owner->GetActorLocation().Y + DirDrive.Y * CurrentSpeed * DeltaTime,
			Owner->GetActorLocation().Z
		);
		
		Owner->SetActorLocation(NewLocation);
		
		// Set rotation to face movement direction
		FRotator NewRotation = FRotator(0.0f, FMath::RadiansToDegrees(FMath::Atan2(DirDrive.Y, DirDrive.X)), 0.0f);
		Owner->SetActorRotation(NewRotation);
	}
}

bool UArmaCycleMovementComponent::TimestepCore(float CurrentTime, bool bCalculateAcceleration)
{
	if (!IsAlive())
		return false;

	float DeltaTime = CurrentTime - LastTimeAlive;
	if (DeltaTime <= 0.0f)
		return true;

	// Calculate acceleration if requested
	if (bCalculateAcceleration)
	{
		CalculateAcceleration();
	}

	// Apply acceleration
	ApplyAcceleration(DeltaTime);

	// Update distance
	Distance += CurrentSpeed * DeltaTime;

	// Store for next frame
	LastTimestep = DeltaTime;
	LastTimeAlive = CurrentTime;

	// Mark space ahead as needing refresh
	bRefreshSpaceAhead = true;

	return true;
}

void UArmaCycleMovementComponent::CalculateAcceleration()
{
	// Base acceleration from game settings
	float BaseAccel = 10.0f * SpeedMultiplier;

	// Brake modifier
	if (bBraking && BrakingReservoir > 0.0f)
	{
		Acceleration = -BaseAccel * 2.0f;  // Braking decelerates
		BrakeUsage = 1.0f;
	}
	else
	{
		// Normal acceleration toward max speed
		float TargetSpeed = GetMaximalSpeed();
		float SpeedDiff = TargetSpeed - CurrentSpeed;
		Acceleration = FMath::Clamp(SpeedDiff * 0.5f, -BaseAccel, BaseAccel);
		BrakeUsage = 0.0f;
	}

	// Apply rubber effects
	float MaxSpace = GetMaxSpaceAhead(100.0f);
	if (MaxSpace < 1.0f)
	{
		// Very close to wall - rubber slows us down
		RubberSpeedFactor = FMath::Max(0.1f, MaxSpace);
		Acceleration -= (1.0f - RubberSpeedFactor) * RubberSpeed;
		RubberUsage = 1.0f - RubberSpeedFactor;
	}
	else
	{
		RubberSpeedFactor = 1.0f;
		RubberUsage = 0.0f;
	}
}

void UArmaCycleMovementComponent::ApplyAcceleration(float DeltaTime)
{
	// Verlet integration for smoother physics
	VerletSpeed = CurrentSpeed;
	CurrentSpeed += Acceleration * DeltaTime;

	// Clamp speed
	float MaxSpeed = GetMaximalSpeed();
	CurrentSpeed = FMath::Clamp(CurrentSpeed, 0.0f, MaxSpeed);

	// Update braking reservoir
	if (bBraking && BrakeUsage > 0.0f)
	{
		BrakingReservoir -= BrakeUsage * DeltaTime * 0.5f;
		BrakingReservoir = FMath::Max(0.0f, BrakingReservoir);
	}
	else if (!bBraking)
	{
		// Regenerate braking reservoir
		BrakingReservoir += DeltaTime * 0.2f;
		BrakingReservoir = FMath::Min(1.0f, BrakingReservoir);
	}

	// Update rubber
	if (RubberUsage > 0.0f)
	{
		Rubber += RubberUsage * DeltaTime;
		if (Rubber >= 1.0f && RubberDepleteTime <= 0.0f)
		{
			RubberDepleteTime = GetWorld()->GetTimeSeconds();
		}
	}
}

void UArmaCycleMovementComponent::AccelerationDiscontinuity()
{
	// Called when acceleration makes a sharp jump (e.g., after turn)
	bRefreshSpaceAhead = true;
}

float UArmaCycleMovementComponent::GetMaximalSpeed()
{
	return ArmaPhysics::DefaultSpeed * SpeedMultiplier * 2.0f;
}

bool UArmaCycleMovementComponent::CanMakeTurn(int32 Direction) const
{
	return CanMakeTurnAtTime(GetWorld()->GetTimeSeconds(), Direction);
}

bool UArmaCycleMovementComponent::CanMakeTurnAtTime(float Time, int32 Direction) const
{
	if (!IsAlive())
		return false;

	// Check turn delay
	float LastTurnTime = (Direction > 0) ? LastTurnTimeLeft : LastTurnTimeRight;
	float OtherLastTurn = (Direction > 0) ? LastTurnTimeRight : LastTurnTimeLeft;

	float Delay = GetTurnDelay();
	float DelayDb = GetTurnDelayDb();

	// Can't turn if too soon after any turn
	if (Time < FMath::Max(LastTurnTime + Delay, OtherLastTurn + DelayDb))
		return false;

	// Check minimum distance since last turn
	float MinDist = CurrentSpeed * 0.05f;  // Minimum 50ms worth of distance
	if (GetDistanceSinceLastTurn() < MinDist)
		return false;

	return true;
}

float UArmaCycleMovementComponent::GetTurnDelay() const
{
	return ArmaPhysics::DefaultTurnDelay / SpeedMultiplier;
}

float UArmaCycleMovementComponent::GetTurnDelayDb() const
{
	return ArmaPhysics::DefaultTurnDelayDb / SpeedMultiplier;
}

float UArmaCycleMovementComponent::GetNextTurnTime(int32 Direction) const
{
	float LastTurnTime = (Direction > 0) ? LastTurnTimeLeft : LastTurnTimeRight;
	float OtherLastTurn = (Direction > 0) ? LastTurnTimeRight : LastTurnTimeLeft;

	return FMath::Max(LastTurnTime + GetTurnDelay(), OtherLastTurn + GetTurnDelayDb());
}

bool UArmaCycleMovementComponent::Turn(int32 Direction)
{
	if (Direction == 0)
		return false;

	if (CanMakeTurn(Direction))
	{
		return DoTurn(Direction);
	}
	else
	{
		// Queue the turn
		PendingTurns.Add(Direction);
		return false;
	}
}

bool UArmaCycleMovementComponent::DoTurn(int32 Direction)
{
	if (!GridSubsystem)
		return false;

	// Store last direction
	LastDirDrive = DirDrive;

	// Calculate new direction
	WindingNumber = GridSubsystem->Turn(WindingNumber, Direction);
	WindingNumberWrapped = ((WindingNumber % GridSubsystem->GetWindingNumber()) + GridSubsystem->GetWindingNumber()) % GridSubsystem->GetWindingNumber();
	DirDrive = GridSubsystem->GetDirection(WindingNumber);

	// Store turn position and time
	if (AActor* Owner = GetOwner())
	{
		FVector Loc = Owner->GetActorLocation();
		LastTurnPos = FArmaCoord(Loc.X, Loc.Y);
	}

	float CurrentTime = GetWorld()->GetTimeSeconds();
	if (Direction > 0)
		LastTurnTimeLeft = CurrentTime;
	else
		LastTurnTimeRight = CurrentTime;

	TurnCount++;

	// Signal acceleration discontinuity
	AccelerationDiscontinuity();

	// Broadcast turn event
	OnTurn.Broadcast(Direction);

	return true;
}

float UArmaCycleMovementComponent::GetDistanceSinceLastTurn() const
{
	return DoGetDistanceSinceLastTurn();
}

float UArmaCycleMovementComponent::DoGetDistanceSinceLastTurn() const
{
	if (AActor* Owner = GetOwner())
	{
		FVector Loc = Owner->GetActorLocation();
		FArmaCoord CurrentPos(Loc.X, Loc.Y);
		return (CurrentPos - LastTurnPos).Norm();
	}
	return 0.0f;
}

float UArmaCycleMovementComponent::GetMaxSpaceAhead(float MaxReport) const
{
	if (!bRefreshSpaceAhead && MaxReport <= MaxSpaceMaxCast)
	{
		return FMath::Min(CachedMaxSpaceAhead, MaxReport);
	}

	// Perform raycast to find distance to nearest wall
	if (AActor* Owner = GetOwner())
	{
		FVector Start = Owner->GetActorLocation();
		FVector End = Start + FVector(DirDrive.X, DirDrive.Y, 0.0f) * MaxReport;

		FHitResult HitResult;
		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(Owner);

		if (GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, QueryParams))
		{
			CachedMaxSpaceAhead = HitResult.Distance;
		}
		else
		{
			CachedMaxSpaceAhead = MaxReport;
		}
	}
	else
	{
		CachedMaxSpaceAhead = MaxReport;
	}

	MaxSpaceMaxCast = MaxReport;
	bRefreshSpaceAhead = false;

	return FMath::Min(CachedMaxSpaceAhead, MaxReport);
}

bool UArmaCycleMovementComponent::IsVulnerable() const
{
	// Check if cycle is in a grace period after spawn
	float CurrentTime = GetWorld()->GetTimeSeconds();
	// Could add spawn invulnerability here
	return IsAlive();
}

void UArmaCycleMovementComponent::Die(float Time)
{
	if (AliveState <= 0)
		return;

	AliveState = -1;  // Just died

	// Broadcast death event
	OnDeath.Broadcast(Time);

	// Set to fully dead after a frame
	GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
	{
		AliveState = 0;
	});
}

bool UArmaCycleMovementComponent::EdgeIsDangerous(AArmaWall* Wall, float Time, float Alpha) const
{
	if (!Wall)
		return false;

	// Own walls are less dangerous
	if (Wall->GetOwnerCycle() == OwnerCycle)
	{
		// But can still kill if too close
		return Alpha > 0.99f;
	}

	// All other walls are dangerous
	return true;
}

void UArmaCycleMovementComponent::PassEdge(AArmaWall* Wall, float Time, float Alpha, int32 Recursion)
{
	if (!Wall || !IsVulnerable())
		return;

	if (EdgeIsDangerous(Wall, Time, Alpha))
	{
		RightBeforeDeath(Recursion);
		
		if (Recursion <= 0)
		{
			Die(Time);
		}
	}
}

void UArmaCycleMovementComponent::RightBeforeDeath(int32 NumTries)
{
	// Last chance to avoid death
	// Could try emergency turns here
}

void UArmaCycleMovementComponent::MoveSafely(const FArmaCoord& Dest, float StartTime, float EndTime)
{
	// Move without triggering death exceptions
	if (AActor* Owner = GetOwner())
	{
		Owner->SetActorLocation(FVector(Dest.X, Dest.Y, Owner->GetActorLocation().Z));
	}
}

void UArmaCycleMovementComponent::AddDestination()
{
	if (!GetOwner())
		return;

	FArmaDestination NewDest;
	FVector Loc = GetOwner()->GetActorLocation();
	NewDest.Position = FArmaCoord(Loc.X, Loc.Y);
	NewDest.Direction = DirDrive;
	NewDest.GameTime = GetWorld()->GetTimeSeconds();
	NewDest.Distance = Distance;
	NewDest.Speed = CurrentSpeed;
	NewDest.bBraking = bBraking;
	NewDest.Turns = TurnCount;
	NewDest.bHasBeenUsed = false;

	DestinationList.Add(NewDest);
}

void UArmaCycleMovementComponent::AdvanceDestination()
{
	if (CurrentDestination)
	{
		LastDestination = CurrentDestination;
		CurrentDestination->bHasBeenUsed = true;
	}

	// Find next unused destination
	for (FArmaDestination& Dest : DestinationList)
	{
		if (!Dest.bHasBeenUsed)
		{
			CurrentDestination = &Dest;
			return;
		}
	}

	CurrentDestination = nullptr;
}

//////////////////////////////////////////////////////////////////////////
// FArmaEnemyInfluence
//////////////////////////////////////////////////////////////////////////

void FArmaEnemyInfluence::AddInfluence(AArmaCycle* Enemy, float Time, float TimePenalty)
{
	if (!Enemy)
		return;

	float EffectiveTime = Time - TimePenalty;

	// Only update if this is more recent
	if (EffectiveTime > LastTime)
	{
		LastEnemy = Enemy;
		LastTime = EffectiveTime;
	}
}

