// ArmaTypes.h - Core type definitions ported from original Armagetron
// Ports eCoord, REAL, gRealColor and other foundational types

#pragma once

#include "CoreMinimal.h"
#include "ArmaTypes.generated.h"

// Port of REAL typedef from original codebase
typedef float REAL;

/**
 * FArmaCoord - Port of eCoord from src/engine/eCoord.h
 * 2D coordinate/vector used throughout the game for positions and directions
 */
USTRUCT(BlueprintType)
struct ARMAGETRONUE5_API FArmaCoord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Armagetron")
	float X;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Armagetron")
	float Y;

	FArmaCoord() : X(0.0f), Y(0.0f) {}
	FArmaCoord(float InX, float InY) : X(InX), Y(InY) {}
	FArmaCoord(const FVector2D& Vec) : X(Vec.X), Y(Vec.Y) {}

	// Convert to UE5 types
	FVector2D ToVector2D() const { return FVector2D(X, Y); }
	FVector ToVector3D(float Z = 0.0f) const { return FVector(X, Y, Z); }

	// Magnitude squared (NormSquared in original)
	float NormSquared() const { return X * X + Y * Y; }
	
	// Magnitude (Norm in original)
	float Norm() const { return FMath::Sqrt(NormSquared()); }

	// Normalize
	FArmaCoord Normalized() const
	{
		float N = Norm();
		if (N > KINDA_SMALL_NUMBER)
			return FArmaCoord(X / N, Y / N);
		return FArmaCoord(0, 0);
	}

	// Dot product
	float Dot(const FArmaCoord& Other) const { return X * Other.X + Y * Other.Y; }

	// Cross product (returns Z component of 3D cross)
	float Cross(const FArmaCoord& Other) const { return X * Other.Y - Y * Other.X; }

	// Turn (perpendicular vector)
	FArmaCoord Turn(int Dir) const
	{
		// Dir > 0: turn left, Dir < 0: turn right
		if (Dir > 0)
			return FArmaCoord(-Y, X);  // 90 degrees counter-clockwise
		else if (Dir < 0)
			return FArmaCoord(Y, -X);  // 90 degrees clockwise
		return *this;
	}

	// Conj (complex conjugate - flip Y)
	FArmaCoord Conj() const { return FArmaCoord(X, -Y); }

	// Operators
	FArmaCoord operator+(const FArmaCoord& Other) const { return FArmaCoord(X + Other.X, Y + Other.Y); }
	FArmaCoord operator-(const FArmaCoord& Other) const { return FArmaCoord(X - Other.X, Y - Other.Y); }
	FArmaCoord operator*(float Scale) const { return FArmaCoord(X * Scale, Y * Scale); }
	FArmaCoord operator/(float Scale) const { return FArmaCoord(X / Scale, Y / Scale); }
	FArmaCoord operator-() const { return FArmaCoord(-X, -Y); }
	
	FArmaCoord& operator+=(const FArmaCoord& Other) { X += Other.X; Y += Other.Y; return *this; }
	FArmaCoord& operator-=(const FArmaCoord& Other) { X -= Other.X; Y -= Other.Y; return *this; }
	FArmaCoord& operator*=(float Scale) { X *= Scale; Y *= Scale; return *this; }
	FArmaCoord& operator/=(float Scale) { X /= Scale; Y /= Scale; return *this; }

	bool operator==(const FArmaCoord& Other) const { return FMath::IsNearlyEqual(X, Other.X) && FMath::IsNearlyEqual(Y, Other.Y); }
	bool operator!=(const FArmaCoord& Other) const { return !(*this == Other); }

	// Complex multiplication (used for rotation in original)
	FArmaCoord operator*(const FArmaCoord& Other) const
	{
		return FArmaCoord(X * Other.X - Y * Other.Y, X * Other.Y + Y * Other.X);
	}

	static const FArmaCoord Zero;
	static const FArmaCoord UnitX;
	static const FArmaCoord UnitY;
};

/**
 * FArmaColor - Port of gRealColor from src/tron/gCycleMovement.h
 * RGB color used for cycles, trails, etc.
 */
USTRUCT(BlueprintType)
struct ARMAGETRONUE5_API FArmaColor
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Armagetron", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float R;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Armagetron", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float G;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Armagetron", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float B;

	FArmaColor() : R(1.0f), G(1.0f), B(1.0f) {}
	FArmaColor(float InR, float InG, float InB) : R(InR), G(InG), B(InB) {}
	FArmaColor(const FLinearColor& Color) : R(Color.R), G(Color.G), B(Color.B) {}
	FArmaColor(const FColor& Color) : R(Color.R / 255.0f), G(Color.G / 255.0f), B(Color.B / 255.0f) {}

	FLinearColor ToLinearColor(float Alpha = 1.0f) const { return FLinearColor(R, G, B, Alpha); }
	FColor ToFColor(uint8 Alpha = 255) const { return FColor(R * 255, G * 255, B * 255, Alpha); }

	FString ToString() const { return FString::Printf(TEXT("(R=%.3f,G=%.3f,B=%.3f)"), R, G, B); }

	// Predefined cycle colors (from original game)
	static const FArmaColor Red;
	static const FArmaColor Blue;
	static const FArmaColor Green;
	static const FArmaColor Yellow;
	static const FArmaColor Orange;
	static const FArmaColor Purple;
	static const FArmaColor Cyan;
	static const FArmaColor White;
};

