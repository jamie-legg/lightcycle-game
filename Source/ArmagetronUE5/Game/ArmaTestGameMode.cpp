// ArmaTestGameMode.cpp

#include "ArmaTestGameMode.h"
#include "ArmaCyclePawn.h"
#include "ArmaWallRegistry.h"
#include "AI/ArmaAICycle.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

AArmaTestGameMode::AArmaTestGameMode()
{
	// Set the default pawn to our test pawn
	DefaultPawnClass = AArmaCyclePawn::StaticClass();
	
	// Use standard player controller
	PlayerControllerClass = APlayerController::StaticClass();
}

void AArmaTestGameMode::BeginPlay()
{
	Super::BeginPlay();
	
	// Spawn arena rim walls (boundary)
	SpawnArenaRim();
	
	// Spawn AI players
	SpawnAIPlayers();
}

void AArmaTestGameMode::SpawnArenaRim()
{
	UArmaWallRegistry* WallRegistry = UArmaWallRegistry::Get(GetWorld());
	if (WallRegistry)
	{
		// Create arena boundary: 5000x5000 centered at origin
		WallRegistry->SpawnArenaRim(ArenaHalfSize, ArenaHalfSize, 150.0f);
		UE_LOG(LogTemp, Warning, TEXT("ArmaTestGameMode: Spawned arena rim walls (%.0fx%.0f)"), 
			ArenaHalfSize * 2, ArenaHalfSize * 2);
	}
}

void AArmaTestGameMode::SpawnAIPlayers()
{
	UWorld* World = GetWorld();
	if (!World) return;
	
	// AI spawn locations (offset from player start)
	TArray<FVector> AISpawnLocations = {
		FVector(200.0f, 500.0f, 92.0f),    // AI 1 - to the right
		FVector(200.0f, -500.0f, 92.0f),   // AI 2 - to the left
	};
	
	TArray<FLinearColor> AIColors = {
		FLinearColor(1.0f, 0.2f, 0.2f, 1.0f),  // Red
		FLinearColor(0.2f, 1.0f, 0.2f, 1.0f),  // Green
	};
	
	TArray<FVector> AIDirections = {
		FVector(1.0f, 0.0f, 0.0f),  // Facing +X
		FVector(1.0f, 0.0f, 0.0f),  // Facing +X
	};
	
	for (int32 i = 0; i < NumAIPlayers && i < AISpawnLocations.Num(); i++)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		
		AArmaAICycle* AIPlayer = World->SpawnActor<AArmaAICycle>(
			AArmaAICycle::StaticClass(),
			AISpawnLocations[i],
			FRotator::ZeroRotator,
			SpawnParams
		);
		
		if (AIPlayer)
		{
			AIPlayer->AIColor = AIColors[i % AIColors.Num()];
			AIPlayer->CycleColor = AIColors[i % AIColors.Num()];
			AIPlayer->MoveDirection = AIDirections[i % AIDirections.Num()];
			AIPlayer->SpawnLocation = AISpawnLocations[i];
			AIPlayer->SpawnDirection = AIDirections[i % AIDirections.Num()];
			AIPlayer->AIIQ = 80 + (i * 10);  // Varying IQ
			
			AIPlayers.Add(AIPlayer);
			
			UE_LOG(LogTemp, Warning, TEXT("Spawned AI Player %d at %s with IQ %d"), 
				i + 1, *AISpawnLocations[i].ToString(), AIPlayer->AIIQ);
		}
	}
}

