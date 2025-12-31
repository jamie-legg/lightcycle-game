// ArmaTestGameMode.h - Game mode with AI players

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ArmaTestGameMode.generated.h"

class AArmaAICycle;

/**
 * Game mode that spawns player and AI cycles
 */
UCLASS()
class ARMAGETRONUE5_API AArmaTestGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AArmaTestGameMode();
	
	virtual void BeginPlay() override;
	
	// Number of AI players to spawn
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	int32 NumAIPlayers = 1;
	
	// Spawned AI players
	UPROPERTY(BlueprintReadOnly, Category = "AI")
	TArray<AArmaAICycle*> AIPlayers;
	
	// Arena size (half-width from center)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena")
	float ArenaHalfSize = 5000.0f;
	
protected:
	void SpawnAIPlayers();
	void SpawnArenaRim();
};

