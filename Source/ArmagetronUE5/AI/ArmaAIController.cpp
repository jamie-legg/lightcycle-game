// ArmaAIController.cpp - Implementation of AI state machine and behavior

#include "ArmaAIController.h"
#include "Game/ArmaCycle.h"
#include "Game/ArmaCycleMovement.h"
#include "Game/ArmaWall.h"
#include "Core/ArmaGrid.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

//////////////////////////////////////////////////////////////////////////
// FArmaAISensor Implementation
//////////////////////////////////////////////////////////////////////////

void FArmaAISensor::PerformCast(UWorld* World, AArmaCycle* OwnerCycle, const FArmaCoord& InOrigin, const FArmaCoord& InDir, float MaxDistance)
{
	Origin = InOrigin;
	Direction = InDir.Normalized();
	Distance = MaxDistance;
	HitWall = nullptr;
	HitCycle = nullptr;
	bHitOwnWall = false;
	Danger = 0.0f;

	if (!World || !OwnerCycle)
		return;

	FVector Start(Origin.X, Origin.Y, 1.0f);  // Slightly above ground
	FVector End = Start + FVector(Direction.X, Direction.Y, 0.0f) * MaxDistance;

	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(OwnerCycle);
	QueryParams.bTraceComplex = true;

	if (World->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, QueryParams))
	{
		Distance = HitResult.Distance;
		HitPoint = FArmaCoord(HitResult.ImpactPoint.X, HitResult.ImpactPoint.Y);

		// Check what we hit
		if (AActor* HitActor = HitResult.GetActor())
		{
			if (AArmaWall* Wall = Cast<AArmaWall>(HitActor))
			{
				HitWall = Wall;
				bHitOwnWall = (Wall->GetOwnerCycle() == OwnerCycle);
			}
			else if (AArmaCycle* Cycle = Cast<AArmaCycle>(HitActor))
			{
				HitCycle = Cycle;
			}
		}
	}
	else
	{
		HitPoint = Origin + Direction * MaxDistance;
	}

	// Calculate danger based on distance
	if (Distance < 10.0f)
		Danger = 1.0f;
	else if (Distance < 50.0f)
		Danger = (50.0f - Distance) / 40.0f;
	else
		Danger = 0.0f;
}

//////////////////////////////////////////////////////////////////////////
// AArmaAIController Implementation
//////////////////////////////////////////////////////////////////////////

AArmaAIController::AArmaAIController()
{
	PrimaryActorTick.bCanEverTick = true;

	CurrentState = EArmaAIState::Survive;
	TraceSide = 1;
	LastChangeAttempt = 0.0f;
	LazySideChange = 0.0f;
	NextStateChange = 0.0f;
	bEmergency = false;
	TriesLeft = 3;
	FreeSide = 0.0f;
	LastThinkTime = 0.0f;
	NextThinkTime = 0.0f;
	Concentration = 1.0f;
	LastPathTime = 0.0f;
	CurrentRouteIndex = 0;

	// Default AI character
	AICharacterSettings.Name = TEXT("Bot");
	AICharacterSettings.IQ = 100.0f;
}

void AArmaAIController::BeginPlay()
{
	Super::BeginPlay();

	GridSubsystem = GetWorld()->GetSubsystem<UArmaGridSubsystem>();
	RandomStream.GenerateNewSeed();
}

void AArmaAIController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	float CurrentTime = GetWorld()->GetTimeSeconds();

	// Check if it's time to think
	if (CurrentTime >= NextThinkTime)
	{
		float ThinkDelay = Think();
		NextThinkTime = CurrentTime + ThinkDelay;
	}
}

void AArmaAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// Initialize AI for this cycle
	if (AArmaCycle* Cycle = Cast<AArmaCycle>(InPawn))
	{
		// Could configure based on character
	}
}

void AArmaAIController::OnUnPossess()
{
	Super::OnUnPossess();
}

