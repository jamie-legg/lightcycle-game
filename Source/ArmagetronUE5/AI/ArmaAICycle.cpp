// ArmaAICycle.cpp - AI-controlled lightcycle implementation

#include "ArmaAICycle.h"
#include "Game/ArmaWallRegistry.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"

AArmaAICycle::AArmaAICycle()
{
	// AI uses different color
	CycleColor = AIColor;
	
	// AI doesn't need player input
	AutoPossessPlayer = EAutoReceiveInput::Disabled;
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}

void AArmaAICycle::BeginPlay()
{
	// CRITICAL: Set AI color BEFORE calling Super::BeginPlay()
	// Super::BeginPlay() creates the first wall segment using CycleColor
	// AIColor default value from UPROPERTY is now available
	CycleColor = FLinearColor(1.0f, 0.0f, 0.0f, 1.0f);  // Force bright red
	
	UE_LOG(LogTemp, Warning, TEXT("AI BeginPlay: Setting CycleColor to RED (%.1f, %.1f, %.1f)"), 
		CycleColor.R, CycleColor.G, CycleColor.B);
	
	Super::BeginPlay();
	
	// Scale reaction time by IQ (higher IQ = faster reaction)
	float IQFactor = FMath::Clamp(AIIQ / 100.0f, 0.1f, 2.0f);
	ReactionTime = ReactionTime / IQFactor;
	AIThinkInterval = AIThinkInterval / IQFactor;
	
	// Set initial think time
	NextThinkTime = GetWorld()->GetTimeSeconds() + AIThinkInterval;
	
	UE_LOG(LogTemp, Warning, TEXT("AI Cycle spawned: IQ=%d, ReactionTime=%.2f, Color=(%.1f,%.1f,%.1f)"), 
		AIIQ, ReactionTime, CycleColor.R, CycleColor.G, CycleColor.B);
}

void AArmaAICycle::Tick(float DeltaTime)
{
	float CurrentTime = GetWorld()->GetTimeSeconds();
	
	// DEBUG: Log AI position every second
	static float LastLogTime = 0;
	if (CurrentTime - LastLogTime > 1.0f)
	{
		FVector Pos = GetActorLocation();
		UE_LOG(LogTemp, Warning, TEXT("AI TICK: Pos=(%.1f, %.1f, %.1f) Alive=%d Speed=%.1f Dir=(%.2f,%.2f)"),
			Pos.X, Pos.Y, Pos.Z, bIsAlive, MoveSpeed, MoveDirection.X, MoveDirection.Y);
		LastLogTime = CurrentTime;
		
		// Check if outside bounds
		const float Boundary = 5000.0f;
		if (FMath::Abs(Pos.X) > Boundary || FMath::Abs(Pos.Y) > Boundary)
		{
			UE_LOG(LogTemp, Error, TEXT("AI OUTSIDE BOUNDARY! Clamping from (%.1f, %.1f)"), Pos.X, Pos.Y);
			Pos.X = FMath::Clamp(Pos.X, -4900.0f, 4900.0f);
			Pos.Y = FMath::Clamp(Pos.Y, -4900.0f, 4900.0f);
			SetActorLocation(Pos);
		}
	}
	
	// Check if waiting to respawn
	if (bWaitingToRespawn)
	{
		if (CurrentTime >= DeathTime + RespawnDelay)
		{
			AIRespawn();
		}
		return;
	}
	
	// Let parent handle movement and collision
	Super::Tick(DeltaTime);
	
	// Check if we died
	if (!bIsAlive && !bWaitingToRespawn)
	{
		bWaitingToRespawn = true;
		DeathTime = CurrentTime;
		UE_LOG(LogTemp, Warning, TEXT("AI died, will respawn in %.1f seconds"), RespawnDelay);
		return;
	}
	
	// AI thinking
	if (CurrentTime >= NextThinkTime && bIsAlive)
	{
		Think();
		NextThinkTime = CurrentTime + AIThinkInterval;
	}
	
	// Execute pending turn after reaction delay
	if (PendingTurn != 0 && CurrentTime >= TurnDecisionTime + ReactionTime)
	{
		ExecuteTurn(PendingTurn);
		PendingTurn = 0;
	}
}

void AArmaAICycle::Think()
{
	// Update sensors
	UpdateSensors();
	
	// State machine (simplified from Armagetron)
	switch (CurrentState)
	{
	case EArmaAIState::Survive:
		ThinkSurvive();
		break;
		
	case EArmaAIState::Trace:
		ThinkTrace();
		break;
		
	case EArmaAIState::CloseCombat:
		// TODO: Implement combat logic
		ThinkSurvive();
		break;
		
	case EArmaAIState::Path:
	case EArmaAIState::Route:
	default:
		ThinkSurvive();
		break;
	}
}

void AArmaAICycle::ThinkSurvive()
{
	// Emergency check first - is there a wall very close ahead?
	if (FrontSensor.bHit && FrontSensor.Distance < EmergencyDistance)
	{
		EmergencySurvive();
		return;
	}
	
	// Proactive avoidance - turn before we get too close
	float TurnThreshold = MoveSpeed * AIThinkInterval * 3.0f;  // 3 think intervals ahead
	
	if (FrontSensor.bHit && FrontSensor.Distance < TurnThreshold)
	{
		// Wall ahead, pick the better side
		int PreferredSide = 0;
		
		if (LeftSensor.Distance > RightSensor.Distance)
		{
			PreferredSide = -1;  // More room on left
		}
		else
		{
			PreferredSide = 1;   // More room on right
		}
		
		// Schedule the turn
		if (PendingTurn == 0)
		{
			PendingTurn = PreferredSide;
			TurnDecisionTime = GetWorld()->GetTimeSeconds();
		}
	}
}

