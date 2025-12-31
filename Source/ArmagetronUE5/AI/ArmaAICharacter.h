// ArmaAICharacter.h - Port of gAICharacter from src/tron/gAICharacter.h
// AI personality and difficulty definitions

#pragma once

#include "CoreMinimal.h"
#include "Core/ArmaTypes.h"
#include "ArmaAICharacter.generated.h"

/**
 * AI Property indices - matching original AI_PROPERTIES
 */
namespace ArmaAIPropertyIndex
{
	constexpr int32 Reactivity = 0;       // How fast does the AI react?
	constexpr int32 Aggression = 1;       // How aggressive is the AI?
	constexpr int32 SurvivalInstinct = 2; // How much does AI prioritize survival?
	constexpr int32 Pathfinding = 3;      // How good is pathfinding?
	constexpr int32 WallHugging = 4;      // How close does AI drive to walls?
	constexpr int32 Prediction = 5;       // How well does AI predict enemy movement?
	constexpr int32 RubberUsage = 6;      // How well does AI use rubber?
	constexpr int32 BrakeUsage = 7;       // How well does AI use brakes?
	constexpr int32 Trapping = 8;         // How good at trapping enemies?
	constexpr int32 TeamPlay = 9;         // How well does AI work with team?
	constexpr int32 StateChange = 10;     // How often does AI change states?
	constexpr int32 Randomness = 11;      // How random is AI behavior?
	constexpr int32 LookaheadRange = 12;  // How far ahead does AI look?
	constexpr int32 PropertyCount = 13;
}

/**
 * UArmaAICharacterData - Data asset for AI personalities
 */
UCLASS(BlueprintType)
class ARMAGETRONUE5_API UArmaAICharacterData : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Character")
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Character", meta = (MultiLine = true))
	FString Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Character")
	float IQ;

	// AI Properties (0-10 scale for each)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Properties", meta = (ClampMin = "0", ClampMax = "10"))
	int32 Reactivity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Properties", meta = (ClampMin = "0", ClampMax = "10"))
	int32 Aggression;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Properties", meta = (ClampMin = "0", ClampMax = "10"))
	int32 SurvivalInstinct;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Properties", meta = (ClampMin = "0", ClampMax = "10"))
	int32 Pathfinding;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Properties", meta = (ClampMin = "0", ClampMax = "10"))
	int32 WallHugging;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Properties", meta = (ClampMin = "0", ClampMax = "10"))
	int32 Prediction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Properties", meta = (ClampMin = "0", ClampMax = "10"))
	int32 RubberUsage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Properties", meta = (ClampMin = "0", ClampMax = "10"))
	int32 BrakeUsage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Properties", meta = (ClampMin = "0", ClampMax = "10"))
	int32 Trapping;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Properties", meta = (ClampMin = "0", ClampMax = "10"))
	int32 TeamPlay;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Properties", meta = (ClampMin = "0", ClampMax = "10"))
	int32 StateChange;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Properties", meta = (ClampMin = "0", ClampMax = "10"))
	int32 Randomness;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Properties", meta = (ClampMin = "0", ClampMax = "10"))
	int32 LookaheadRange;

	// Convert to FArmaAICharacter
	FArmaAICharacter ToArmaCharacter() const;
};

/**
 * UArmaAICharacterLibrary - Function library for AI characters
 */
UCLASS()
class ARMAGETRONUE5_API UArmaAICharacterLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// Get all available AI characters
	UFUNCTION(BlueprintCallable, Category = "AI Character")
	static TArray<FArmaAICharacter> GetAllCharacters();

	// Load characters from config file (like original aiplayers.cfg)
	UFUNCTION(BlueprintCallable, Category = "AI Character")
	static bool LoadCharactersFromFile(const FString& Filename);

	// Get character by name
	UFUNCTION(BlueprintCallable, Category = "AI Character")
	static FArmaAICharacter GetCharacterByName(const FString& Name);

	// Get character closest to target IQ
	UFUNCTION(BlueprintCallable, Category = "AI Character")
	static FArmaAICharacter GetCharacterByIQ(float TargetIQ);

	// Create a random character with given IQ
	UFUNCTION(BlueprintCallable, Category = "AI Character")
	static FArmaAICharacter CreateRandomCharacter(float IQ);

	// Get default characters (built-in)
	UFUNCTION(BlueprintCallable, Category = "AI Character")
	static TArray<FArmaAICharacter> GetDefaultCharacters();

