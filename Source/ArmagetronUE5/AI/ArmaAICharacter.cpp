// ArmaAICharacter.cpp - Implementation of AI character system

#include "ArmaAICharacter.h"

// Static storage for loaded characters
TArray<FArmaAICharacter> UArmaAICharacterLibrary::LoadedCharacters;

//////////////////////////////////////////////////////////////////////////
// UArmaAICharacterData Implementation
//////////////////////////////////////////////////////////////////////////

FArmaAICharacter UArmaAICharacterData::ToArmaCharacter() const
{
	FArmaAICharacter Char;
	Char.Name = Name;
	Char.Description = Description;
	Char.IQ = IQ;
	
	Char.Properties.SetNum(ArmaAIPropertyIndex::PropertyCount);
	Char.Properties[ArmaAIPropertyIndex::Reactivity] = Reactivity;
	Char.Properties[ArmaAIPropertyIndex::Aggression] = Aggression;
	Char.Properties[ArmaAIPropertyIndex::SurvivalInstinct] = SurvivalInstinct;
	Char.Properties[ArmaAIPropertyIndex::Pathfinding] = Pathfinding;
	Char.Properties[ArmaAIPropertyIndex::WallHugging] = WallHugging;
	Char.Properties[ArmaAIPropertyIndex::Prediction] = Prediction;
	Char.Properties[ArmaAIPropertyIndex::RubberUsage] = RubberUsage;
	Char.Properties[ArmaAIPropertyIndex::BrakeUsage] = BrakeUsage;
	Char.Properties[ArmaAIPropertyIndex::Trapping] = Trapping;
	Char.Properties[ArmaAIPropertyIndex::TeamPlay] = TeamPlay;
	Char.Properties[ArmaAIPropertyIndex::StateChange] = StateChange;
	Char.Properties[ArmaAIPropertyIndex::Randomness] = Randomness;
	Char.Properties[ArmaAIPropertyIndex::LookaheadRange] = LookaheadRange;

	return Char;
}

//////////////////////////////////////////////////////////////////////////
// UArmaAICharacterLibrary Implementation
//////////////////////////////////////////////////////////////////////////

TArray<FArmaAICharacter> UArmaAICharacterLibrary::GetAllCharacters()
{
	if (LoadedCharacters.Num() == 0)
	{
		// Return default characters if none loaded
		return GetDefaultCharacters();
	}
	return LoadedCharacters;
}

bool UArmaAICharacterLibrary::LoadCharactersFromFile(const FString& Filename)
{
	// Load from config file (similar to original aiplayers.cfg format)
	FString FileContent;
	if (!FFileHelper::LoadFileToString(FileContent, *Filename))
	{
		return false;
	}

	LoadedCharacters.Empty();

	TArray<FString> Lines;
	FileContent.ParseIntoArrayLines(Lines);

	FArmaAICharacter CurrentChar;
	bool bReadingChar = false;

	for (const FString& Line : Lines)
	{
		FString TrimmedLine = Line.TrimStartAndEnd();
		
		// Skip comments and empty lines
		if (TrimmedLine.IsEmpty() || TrimmedLine.StartsWith(TEXT("#")))
			continue;

		// Parse AI character definition
		if (TrimmedLine.StartsWith(TEXT("AI_CHARACTER")))
		{
			if (bReadingChar && !CurrentChar.Name.IsEmpty())
			{
				LoadedCharacters.Add(CurrentChar);
			}
			
			// Start new character
			CurrentChar = FArmaAICharacter();
			CurrentChar.Properties.SetNum(ArmaAIPropertyIndex::PropertyCount);
			bReadingChar = true;

			// Extract name
			int32 SpaceIdx;
			if (TrimmedLine.FindChar(' ', SpaceIdx))
			{
				CurrentChar.Name = TrimmedLine.Mid(SpaceIdx + 1).TrimQuotes();
			}
		}
		else if (bReadingChar)
		{
			// Parse property lines
			TArray<FString> Parts;
			TrimmedLine.ParseIntoArray(Parts, TEXT(" "), true);
			
			if (Parts.Num() >= 2)
			{
				FString Key = Parts[0].ToUpper();
				int32 Value = FCString::Atoi(*Parts[1]);

				if (Key == TEXT("IQ"))
					CurrentChar.IQ = Value;
				else if (Key == TEXT("REACTIVITY"))
					CurrentChar.Properties[ArmaAIPropertyIndex::Reactivity] = Value;
				else if (Key == TEXT("AGGRESSION"))
					CurrentChar.Properties[ArmaAIPropertyIndex::Aggression] = Value;
				else if (Key == TEXT("SURVIVAL"))
					CurrentChar.Properties[ArmaAIPropertyIndex::SurvivalInstinct] = Value;
				else if (Key == TEXT("PATHFINDING"))
					CurrentChar.Properties[ArmaAIPropertyIndex::Pathfinding] = Value;
				else if (Key == TEXT("WALLHUGGING"))
					CurrentChar.Properties[ArmaAIPropertyIndex::WallHugging] = Value;
				else if (Key == TEXT("PREDICTION"))
					CurrentChar.Properties[ArmaAIPropertyIndex::Prediction] = Value;
				else if (Key == TEXT("RUBBER"))
					CurrentChar.Properties[ArmaAIPropertyIndex::RubberUsage] = Value;
				else if (Key == TEXT("BRAKE"))
					CurrentChar.Properties[ArmaAIPropertyIndex::BrakeUsage] = Value;
				else if (Key == TEXT("TRAPPING"))
					CurrentChar.Properties[ArmaAIPropertyIndex::Trapping] = Value;
				else if (Key == TEXT("TEAMPLAY"))
					CurrentChar.Properties[ArmaAIPropertyIndex::TeamPlay] = Value;
				else if (Key == TEXT("STATECHANGE"))
					CurrentChar.Properties[ArmaAIPropertyIndex::StateChange] = Value;
				else if (Key == TEXT("RANDOMNESS"))
					CurrentChar.Properties[ArmaAIPropertyIndex::Randomness] = Value;
				else if (Key == TEXT("LOOKAHEAD"))
					CurrentChar.Properties[ArmaAIPropertyIndex::LookaheadRange] = Value;
			}
		}
	}

	// Add last character
	if (bReadingChar && !CurrentChar.Name.IsEmpty())
	{
		LoadedCharacters.Add(CurrentChar);
	}

	return LoadedCharacters.Num() > 0;
}