void AArmaAIController::SwitchToState(EArmaAIState NewState, float MinTime)
{
	CurrentState = NewState;
	NextStateChange = GetWorld()->GetTimeSeconds() + MinTime;

	// Reset state-specific variables
	bEmergency = false;
	TriesLeft = 3;
}

void AArmaAIController::AddWaypoint(const FArmaCoord& Point)
{
	RoutePoints.Add(Point);
}

void AArmaAIController::SetRoute(const TArray<FArmaCoord>& Route)
{
	RoutePoints = Route;
	CurrentRouteIndex = 0;
}

void AArmaAIController::ClearRoute()
{
	RoutePoints.Empty();
	CurrentRouteIndex = 0;
}

void AArmaAIController::SetNumberOfAIs(int32 Num, int32 MinPlayers, int32 IQ, int32 Tries)
{
	// Would spawn/despawn AI controllers as needed
}

void AArmaAIController::ConfigureAIs()
{
	// Configuration menu handling
}

void AArmaAIController::CycleBlocksWay(AArmaCycle* A, AArmaCycle* B, int32 ADir, int32 BDir, float BDist, int32 Winding)
{
	// Update AI memory when cycles get close
	if (A && A->Memory.Entries.Num() > 0)
	{
		if (FCycleMemoryEntry* Entry = A->Memory.Remember(B))
		{
			Entry->Side = ADir;
			Entry->Distance = BDist;
			Entry->Time = A->GetWorld()->GetTimeSeconds();
		}
	}
}

void AArmaAIController::CycleBlocksRim(AArmaCycle* A, int32 ADir)
{
	// Handle rim wall blocking
}

void AArmaAIController::BreakWall(AArmaCycle* A, float ADist)
{
	// Handle wall destruction
}

float AArmaAIController::Think()
{
	AArmaCycle* Cycle = GetCycle();
	if (!Cycle || !Cycle->IsAlive())
		return 1.0f;

	// Cast sensors
	FArmaAIThinkData Data;
	CastSensors(Data);

	// Check for emergency
	bEmergency = (Data.Front.Distance < 20.0f || Data.Left.Distance < 5.0f || Data.Right.Distance < 5.0f);

	// Think based on state
	if (bEmergency)
	{
		// Emergency handling
		switch (CurrentState)
		{
		case EArmaAIState::Survive:
			EmergencySurvive(Data);
			break;
		case EArmaAIState::Trace:
			EmergencyTrace(Data);
			break;
		case EArmaAIState::Path:
			EmergencyPath(Data);
			break;
		case EArmaAIState::CloseCombat:
			EmergencyCloseCombat(Data);
			break;
		case EArmaAIState::Route:
			EmergencyRoute(Data);
			break;
		}
	}
	else
	{
		// Normal thinking
		switch (CurrentState)
		{
		case EArmaAIState::Survive:
			ThinkSurvive(Data);
			break;
		case EArmaAIState::Trace:
			ThinkTrace(Data);
			break;
		case EArmaAIState::Path:
			ThinkPath(Data);
			break;
		case EArmaAIState::CloseCombat:
			ThinkCloseCombat(Data);
			break;
		case EArmaAIState::Route:
			ThinkRoute(Data);
			break;
		}
	}

	// Act on gathered data
	ActOnData(Data);

	// Return time until next think (based on IQ and concentration)
	float BaseDelay = 0.1f;
	float IQFactor = 200.0f / FMath::Max(AICharacterSettings.IQ, 50.0f);
	return FMath::Max(0.01f, BaseDelay * IQFactor * (1.0f / Concentration));
}