/**
 * EArmaAIState - Port of gAI_STATE from src/tron/gAIBase.h
 * AI behavior states
 */
UENUM(BlueprintType)
enum class EArmaAIState : uint8
{
	Survive		UMETA(DisplayName = "Survive"),		// Just try to stay alive
	Trace		UMETA(DisplayName = "Trace"),		// Trace a wall
	Path		UMETA(DisplayName = "Path"),		// Follow a path to target
	CloseCombat	UMETA(DisplayName = "Close Combat"),// Try to frag nearby opponent
	Route		UMETA(DisplayName = "Route")		// Follow a route (set of coords)
};

/**
 * EArmaGameType - Port of gGameType from src/tron/gGame.h
 */
UENUM(BlueprintType)
enum class EArmaGameType : uint8
{
	Freestyle	UMETA(DisplayName = "Freestyle"),
	Duel		UMETA(DisplayName = "Duel"),
	HumanVsAI	UMETA(DisplayName = "Human vs AI")
};

/**
 * EArmaFinishType - Port of gFinishType from src/tron/gGame.h
 */
UENUM(BlueprintType)
enum class EArmaFinishType : uint8
{
	Express		UMETA(DisplayName = "Express"),
	Immediately	UMETA(DisplayName = "Immediately"),
	Speedup		UMETA(DisplayName = "Speedup"),
	Normal		UMETA(DisplayName = "Normal")
};

/**
 * FArmaGameSettings - Port of gGameSettings from src/tron/gGame.h
 * Game configuration settings
 */
USTRUCT(BlueprintType)
struct ARMAGETRONUE5_API FArmaGameSettings
{
	GENERATED_BODY()

	// Scoring
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoring")
	int32 ScoreWin = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoring")
	int32 ScoreDiffWin = 1;

	// Limits
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Limits")
	int32 LimitTime = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Limits")
	int32 LimitRounds = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Limits")
	int32 LimitScore = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Limits")
	int32 LimitScoreMinLead = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Limits")
	int32 MaxBlowout = 100;

	// AI Settings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	int32 NumAIs = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	int32 MinPlayers = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	int32 AI_IQ = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	bool bAutoNum = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	bool bAutoIQ = false;

	// Speed and Size
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay")
	float SpeedFactor = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay")
	float SizeFactor = 0.0f;

	// Win Zone
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay")
	float WinZoneMinRoundTime = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay")
	float WinZoneMinLastDeath = 30.0f;

	// Game type
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay")
	EArmaGameType GameType = EArmaGameType::HumanVsAI;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay")
	EArmaFinishType FinishType = EArmaFinishType::Normal;

	// Team Settings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Teams")
	int32 MinTeams = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Teams")
	int32 MaxTeams = 16;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Teams")
	int32 MinPlayersPerTeam = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Teams")
	int32 MaxPlayersPerTeam = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Teams")
	int32 MaxTeamImbalance = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Teams")
	bool bBalanceTeamsWithAIs = true;

	// Wall Settings (from gCycle statics)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Walls")
	float WallsStayUpDelay = -1.0f;  // Negative = stay forever

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Walls")
	float WallsLength = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Walls")
	float ExplosionRadius = 4.0f;
};

/**
 * FArmaWallCoord - Port of gPlayerWallCoord from src/tron/gWall.h
 * Coordinate entry for wall segments
 */
USTRUCT(BlueprintType)
struct ARMAGETRONUE5_API FArmaWallCoord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall")
	float Pos = 0.0f;  // Start position relative to cycle start

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall")
	float Time = 0.0f;  // Time this point was created

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall")
	bool bIsDangerous = true;  // True if segment AFTER this is a wall

	// Reference to holer (explosion that created hole) - handled separately
};

/**
 * FArmaAICharacter - Port of gAICharacter from src/tron/gAICharacter.h
 * AI personality/difficulty definition
 */
USTRUCT(BlueprintType)
struct ARMAGETRONUE5_API FArmaAICharacter
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Character")
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Character")
	FString Description;

	// 13 AI properties from original
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Character")
	TArray<int32> Properties;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Character")
	float IQ = 100.0f;  // Estimated battle strength

	FArmaAICharacter()
	{
		Properties.SetNum(13);
		for (int32& Prop : Properties)
		{
			Prop = 0;
		}
	}
};

// Cycle Physics Constants - ported from various sources
namespace ArmaPhysics
{
	// From gCycleMovement
	constexpr float DefaultSpeed = 20.0f;
	constexpr float DefaultSpeedMultiplier = 1.0f;
	constexpr float DefaultRubber = 0.0f;
	constexpr float DefaultRubberSpeed = 60.0f;
	constexpr float DefaultBrakingReservoir = 1.0f;
	constexpr float DefaultTurnDelay = 0.2f;
	constexpr float DefaultTurnDelayDb = 0.1f;  // Same direction turn delay

	// From gCycle
	constexpr float DefaultWallsStayUpDelay = -1.0f;
	constexpr float DefaultWallsLength = 300.0f;
	constexpr float DefaultExplosionRadius = 4.0f;

	// Arena defaults
	constexpr float DefaultArenaSize = 500.0f;
	constexpr float DefaultArenaSizeMultiplier = 1.0f;
}