void AArmaAICycle::ThinkTrace()
{
	// Trace mode: follow along a wall on one side
	// From Armagetron: used to grind walls for speed
	
	// Emergency check
	if (FrontSensor.bHit && FrontSensor.Distance < EmergencyDistance)
	{
		EmergencySurvive(TraceSide);
		return;
	}
	
	// Check if we're still near the wall we're tracing
	FArmaAISensorData& TracedSensor = (TraceSide > 0) ? RightSensor : LeftSensor;
	
	if (!TracedSensor.bHit || TracedSensor.Distance > SensorRange * 0.5f)
	{
		// Lost the wall, switch to survive
		CurrentState = EArmaAIState::Survive;
		return;
	}
	
	// If wall ahead, turn in trace direction
	if (FrontSensor.bHit && FrontSensor.Distance < MoveSpeed * AIThinkInterval * 5.0f)
	{
		if (PendingTurn == 0)
		{
			PendingTurn = TraceSide;
			TurnDecisionTime = GetWorld()->GetTimeSeconds();
		}
	}
}

bool AArmaAICycle::EmergencySurvive(int PreferredDirection)
{
	// Emergency! Pick the safest direction to turn
	
	float LeftSpace = LeftSensor.Distance;
	float RightSpace = RightSensor.Distance;
	float FrontSpace = FrontSensor.Distance;
	
	int TurnDirection = 0;
	
	// If one side is clearly better, go that way
	if (LeftSpace > RightSpace * 1.5f && LeftSpace > FrontSpace)
	{
		TurnDirection = -1;
	}
	else if (RightSpace > LeftSpace * 1.5f && RightSpace > FrontSpace)
	{
		TurnDirection = 1;
	}
	else if (PreferredDirection != 0)
	{
		// Use the preferred direction if no clear winner
		TurnDirection = PreferredDirection;
	}
	else
	{
		// Random choice
		TurnDirection = (FMath::FRand() > 0.5f) ? 1 : -1;
	}
	
	// Execute immediately in emergency (bypass reaction time)
	ExecuteTurn(TurnDirection);
	PendingTurn = 0;
	
	return true;
}

FArmaAISensorData AArmaAICycle::CastSensor(FVector Direction, float Range)
{
	FArmaAISensorData Result;
	
	FVector2D MyPos2D(GetActorLocation().X, GetActorLocation().Y);
	FVector2D Dir2D(Direction.X, Direction.Y);
	Dir2D.Normalize();
	
	const float WallGracePeriod = 0.3f;
	
	// Use global wall registry to check ALL walls (own, other players', and rim walls)
	UArmaWallRegistry* WallRegistry = UArmaWallRegistry::Get(GetWorld());
	if (WallRegistry)
	{
		FArmaRegisteredWall HitWall;
		float HitDist = WallRegistry->RaycastWalls(MyPos2D, Dir2D, Range, this, WallGracePeriod, HitWall);
		
		if (HitDist < MAX_FLT)
		{
			Result.Distance = HitDist;
			Result.bHit = true;
			FVector2D HitPt = MyPos2D + Dir2D * HitDist;
			Result.HitPoint = FVector(HitPt.X, HitPt.Y, GetActorLocation().Z);
			Result.bIsOwnWall = (HitWall.OwnerActor == this);
			Result.bIsRim = (HitWall.WallType == EArmaWallType::Rim);
		}
	}
	
	return Result;
}

void AArmaAICycle::UpdateSensors()
{
	// Cast sensors in three directions
	FVector Forward = MoveDirection;
	FVector Left = FVector(-MoveDirection.Y, MoveDirection.X, 0);
	FVector Right = FVector(MoveDirection.Y, -MoveDirection.X, 0);
	
	FrontSensor = CastSensor(Forward, SensorRange);
	LeftSensor = CastSensor(Left, SensorRange * 0.5f);
	RightSensor = CastSensor(Right, SensorRange * 0.5f);
	
	// Debug visualization
	if (bDebugDrawEnabled)
	{
		UWorld* World = GetWorld();
		if (World)
		{
			FVector Start = GetActorLocation();
			
			// Front - red if close, green if far
			FColor FrontColor = FrontSensor.bHit ? 
				(FrontSensor.Distance < EmergencyDistance ? FColor::Red : FColor::Yellow) : FColor::Green;
			DrawDebugLine(World, Start, Start + Forward * FMath::Min(FrontSensor.Distance, SensorRange), 
				FrontColor, false, -1.0f, 0, 3.0f);
			
			// Left - cyan
			DrawDebugLine(World, Start, Start + Left * FMath::Min(LeftSensor.Distance, SensorRange * 0.5f), 
				FColor::Cyan, false, -1.0f, 0, 2.0f);
			
			// Right - magenta
			DrawDebugLine(World, Start, Start + Right * FMath::Min(RightSensor.Distance, SensorRange * 0.5f), 
				FColor::Magenta, false, -1.0f, 0, 2.0f);
		}
	}
}

void AArmaAICycle::ExecuteTurn(int Direction)
{
	if (Direction < 0)
	{
		TurnLeft();
	}
	else if (Direction > 0)
	{
		TurnRight();
	}
}

void AArmaAICycle::AIRespawn()
{
	UE_LOG(LogTemp, Warning, TEXT("AI Respawning..."));
	
	// Clear waiting flag
	bWaitingToRespawn = false;
	
	// Call parent respawn
	Respawn();
	
	// Reset state
	CurrentState = EArmaAIState::Survive;
	PendingTurn = 0;
	NextThinkTime = GetWorld()->GetTimeSeconds() + AIThinkInterval;
}