void AArmaAIController::ThinkSurvive(FArmaAIThinkData& Data)
{
	// Survival mode - just avoid walls

	// If both sides are clear, continue straight
	if (Data.Left.Distance > 30.0f && Data.Right.Distance > 30.0f)
	{
		// Maybe turn toward more open space
		if (Data.Front.Distance < 100.0f)
		{
			Data.Turn = FindBestTurn(Data);
		}
	}
	else if (Data.Front.Distance < 50.0f)
	{
		// Wall ahead - pick a side
		Data.Turn = FindBestTurn(Data);
	}

	// Consider switching to trace or attack mode
	float CurrentTime = GetWorld()->GetTimeSeconds();
	if (CurrentTime > NextStateChange)
	{
		// Maybe switch to tracing if we're near a wall
		if (Data.Left.Distance < 30.0f || Data.Right.Distance < 30.0f)
		{
			// Pick side to trace
			TraceSide = (Data.Left.Distance < Data.Right.Distance) ? -1 : 1;
			SwitchToState(EArmaAIState::Trace, 5.0f);
		}
		// Or look for targets
		else if (GetDistanceToNearestEnemy() < 200.0f)
		{
			SwitchToState(EArmaAIState::CloseCombat, 5.0f);
		}
	}
}

void AArmaAIController::ThinkTrace(FArmaAIThinkData& Data)
{
	// Trace mode - follow a wall

	FArmaAISensor& TraceSensor = (TraceSide > 0) ? Data.Left : Data.Right;
	FArmaAISensor& OtherSensor = (TraceSide > 0) ? Data.Right : Data.Left;

	// If wall we're tracing is far, turn toward it
	if (TraceSensor.Distance > 20.0f)
	{
		Data.Turn = TraceSide;
	}
	// If wall ahead, turn away from traced wall
	else if (Data.Front.Distance < 30.0f)
	{
		Data.Turn = -TraceSide;
	}

	// Consider changing trace side
	float CurrentTime = GetWorld()->GetTimeSeconds();
	if (CurrentTime - LastChangeAttempt > 2.0f && TraceSensor.Distance > 50.0f)
	{
		LastChangeAttempt = CurrentTime;
		TraceSide = -TraceSide;
	}

	// Consider switching out of trace mode
	if (CurrentTime > NextStateChange && TraceSensor.Distance > 100.0f)
	{
		SwitchToState(EArmaAIState::Survive, 5.0f);
	}
}

void AArmaAIController::ThinkPath(FArmaAIThinkData& Data)
{
	// Pathfinding mode - follow path to target

	if (Path.Num() == 0 || !Target.IsValid())
	{
		SwitchToState(EArmaAIState::Survive, 5.0f);
		return;
	}

	AArmaCycle* Cycle = GetCycle();
	if (!Cycle)
		return;

	// Get current path point
	FArmaCoord CurrentPoint = Path[0];
	FVector CyclePos = Cycle->GetActorLocation();
	FArmaCoord CycleCoord(CyclePos.X, CyclePos.Y);

	// Calculate direction to path point
	FArmaCoord ToPoint = CurrentPoint - CycleCoord;
	float DistToPoint = ToPoint.Norm();

	// If close to point, advance to next
	if (DistToPoint < 20.0f)
	{
		Path.RemoveAt(0);
		if (Path.Num() == 0)
		{
			SwitchToState(EArmaAIState::CloseCombat, 5.0f);
			return;
		}
	}

	// Turn toward path point
	if (UArmaCycleMovementComponent* Movement = Cycle->GetCycleMovement())
	{
		FArmaCoord Dir = Movement->GetDirection();
		float Cross = Dir.Cross(ToPoint);

		if (Cross > 0.3f)
			Data.Turn = 1;  // Turn left
		else if (Cross < -0.3f)
			Data.Turn = -1;  // Turn right
	}
}