private:
	// Storage for loaded characters
	static TArray<FArmaAICharacter> LoadedCharacters;
};

/**
 * Predefined AI character templates
 */
namespace ArmaAIPresets
{
	// Very easy opponent
	inline FArmaAICharacter CreateNovice()
	{
		FArmaAICharacter Char;
		Char.Name = TEXT("Novice");
		Char.Description = TEXT("A beginner AI, easy to beat.");
		Char.IQ = 30.0f;
		Char.Properties.SetNum(ArmaAIPropertyIndex::PropertyCount);
		Char.Properties[ArmaAIPropertyIndex::Reactivity] = 3;
		Char.Properties[ArmaAIPropertyIndex::Aggression] = 2;
		Char.Properties[ArmaAIPropertyIndex::SurvivalInstinct] = 4;
		Char.Properties[ArmaAIPropertyIndex::Pathfinding] = 2;
		Char.Properties[ArmaAIPropertyIndex::WallHugging] = 2;
		Char.Properties[ArmaAIPropertyIndex::Prediction] = 1;
		Char.Properties[ArmaAIPropertyIndex::RubberUsage] = 1;
		Char.Properties[ArmaAIPropertyIndex::BrakeUsage] = 1;
		Char.Properties[ArmaAIPropertyIndex::Trapping] = 1;
		Char.Properties[ArmaAIPropertyIndex::TeamPlay] = 2;
		Char.Properties[ArmaAIPropertyIndex::StateChange] = 5;
		Char.Properties[ArmaAIPropertyIndex::Randomness] = 7;
		Char.Properties[ArmaAIPropertyIndex::LookaheadRange] = 3;
		return Char;
	}

	// Medium opponent
	inline FArmaAICharacter CreateIntermediate()
	{
		FArmaAICharacter Char;
		Char.Name = TEXT("Intermediate");
		Char.Description = TEXT("A balanced AI opponent.");
		Char.IQ = 70.0f;
		Char.Properties.SetNum(ArmaAIPropertyIndex::PropertyCount);
		Char.Properties[ArmaAIPropertyIndex::Reactivity] = 5;
		Char.Properties[ArmaAIPropertyIndex::Aggression] = 5;
		Char.Properties[ArmaAIPropertyIndex::SurvivalInstinct] = 6;
		Char.Properties[ArmaAIPropertyIndex::Pathfinding] = 5;
		Char.Properties[ArmaAIPropertyIndex::WallHugging] = 5;
		Char.Properties[ArmaAIPropertyIndex::Prediction] = 4;
		Char.Properties[ArmaAIPropertyIndex::RubberUsage] = 4;
		Char.Properties[ArmaAIPropertyIndex::BrakeUsage] = 4;
		Char.Properties[ArmaAIPropertyIndex::Trapping] = 4;
		Char.Properties[ArmaAIPropertyIndex::TeamPlay] = 5;
		Char.Properties[ArmaAIPropertyIndex::StateChange] = 5;
		Char.Properties[ArmaAIPropertyIndex::Randomness] = 4;
		Char.Properties[ArmaAIPropertyIndex::LookaheadRange] = 5;
		return Char;
	}

	// Hard opponent
	inline FArmaAICharacter CreateExpert()
	{
		FArmaAICharacter Char;
		Char.Name = TEXT("Expert");
		Char.Description = TEXT("A skilled AI opponent.");
		Char.IQ = 100.0f;
		Char.Properties.SetNum(ArmaAIPropertyIndex::PropertyCount);
		Char.Properties[ArmaAIPropertyIndex::Reactivity] = 7;
		Char.Properties[ArmaAIPropertyIndex::Aggression] = 6;
		Char.Properties[ArmaAIPropertyIndex::SurvivalInstinct] = 8;
		Char.Properties[ArmaAIPropertyIndex::Pathfinding] = 7;
		Char.Properties[ArmaAIPropertyIndex::WallHugging] = 7;
		Char.Properties[ArmaAIPropertyIndex::Prediction] = 6;
		Char.Properties[ArmaAIPropertyIndex::RubberUsage] = 6;
		Char.Properties[ArmaAIPropertyIndex::BrakeUsage] = 6;
		Char.Properties[ArmaAIPropertyIndex::Trapping] = 6;
		Char.Properties[ArmaAIPropertyIndex::TeamPlay] = 6;
		Char.Properties[ArmaAIPropertyIndex::StateChange] = 4;
		Char.Properties[ArmaAIPropertyIndex::Randomness] = 3;
		Char.Properties[ArmaAIPropertyIndex::LookaheadRange] = 7;
		return Char;
	}