FArmaAICharacter UArmaAICharacterLibrary::GetCharacterByName(const FString& Name)
{
	TArray<FArmaAICharacter> Characters = GetAllCharacters();
	
	for (const FArmaAICharacter& Char : Characters)
	{
		if (Char.Name.Equals(Name, ESearchCase::IgnoreCase))
		{
			return Char;
		}
	}

	// Return default intermediate if not found
	return ArmaAIPresets::CreateIntermediate();
}

FArmaAICharacter UArmaAICharacterLibrary::GetCharacterByIQ(float TargetIQ)
{
	TArray<FArmaAICharacter> Characters = GetAllCharacters();
	
	if (Characters.Num() == 0)
	{
		return CreateRandomCharacter(TargetIQ);
	}

	// Find closest IQ match
	float BestDiff = FLT_MAX;
	int32 BestIndex = 0;

	for (int32 i = 0; i < Characters.Num(); i++)
	{
		float Diff = FMath::Abs(Characters[i].IQ - TargetIQ);
		if (Diff < BestDiff)
		{
			BestDiff = Diff;
			BestIndex = i;
		}
	}

	return Characters[BestIndex];
}

FArmaAICharacter UArmaAICharacterLibrary::CreateRandomCharacter(float IQ)
{
	FArmaAICharacter Char;
	Char.Name = FString::Printf(TEXT("Bot_%d"), FMath::RandRange(1000, 9999));
	Char.Description = TEXT("Randomly generated AI");
	Char.IQ = IQ;
	
	Char.Properties.SetNum(ArmaAIPropertyIndex::PropertyCount);

	// Scale properties based on IQ (50 = weak, 100 = average, 150 = strong)
	float IQFactor = IQ / 100.0f;
	
	for (int32 i = 0; i < ArmaAIPropertyIndex::PropertyCount; i++)
	{
		// Base value around 5, scaled by IQ, with some randomness
		int32 BaseValue = FMath::RoundToInt(5.0f * IQFactor);
		int32 Randomness = FMath::RandRange(-2, 2);
		Char.Properties[i] = FMath::Clamp(BaseValue + Randomness, 0, 10);
	}

	// Ensure survival instinct is decent
	Char.Properties[ArmaAIPropertyIndex::SurvivalInstinct] = FMath::Max(Char.Properties[ArmaAIPropertyIndex::SurvivalInstinct], 3);

	return Char;
}

TArray<FArmaAICharacter> UArmaAICharacterLibrary::GetDefaultCharacters()
{
	TArray<FArmaAICharacter> Defaults;
	
	Defaults.Add(ArmaAIPresets::CreateNovice());
	Defaults.Add(ArmaAIPresets::CreateIntermediate());
	Defaults.Add(ArmaAIPresets::CreateExpert());
	Defaults.Add(ArmaAIPresets::CreateMaster());
	Defaults.Add(ArmaAIPresets::CreateHunter());
	Defaults.Add(ArmaAIPresets::CreateDefender());

	return Defaults;
}