void AArmaAIController::ThinkCloseCombat(FArmaAIThinkData& Data)
{
	// Close combat mode - aggressive pursuit

	if (!Target.IsValid())
	{
		// Find new target
		TArray<AActor*> Cycles;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), AArmaCycle::StaticClass(), Cycles);

		AArmaCycle* MyCycle = GetCycle();
		float BestDist = FLT_MAX;

		for (AActor* Actor : Cycles)
		{
			AArmaCycle* OtherCycle = Cast<AArmaCycle>(Actor);
			if (OtherCycle && OtherCycle != MyCycle && OtherCycle->IsAlive())
			{
				float Dist = FVector::Distance(MyCycle->GetActorLocation(), OtherCycle->GetActorLocation());
				if (Dist < BestDist)
				{
					BestDist = Dist;
					Target = OtherCycle;
				}
			}
		}

		if (!Target.IsValid())
		{
			SwitchToState(EArmaAIState::Survive, 5.0f);
			return;
		}
	}

	AArmaCycle* Cycle = GetCycle();
	if (!Cycle)
		return;

	// Calculate direction to target
	FVector TargetPos = Target->GetActorLocation();
	FVector CyclePos = Cycle->GetActorLocation();
	FArmaCoord ToTarget(TargetPos.X - CyclePos.X, TargetPos.Y - CyclePos.Y);

	if (UArmaCycleMovementComponent* Movement = Cycle->GetCycleMovement())
	{
		FArmaCoord Dir = Movement->GetDirection();
		float Cross = Dir.Cross(ToTarget);

		// Turn toward target (but not too aggressively)
		if (Cross > 0.5f && Data.Left.Distance > 20.0f)
			Data.Turn = 1;
		else if (Cross < -0.5f && Data.Right.Distance > 20.0f)
			Data.Turn = -1;
	}

	// If target is far, switch to path mode
	float DistToTarget = ToTarget.Norm();
	if (DistToTarget > 300.0f)
	{
		SwitchToState(EArmaAIState::Path, 5.0f);
	}
}

void AArmaAIController::ThinkRoute(FArmaAIThinkData& Data)
{
	// Route mode - follow predefined route

	if (RoutePoints.Num() == 0 || CurrentRouteIndex >= RoutePoints.Num())
	{
		SwitchToState(EArmaAIState::Survive, 5.0f);
		return;
	}

	AArmaCycle* Cycle = GetCycle();
	if (!Cycle)
		return;

	FArmaCoord RouteTarget = RoutePoints[CurrentRouteIndex];
	FVector CyclePos = Cycle->GetActorLocation();
	FArmaCoord CycleCoord(CyclePos.X, CyclePos.Y);

	FArmaCoord ToTarget = RouteTarget - CycleCoord;
	float Dist = ToTarget.Norm();

	// Advance to next waypoint if close
	if (Dist < 30.0f)
	{
		CurrentRouteIndex++;
		if (CurrentRouteIndex >= RoutePoints.Num())
		{
			SwitchToState(EArmaAIState::Survive, 5.0f);
			return;
		}
	}

	// Turn toward waypoint
	if (UArmaCycleMovementComponent* Movement = Cycle->GetCycleMovement())
	{
		FArmaCoord Dir = Movement->GetDirection();
		float Cross = Dir.Cross(ToTarget);

		if (Cross > 0.3f)
			Data.Turn = 1;
		else if (Cross < -0.3f)
			Data.Turn = -1;
	}
}

bool AArmaAIController::EmergencySurvive(FArmaAIThinkData& Data, int32 EnemyEvade, int32 PreferredSide)
{
	// Emergency - find any safe direction

	// Try preferred side first
	if (PreferredSide != 0)
	{
		FArmaAISensor& PrefSensor = (PreferredSide > 0) ? Data.Left : Data.Right;
		if (PrefSensor.Distance > 10.0f)
		{
			Data.Turn = PreferredSide;
			return true;
		}
	}

	// Try left
	if (Data.Left.Distance > Data.Right.Distance && Data.Left.Distance > 10.0f)
	{
		Data.Turn = 1;
		return true;
	}

	// Try right
	if (Data.Right.Distance > 10.0f)
	{
		Data.Turn = -1;
		return true;
	}

	// No safe direction - we're likely dead
	TriesLeft--;
	return false;
}

void AArmaAIController::EmergencyTrace(FArmaAIThinkData& Data)
{
	EmergencySurvive(Data, -1, -TraceSide);
}

void AArmaAIController::EmergencyPath(FArmaAIThinkData& Data)
{
	EmergencySurvive(Data);
}

void AArmaAIController::EmergencyCloseCombat(FArmaAIThinkData& Data)
{
	EmergencySurvive(Data);
}