	// Very hard opponent
	inline FArmaAICharacter CreateMaster()
	{
		FArmaAICharacter Char;
		Char.Name = TEXT("Master");
		Char.Description = TEXT("A master AI opponent. Very challenging.");
		Char.IQ = 150.0f;
		Char.Properties.SetNum(ArmaAIPropertyIndex::PropertyCount);
		Char.Properties[ArmaAIPropertyIndex::Reactivity] = 9;
		Char.Properties[ArmaAIPropertyIndex::Aggression] = 7;
		Char.Properties[ArmaAIPropertyIndex::SurvivalInstinct] = 9;
		Char.Properties[ArmaAIPropertyIndex::Pathfinding] = 8;
		Char.Properties[ArmaAIPropertyIndex::WallHugging] = 8;
		Char.Properties[ArmaAIPropertyIndex::Prediction] = 8;
		Char.Properties[ArmaAIPropertyIndex::RubberUsage] = 8;
		Char.Properties[ArmaAIPropertyIndex::BrakeUsage] = 8;
		Char.Properties[ArmaAIPropertyIndex::Trapping] = 8;
		Char.Properties[ArmaAIPropertyIndex::TeamPlay] = 7;
		Char.Properties[ArmaAIPropertyIndex::StateChange] = 3;
		Char.Properties[ArmaAIPropertyIndex::Randomness] = 2;
		Char.Properties[ArmaAIPropertyIndex::LookaheadRange] = 9;
		return Char;
	}

	// Aggressive hunter
	inline FArmaAICharacter CreateHunter()
	{
		FArmaAICharacter Char;
		Char.Name = TEXT("Hunter");
		Char.Description = TEXT("An aggressive hunter. Will chase you down.");
		Char.IQ = 90.0f;
		Char.Properties.SetNum(ArmaAIPropertyIndex::PropertyCount);
		Char.Properties[ArmaAIPropertyIndex::Reactivity] = 8;
		Char.Properties[ArmaAIPropertyIndex::Aggression] = 9;
		Char.Properties[ArmaAIPropertyIndex::SurvivalInstinct] = 5;
		Char.Properties[ArmaAIPropertyIndex::Pathfinding] = 7;
		Char.Properties[ArmaAIPropertyIndex::WallHugging] = 4;
		Char.Properties[ArmaAIPropertyIndex::Prediction] = 7;
		Char.Properties[ArmaAIPropertyIndex::RubberUsage] = 5;
		Char.Properties[ArmaAIPropertyIndex::BrakeUsage] = 3;
		Char.Properties[ArmaAIPropertyIndex::Trapping] = 8;
		Char.Properties[ArmaAIPropertyIndex::TeamPlay] = 4;
		Char.Properties[ArmaAIPropertyIndex::StateChange] = 6;
		Char.Properties[ArmaAIPropertyIndex::Randomness] = 3;
		Char.Properties[ArmaAIPropertyIndex::LookaheadRange] = 8;
		return Char;
	}

	// Defensive player
	inline FArmaAICharacter CreateDefender()
	{
		FArmaAICharacter Char;
		Char.Name = TEXT("Defender");
		Char.Description = TEXT("A cautious defender. Hard to trap.");
		Char.IQ = 85.0f;
		Char.Properties.SetNum(ArmaAIPropertyIndex::PropertyCount);
		Char.Properties[ArmaAIPropertyIndex::Reactivity] = 7;
		Char.Properties[ArmaAIPropertyIndex::Aggression] = 3;
		Char.Properties[ArmaAIPropertyIndex::SurvivalInstinct] = 9;
		Char.Properties[ArmaAIPropertyIndex::Pathfinding] = 6;
		Char.Properties[ArmaAIPropertyIndex::WallHugging] = 8;
		Char.Properties[ArmaAIPropertyIndex::Prediction] = 5;
		Char.Properties[ArmaAIPropertyIndex::RubberUsage] = 8;
		Char.Properties[ArmaAIPropertyIndex::BrakeUsage] = 7;
		Char.Properties[ArmaAIPropertyIndex::Trapping] = 3;
		Char.Properties[ArmaAIPropertyIndex::TeamPlay] = 6;
		Char.Properties[ArmaAIPropertyIndex::StateChange] = 4;
		Char.Properties[ArmaAIPropertyIndex::Randomness] = 4;
		Char.Properties[ArmaAIPropertyIndex::LookaheadRange] = 7;
		return Char;
	}
}