void AArmaAIController::EmergencyRoute(FArmaAIThinkData& Data)
{
	EmergencySurvive(Data);
}

void AArmaAIController::ActOnData(FArmaAIThinkData& Data)
{
	AArmaCycle* Cycle = GetCycle();
	if (!Cycle || !Cycle->IsAlive())
		return;

	// Execute turn if requested
	if (Data.Turn != 0)
	{
		if (Data.Turn > 0)
			Cycle->TurnLeft();
		else
			Cycle->TurnRight();
	}
}

void AArmaAIController::SetTraceSide(int32 Side)
{
	TraceSide = (Side > 0) ? 1 : -1;
}

void AArmaAIController::CastSensors(FArmaAIThinkData& Data)
{
	AArmaCycle* Cycle = GetCycle();
	if (!Cycle)
		return;

	FVector Pos = Cycle->GetActorLocation();
	FArmaCoord Origin(Pos.X, Pos.Y);

	UArmaCycleMovementComponent* Movement = Cycle->GetCycleMovement();
	if (!Movement)
		return;

	FArmaCoord Dir = Movement->GetDirection();
	FArmaCoord LeftDir = Dir.Turn(1);
	FArmaCoord RightDir = Dir.Turn(-1);

	float SensorRange = 200.0f;

	// Cast front sensor
	Data.Front.PerformCast(GetWorld(), Cycle, Origin, Dir, SensorRange);

	// Cast left sensor
	Data.Left.PerformCast(GetWorld(), Cycle, Origin, LeftDir, SensorRange);

	// Cast right sensor
	Data.Right.PerformCast(GetWorld(), Cycle, Origin, RightDir, SensorRange);
}

int32 AArmaAIController::FindBestTurn(const FArmaAIThinkData& Data) const
{
	// Pick the direction with more space
	if (Data.Left.Distance > Data.Right.Distance + 10.0f)
		return 1;  // Turn left
	else if (Data.Right.Distance > Data.Left.Distance + 10.0f)
		return -1;  // Turn right
	else
	{
		// Similar space - random choice with bias based on FreeSide
		if (FreeSide > 0.1f)
			return 1;
		else if (FreeSide < -0.1f)
			return -1;
		else
			return (RandomStream.FRand() > 0.5f) ? 1 : -1;
	}
}

bool AArmaAIController::IsTurnSafe(int32 Direction, float LookAhead) const
{
	AArmaCycle* Cycle = GetCycle();
	if (!Cycle)
		return false;

	FVector Pos = Cycle->GetActorLocation();
	UArmaCycleMovementComponent* Movement = Cycle->GetCycleMovement();
	if (!Movement)
		return false;

	FArmaCoord Dir = Movement->GetDirection();
	FArmaCoord NewDir = Dir.Turn(Direction);

	FArmaAISensor Sensor;
	Sensor.PerformCast(GetWorld(), Cycle, FArmaCoord(Pos.X, Pos.Y), NewDir, LookAhead);

	return Sensor.Distance > 20.0f;
}

float AArmaAIController::GetDistanceToNearestEnemy() const
{
	AArmaCycle* Cycle = GetCycle();
	if (!Cycle)
		return FLT_MAX;

	TArray<AActor*> Cycles;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AArmaCycle::StaticClass(), Cycles);

	float BestDist = FLT_MAX;
	FVector MyPos = Cycle->GetActorLocation();

	for (AActor* Actor : Cycles)
	{
		AArmaCycle* OtherCycle = Cast<AArmaCycle>(Actor);
		if (OtherCycle && OtherCycle != Cycle && OtherCycle->IsAlive())
		{
			float Dist = FVector::Distance(MyPos, OtherCycle->GetActorLocation());
			if (Dist < BestDist)
			{
				BestDist = Dist;
			}
		}
	}

	return BestDist;
}

AArmaCycle* AArmaAIController::GetCycle() const
{
	return Cast<AArmaCycle>(GetPawn());
}

