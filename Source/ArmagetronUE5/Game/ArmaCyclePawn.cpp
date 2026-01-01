// ArmaCyclePawn.cpp - Armagetron/Snake style with glowing trails

#include "ArmaCyclePawn.h"
#include "ArmaWallRegistry.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Engine.h"
#include "DrawDebugHelpers.h"
#include "Components/PointLightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"

AArmaCyclePawn::AArmaCyclePawn()
{
	PrimaryActorTick.bCanEverTick = true;

	// Create root
	RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
	RootComponent = RootSceneComponent;

	// Create collision box - enable collision for wall detection
	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	CollisionBox->SetupAttachment(RootComponent);
	CollisionBox->SetBoxExtent(FVector(75.0f, 25.0f, 75.0f));
	CollisionBox->SetRelativeLocation(FVector(0, 0, 75));
	CollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionBox->SetCollisionObjectType(ECC_Pawn);
	CollisionBox->SetCollisionResponseToAllChannels(ECR_Overlap);
	CollisionBox->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	CollisionBox->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	CollisionBox->SetGenerateOverlapEvents(true);
	CollisionBox->SetNotifyRigidBodyCollision(true);

	// Create cycle mesh - sleek rounded rectangular prism using cylinder
	CycleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CycleMesh"));
	CycleMesh->SetupAttachment(RootComponent);
	
	// Use cylinder for rounded edges effect (rotated to point forward)
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		CycleMesh->SetStaticMesh(CylinderMesh.Object);
	}
	// Cylinder default: 100 height along Z, 100 diameter in X-Y plane
	// We want the cylinder pointing FORWARD (along pawn's X axis) so it faces movement direction
	// Pitch 90 degrees tilts the Z-axis forward to become the X-axis
	CycleMesh->SetRelativeRotation(FRotator(90, 0, 0));  // Pitch 90 - cylinder now points forward (+X)
	// Scale (applied to local axes):
	// LocalX,Y = diameter plane (thin sides)
	// LocalZ = height (becomes forward length after rotation)
	// We want: long forward (1.5), thin sides (0.3), thin height (0.3)
	CycleMesh->SetRelativeScale3D(FVector(0.3f, 0.3f, 1.5f));  // 30 diameter, 150 long forward
	// Position: lift up so bottom touches ground (world height = 0.3*100/2 = 15)
	CycleMesh->SetRelativeLocation(FVector(0, 0, 15));
	CycleMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Create camera arm
	CameraArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraArm"));
	CameraArm->SetupAttachment(RootComponent);
	CameraArm->TargetArmLength = 800.0f;
	CameraArm->SetRelativeRotation(FRotator(-35.0f, 0.0f, 0.0f));
	CameraArm->bDoCollisionTest = false;
	CameraArm->bEnableCameraLag = true;
	CameraArm->CameraLagSpeed = 5.0f; // Smooth position following
	CameraArm->bEnableCameraRotationLag = false; // We handle rotation smoothing ourselves
	CameraArm->bUsePawnControlRotation = false; // Don't use controller rotation
	CameraArm->bInheritPitch = false;
	CameraArm->bInheritYaw = true; // Follow the pawn's yaw (which we smooth in Tick)
	CameraArm->bInheritRoll = false;

	// Create camera
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraArm);
}

void AArmaCyclePawn::BeginPlay()
{
	Super::BeginPlay();

	GameStartTime = GetWorld()->GetTimeSeconds();
	
	// Initialize spawn values
	SpawnLocation = GetActorLocation();
	SpawnDirection = MoveDirection;
	SpawnTime = GameStartTime;
	CurrentRubber = MaxRubber;
	MoveSpeed = BaseSpeed;
	
	// Allow immediate first turn after spawn (set LastTurnTime in the past)
	LastTurnTime = GameStartTime - TurnDelay;

	UE_LOG(LogTemp, Warning, TEXT("BeginPlay: CycleMesh=%s, Location=%s"), 
		CycleMesh ? TEXT("Valid") : TEXT("NULL"),
		*GetActorLocation().ToString());

	// Apply chrome reflective material to the bike
	ApplyChromeMaterial();
	
	// Spawn glow light on cycle
	SpawnCycleGlow();

	// Set initial rotation based on MoveDirection
	TargetPawnYaw = MoveDirection.Rotation().Yaw;
	CurrentPawnYaw = TargetPawnYaw;
	SetActorRotation(FRotator(0, CurrentPawnYaw, 0));

	// Spawn ambient lighting for empty maps
	SpawnAmbientLighting();

	// Bind collision hit event for wall detection
	CollisionBox->OnComponentHit.AddDynamic(this, &AArmaCyclePawn::OnWallHit);
	CollisionBox->OnComponentBeginOverlap.AddDynamic(this, &AArmaCyclePawn::OnWallOverlap);
	
	UE_LOG(LogTemp, Warning, TEXT("=== COLLISION SETUP ==="));
	UE_LOG(LogTemp, Warning, TEXT("CollisionBox Enabled: %d"), (int32)CollisionBox->GetCollisionEnabled());
	UE_LOG(LogTemp, Warning, TEXT("CollisionBox ObjectType: %d"), (int32)CollisionBox->GetCollisionObjectType());

	// Spawn the glossy black grid floor
	SpawnFloorGrid();

	// Start first wall segment
	StartNewWallSegment();

	// Show controls
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Cyan, TEXT("=== ARMAGETRON UE5 ==="));
		GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::White, TEXT("A/D: Turn | Space: Brake/Respawn | Mouse: Look | Scroll: Zoom"));
		GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Yellow, TEXT("Watch your RUBBER meter! Grinding walls depletes it."));
	}
}

void AArmaCyclePawn::CreateGlowMaterial()
{
	// Use an UNLIT material that doesn't depend on scene lighting
	// Try DefaultWhite first which should always be visible
	UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr, 
		TEXT("/Engine/EngineDebugMaterials/VertexColorViewMode_ColorOnly.VertexColorViewMode_ColorOnly"));
	
	if (!BaseMat)
	{
		BaseMat = LoadObject<UMaterialInterface>(nullptr, 
			TEXT("/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"));
	}
	
	if (BaseMat)
	{
		GlowMaterial = UMaterialInstanceDynamic::Create(BaseMat, this);
	}
	
	// If no material worked, the mesh will use default white which should be visible
	UE_LOG(LogTemp, Warning, TEXT("CreateGlowMaterial: BaseMat=%s"), BaseMat ? TEXT("Found") : TEXT("NULL"));
}

void AArmaCyclePawn::SpawnCycleGlow()
{
	// Create point light for real-time glow effect
	CycleGlowLight = NewObject<UPointLightComponent>(this);
	if (CycleGlowLight)
	{
		CycleGlowLight->SetupAttachment(RootComponent);
		CycleGlowLight->SetRelativeLocation(FVector(0, 0, 75));
		CycleGlowLight->SetIntensity(100000.0f);
		CycleGlowLight->SetAttenuationRadius(800.0f);
		CycleGlowLight->SetLightColor(FColor(
			FMath::Clamp(int32(CycleColor.R * 255), 0, 255),
			FMath::Clamp(int32(CycleColor.G * 255), 0, 255),
			FMath::Clamp(int32(CycleColor.B * 255), 0, 255)
		));
		CycleGlowLight->SetMobility(EComponentMobility::Movable);
		CycleGlowLight->RegisterComponent();
	}
}

void AArmaCyclePawn::ApplyChromeMaterial()
{
	if (!CycleMesh) return;
	
	// Create a highly reflective chrome material
	UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	
	if (BaseMat)
	{
		UMaterialInstanceDynamic* ChromeMat = UMaterialInstanceDynamic::Create(BaseMat, this);
		if (ChromeMat)
		{
			// Set metallic chrome with colored tint from CycleColor
			// Dark base with colored emission for Tron look
			FLinearColor DarkBase = CycleColor * 0.1f;  // Very dark version of cycle color
			DarkBase.A = 1.0f;
			
			ChromeMat->SetVectorParameterValue(TEXT("Color"), DarkBase);
			
			// High metallic, low roughness for mirror-like reflections
			// Note: BasicShapeMaterial may not have these params, but we try anyway
			ChromeMat->SetScalarParameterValue(TEXT("Metallic"), 1.0f);
			ChromeMat->SetScalarParameterValue(TEXT("Roughness"), 0.05f);
			
			CycleMesh->SetMaterial(0, ChromeMat);
			
			UE_LOG(LogTemp, Warning, TEXT("ApplyChromeMaterial: Applied chrome material with color tint"));
		}
	}
}

void AArmaCyclePawn::SpawnFloorGrid()
{
	UWorld* World = GetWorld();
	if (!World) return;

	FVector PlayerPos = GetActorLocation();
	float FloorZ = -10.0f; // Floor below cycle level to prevent clipping
	float GridSize = 20000.0f; // Very large floor

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// Spawn main floor - dark but visible
	AStaticMeshActor* FloorActor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), 
		FVector(0, 0, FloorZ), FRotator::ZeroRotator, SpawnParams);
	
	if (FloorActor)
	{
		UStaticMeshComponent* FloorMesh = FloorActor->GetStaticMeshComponent();
		FloorMesh->SetMobility(EComponentMobility::Movable);
		
		UStaticMesh* PlaneMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
		if (PlaneMesh)
		{
			FloorMesh->SetStaticMesh(PlaneMesh);
		}
		
		FloorActor->SetActorScale3D(FVector(GridSize / 100.0f, GridSize / 100.0f, 1));
		FloorMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		FloorMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
		
		// Create dark floor material - slightly blue tint for Tron look
		UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		if (BaseMat)
		{
			UMaterialInstanceDynamic* FloorMat = UMaterialInstanceDynamic::Create(BaseMat, FloorActor);
			// Dark blue-black for Tron aesthetic
			FloorMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.01f, 0.015f, 0.03f, 1.0f));
			FloorMesh->SetMaterial(0, FloorMat);
		}
		
		UE_LOG(LogTemp, Warning, TEXT("SpawnFloorGrid: Created floor at Z=%.0f, Size=%.0f"), FloorZ, GridSize);
	}

	// Spawn grid lines - brighter and more visible
	float LineSpacing = 500.0f;
	float LineThickness = 8.0f;  // Thicker lines
	float LineHeight = -8.0f;  // Just above the floor
	int32 NumLines = 20;  // Fewer but more visible lines
	
	UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	
	for (int32 i = -NumLines; i <= NumLines; i++)
	{
		float Offset = i * LineSpacing;
		
		// Horizontal line (along X)
		AStaticMeshActor* HLine = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(),
			FVector(0, Offset, LineHeight), FRotator::ZeroRotator, SpawnParams);
		if (HLine && CubeMesh)
		{
			UStaticMeshComponent* LineMesh = HLine->GetStaticMeshComponent();
			LineMesh->SetMobility(EComponentMobility::Movable);
			LineMesh->SetStaticMesh(CubeMesh);
			HLine->SetActorScale3D(FVector(GridSize / 100.0f, LineThickness / 100.0f, 0.02f));
			
			if (BaseMat)
			{
				UMaterialInstanceDynamic* LineMat = UMaterialInstanceDynamic::Create(BaseMat, HLine);
				// Brighter cyan-grey grid lines
				LineMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.1f, 0.15f, 0.2f, 1.0f));
				LineMesh->SetMaterial(0, LineMat);
			}
			LineMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		
		// Vertical line (along Y)
		AStaticMeshActor* VLine = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(),
			FVector(Offset, 0, LineHeight), FRotator::ZeroRotator, SpawnParams);
		if (VLine && CubeMesh)
		{
			UStaticMeshComponent* LineMesh = VLine->GetStaticMeshComponent();
			LineMesh->SetMobility(EComponentMobility::Movable);
			LineMesh->SetStaticMesh(CubeMesh);
			VLine->SetActorScale3D(FVector(LineThickness / 100.0f, GridSize / 100.0f, 0.02f));
			
			if (BaseMat)
			{
				UMaterialInstanceDynamic* LineMat = UMaterialInstanceDynamic::Create(BaseMat, VLine);
				LineMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.1f, 0.15f, 0.2f, 1.0f));
				LineMesh->SetMaterial(0, LineMat);
			}
			LineMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
	
	UE_LOG(LogTemp, Warning, TEXT("SpawnFloorGrid: Created %d grid lines"), NumLines * 4);
}

void AArmaCyclePawn::SpawnArenaWalls()
{
	UWorld* World = GetWorld();
	if (!World) return;

	FVector PlayerPos = GetActorLocation();
	float ArenaSize = 5000.0f;
	float WallThickness = 50.0f;

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// Wall positions and rotations (4 walls forming a square)
	struct WallDef { FVector Pos; FRotator Rot; FVector Scale; };
	TArray<WallDef> Walls = {
		{ FVector(PlayerPos.X + ArenaSize, PlayerPos.Y, TrailHeight/2), FRotator::ZeroRotator, FVector(0.5f, ArenaSize/50, TrailHeight/100) },
		{ FVector(PlayerPos.X - ArenaSize, PlayerPos.Y, TrailHeight/2), FRotator::ZeroRotator, FVector(0.5f, ArenaSize/50, TrailHeight/100) },
		{ FVector(PlayerPos.X, PlayerPos.Y + ArenaSize, TrailHeight/2), FRotator::ZeroRotator, FVector(ArenaSize/50, 0.5f, TrailHeight/100) },
		{ FVector(PlayerPos.X, PlayerPos.Y - ArenaSize, TrailHeight/2), FRotator::ZeroRotator, FVector(ArenaSize/50, 0.5f, TrailHeight/100) },
	};

	for (const auto& WallDef : Walls)
	{
		AActor* WallActor = World->SpawnActor<AActor>(AActor::StaticClass(), WallDef.Pos, WallDef.Rot, SpawnParams);

		if (WallActor)
		{
			UStaticMeshComponent* WallMesh = NewObject<UStaticMeshComponent>(WallActor);
			UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
			if (CubeMesh && WallMesh)
			{
				WallMesh->SetMobility(EComponentMobility::Movable);
				WallMesh->SetStaticMesh(CubeMesh);
				WallMesh->SetWorldScale3D(WallDef.Scale);
				WallMesh->SetupAttachment(WallActor->GetRootComponent());
				WallActor->SetRootComponent(WallMesh);
				WallMesh->RegisterComponent();
				
				// Dark red/purple arena walls using BasicShapeMaterial
				UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr,
					TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
				if (BaseMat)
				{
					UMaterialInstanceDynamic* WallMat = UMaterialInstanceDynamic::Create(BaseMat, WallActor);
					WallMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.3f, 0.05f, 0.1f)); // Dark red
					WallMesh->SetMaterial(0, WallMat);
				}
			}
		}
	}
	
	UE_LOG(LogTemp, Warning, TEXT("SpawnArenaWalls: Created 4 boundary walls around arena"));
}

void AArmaCyclePawn::SpawnAmbientLighting()
{
	UWorld* World = GetWorld();
	if (!World) return;

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// Spawn a VERY bright directional light (sun)
	AActor* SunLight = World->SpawnActor<AActor>(AActor::StaticClass(), FVector::ZeroVector, FRotator(-45.0f, 45.0f, 0.0f), SpawnParams);
	if (SunLight)
	{
		UDirectionalLightComponent* DirLight = NewObject<UDirectionalLightComponent>(SunLight);
		DirLight->SetMobility(EComponentMobility::Movable);
		DirLight->SetIntensity(10.0f); // Very bright
		DirLight->SetLightColor(FColor(255, 255, 255)); // Pure white
		DirLight->SetupAttachment(SunLight->GetRootComponent());
		SunLight->SetRootComponent(DirLight);
		DirLight->RegisterComponent();
	}

	// Spawn a sky light for ambient illumination
	AActor* SkyLightActor = World->SpawnActor<AActor>(AActor::StaticClass(), FVector(0, 0, 1000), FRotator::ZeroRotator, SpawnParams);
	if (SkyLightActor)
	{
		USkyLightComponent* SkyLight = NewObject<USkyLightComponent>(SkyLightActor);
		SkyLight->SetMobility(EComponentMobility::Movable);
		SkyLight->SetIntensity(5.0f); // Bright ambient
		SkyLight->SetLightColor(FColor(200, 220, 255)); // Light blue ambient
		SkyLight->bLowerHemisphereIsBlack = false;
		SkyLight->SourceType = ESkyLightSourceType::SLS_SpecifiedCubemap; // Use solid color
		SkyLight->SetupAttachment(SkyLightActor->GetRootComponent());
		SkyLightActor->SetRootComponent(SkyLight);
		SkyLight->RegisterComponent();
		SkyLight->RecaptureSky();
	}

	// Also spawn a large ambient point light near player for guaranteed visibility
	AActor* AmbientActor = World->SpawnActor<AActor>(AActor::StaticClass(), GetActorLocation() + FVector(0, 0, 500), FRotator::ZeroRotator, SpawnParams);
	if (AmbientActor)
	{
		UPointLightComponent* AmbientLight = NewObject<UPointLightComponent>(AmbientActor);
		AmbientLight->SetMobility(EComponentMobility::Movable);
		AmbientLight->SetIntensity(500000.0f); // Extremely bright
		AmbientLight->SetAttenuationRadius(10000.0f); // Very large radius
		AmbientLight->SetLightColor(FColor(255, 255, 255));
		AmbientLight->SetupAttachment(AmbientActor->GetRootComponent());
		AmbientActor->SetRootComponent(AmbientLight);
		AmbientLight->RegisterComponent();
	}

	UE_LOG(LogTemp, Warning, TEXT("SpawnAmbientLighting: Created directional, sky, and ambient point lights"));
}

void AArmaCyclePawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	// ========== PROCESS PENDING TURNS (like original gCycleMovement::Timestep) ==========
	// Execute queued turns when their time has come
	if (bIsAlive)
	{
		ProcessPendingTurns();
	}

	// ========== IMMEDIATE BOUNDARY ENFORCEMENT ==========
	// This runs EVERY tick to catch any escaped cycles (failsafe)
	const float HardBoundary = 4950.0f;
	FVector CurrentPos = GetActorLocation();
	bool bNeedsTeleport = false;
	
	if (FMath::Abs(CurrentPos.X) > HardBoundary || FMath::Abs(CurrentPos.Y) > HardBoundary)
	{
		// Cycle is outside arena - clamp it back inside
		CurrentPos.X = FMath::Clamp(CurrentPos.X, -HardBoundary, HardBoundary);
		CurrentPos.Y = FMath::Clamp(CurrentPos.Y, -HardBoundary, HardBoundary);
		SetActorLocation(CurrentPos);
		bNeedsTeleport = true;
		
		UE_LOG(LogTemp, Error, TEXT("BOUNDARY VIOLATION! Cycle was outside arena, clamped to (%.1f, %.1f)"), 
			CurrentPos.X, CurrentPos.Y);
		
		// If we were way outside, probably a bug - kill the cycle
		if (FMath::Abs(GetActorLocation().X) > 10000.0f || FMath::Abs(GetActorLocation().Y) > 10000.0f)
		{
			if (bIsAlive && IsVulnerable())
			{
				UE_LOG(LogTemp, Error, TEXT("*** DEATH! Cycle escaped arena boundary ***"));
				Die();
			}
		}
	}

	// If menu is open, only draw menu
	if (bMenuOpen)
	{
		DrawMenu();
		return;
	}

	if (!bIsAlive) 
	{
		// When dead, just update camera and HUD
		UpdateCamera(DeltaTime);
		DrawHUD();
		return;
	}
	
	// Update invulnerability blink effect
	UpdateInvulnerabilityBlink();

	// ========== ARMAGETRON-STYLE SPEED DECAY ==========
	// Speed naturally decays toward base speed
	float SpeedDiff = BaseSpeed - MoveSpeed;
	if (MoveSpeed < BaseSpeed)
	{
		MoveSpeed += SpeedDiff * SpeedDecayBelow * DeltaTime;
	}
	else if (MoveSpeed > BaseSpeed)
	{
		MoveSpeed += SpeedDiff * SpeedDecayAbove * DeltaTime;
	}
	MoveSpeed = FMath::Clamp(MoveSpeed, 100.0f, MaxSpeed);

	// Update wall proximity acceleration (speed boost near walls)
	UpdateWallAcceleration(DeltaTime);

	// Update rubber regeneration
	UpdateRubber(DeltaTime);
	
	// Update wall decay (remove old walls when exceeding max length)
	UpdateWallDecay();

	// ========== COLLISION CHECK BEFORE MOVING (Armagetron Style) ==========
	// Key insight: Check where we WOULD end up, and if we'd hit a wall, stop there
	// Now uses GLOBAL wall registry so we detect ALL walls (player, AI, and rim)
	FVector2D MyPos2D(GetActorLocation().X, GetActorLocation().Y);
	FVector2D MyDir2D(MoveDirection.X, MoveDirection.Y);
	
	// EMERGENCY BOUNDARY CHECK - If somehow outside arena, teleport back NOW
	const float EmergencyBoundary = 5500.0f;
	if (FMath::Abs(MyPos2D.X) > EmergencyBoundary || FMath::Abs(MyPos2D.Y) > EmergencyBoundary)
	{
		UE_LOG(LogTemp, Error, TEXT("!!! EMERGENCY TELEPORT !!! Cycle at (%.1f, %.1f) - forcing to origin!"), 
			MyPos2D.X, MyPos2D.Y);
		SetActorLocation(FVector(0, 0, 92.0f));
		MoveDirection = FVector(1, 0, 0);
		MoveSpeed = BaseSpeed;
		MyPos2D = FVector2D(0, 0);
		MyDir2D = FVector2D(1, 0);
	}
	
	float DesiredMoveDistance = MoveSpeed * DeltaTime;
	const float WallGracePeriod = 0.3f;
	
	// Use global wall registry for collision detection
	UArmaWallRegistry* WallRegistry = UArmaWallRegistry::Get(GetWorld());
	FArmaRegisteredWall HitWallInfo;
	float ClosestHitDist = MAX_FLT;
	
	if (WallRegistry)
	{
		// Query registry for all walls (including AI and rim walls)
		// Pass ourselves as ignore owner with grace period for our own walls
		ClosestHitDist = WallRegistry->RaycastWalls(
			MyPos2D, 
			MyDir2D, 
			DesiredMoveDistance + 50.0f, 
			this,  // Ignore our own recent walls
			WallGracePeriod, 
			HitWallInfo
		);
		
		// Debug logging for collision detection
		static int LogCounter = 0;
		if (LogCounter++ % 60 == 0)  // Log every 60 frames
		{
			int32 TotalWalls = WallRegistry->GetWallCount();
			UE_LOG(LogTemp, Display, TEXT("COLLISION CHECK: Pos=(%.1f,%.1f) Dir=(%.2f,%.2f) HitDist=%.1f TotalWalls=%d WallType=%s"),
				MyPos2D.X, MyPos2D.Y, MyDir2D.X, MyDir2D.Y, ClosestHitDist, TotalWalls,
				HitWallInfo.WallType == EArmaWallType::Rim ? TEXT("RIM") : TEXT("CYCLE"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("NO WALL REGISTRY!"));
	}
	
	DistanceToWall = ClosestHitDist;
	
	// ========== MOVEMENT WITH COLLISION RESPONSE ==========
	float ActualMoveDistance = DesiredMoveDistance;
	bool bHitWall = (ClosestHitDist < MAX_FLT);
	
	// Check if we're in the turn grace period (digging escape window)
	float CurrentTime = GetWorld()->GetTimeSeconds();
	bool bInTurnGrace = (CurrentTime - LastTurnTime) < TurnGracePeriod;
	
	// During turn grace period, use a much smaller minimum distance
	// This allows the "dig" mechanic where you can turn into a wall and escape
	float EffectiveMinDistance = bInTurnGrace ? 0.1f : MinWallDistance;
	
	// Would we hit a wall this frame?
	if (bHitWall && ClosestHitDist < DesiredMoveDistance + EffectiveMinDistance)
	{
		bIsGrinding = true;
		
		// How far past the safe distance would we go?
		float SafeDistance = ClosestHitDist - EffectiveMinDistance;
		
		if (SafeDistance < 0)
		{
			// We're already too close or would end up inside the wall
			if (CurrentRubber > 0)
			{
				// Use rubber to survive - stay at minimum distance
				// During grace period, use MUCH less rubber (digging is cheaper)
				float RubberMultiplier = bInTurnGrace ? 0.1f : 2.0f;
				float RubberNeeded = FMath::Abs(SafeDistance) * RubberMultiplier;
				CurrentRubber = FMath::Max(0.0f, CurrentRubber - RubberNeeded);
				
				// During grace period, allow SOME movement even when close
				// This is key to the dig mechanic - you can squeeze through
				ActualMoveDistance = bInTurnGrace ? DesiredMoveDistance * 0.5f : 0.0f;
				
				// Sparks
				if (ClosestHitDist < 30.0f)
				{
					FVector2D HitPoint = MyPos2D + MyDir2D * ClosestHitDist;
					SpawnSpark(FVector(HitPoint.X, HitPoint.Y, GetActorLocation().Z), 
						FVector(-MoveDirection.X, -MoveDirection.Y, 0));
				}
			}
			else if (IsVulnerable() && !bInTurnGrace)
			{
				// No rubber, wall collision = death (but not during turn grace)
				UE_LOG(LogTemp, Error, TEXT("*** DEATH! Hit wall, no rubber, dist=%.1f ***"), ClosestHitDist);
				Die();
				return;
			}
			else if (bInTurnGrace)
			{
				// During turn grace with no rubber, allow movement but slower
				ActualMoveDistance = DesiredMoveDistance * 0.3f;
				UE_LOG(LogTemp, Warning, TEXT("Turn grace: Allowing movement despite close wall (dist=%.1f)"), ClosestHitDist);
			}
		}
		else if (SafeDistance < DesiredMoveDistance)
		{
			// We can move some, but not the full distance
			if (CurrentRubber > 0)
			{
				float Overshoot = DesiredMoveDistance - SafeDistance;
				float RubberMultiplier = bInTurnGrace ? 0.1f : 0.3f;
				float RubberNeeded = Overshoot * RubberMultiplier;
				CurrentRubber = FMath::Max(0.0f, CurrentRubber - RubberNeeded);
				ActualMoveDistance = SafeDistance;  // Stop at safe distance
				
				if (ClosestHitDist < 30.0f)
				{
					FVector2D HitPoint = MyPos2D + MyDir2D * ClosestHitDist;
					SpawnSpark(FVector(HitPoint.X, HitPoint.Y, GetActorLocation().Z), 
						FVector(-MoveDirection.X, -MoveDirection.Y, 0));
				}
			}
			else if (IsVulnerable() && SafeDistance <= 0 && !bInTurnGrace)
			{
				UE_LOG(LogTemp, Error, TEXT("*** DEATH! Would hit wall, no rubber ***"));
				Die();
				return;
			}
			else
			{
				ActualMoveDistance = FMath::Max(0.0f, SafeDistance);
			}
		}
	}
	else
	{
		bIsGrinding = false;
	}
	
	// ========== ACTUALLY MOVE (clamped to safe distance) ==========
	FVector DesiredMove = MoveDirection * ActualMoveDistance;
	FVector NewLocation = GetActorLocation() + DesiredMove;
	
	// HARD BOUNDARY CLAMP - Failsafe to prevent escaping arena
	// This catches any edge cases where raycast collision might miss
	const float ArenaHalfSize = 4950.0f;  // Slightly inside the 5000 rim
	bool bWasOutside = false;
	
	if (NewLocation.X > ArenaHalfSize)
	{
		NewLocation.X = ArenaHalfSize;
		bWasOutside = true;
	}
	else if (NewLocation.X < -ArenaHalfSize)
	{
		NewLocation.X = -ArenaHalfSize;
		bWasOutside = true;
	}
	
	if (NewLocation.Y > ArenaHalfSize)
	{
		NewLocation.Y = ArenaHalfSize;
		bWasOutside = true;
	}
	else if (NewLocation.Y < -ArenaHalfSize)
	{
		NewLocation.Y = -ArenaHalfSize;
		bWasOutside = true;
	}
	
	// If we hit the boundary and have no rubber, die
	if (bWasOutside && CurrentRubber <= 0 && IsVulnerable())
	{
		UE_LOG(LogTemp, Error, TEXT("*** DEATH! Hit arena boundary ***"));
		Die();
		return;
	}
	else if (bWasOutside)
	{
		// Use rubber to survive boundary collision
		CurrentRubber = FMath::Max(0.0f, CurrentRubber - 10.0f);
	}
	
	SetActorLocation(NewLocation);

	// ========== POST-MOVEMENT COLLISION CHECK (Safety Net) ==========
	// Check if we're now too close to any wall (catches edge cases)
	if (WallRegistry && bIsAlive)
	{
		FVector2D FinalPos2D(NewLocation.X, NewLocation.Y);
		FArmaRegisteredWall NearbyWall;
		
		// Check in all 4 cardinal directions for nearby walls
		FVector2D Directions[4] = {
			FVector2D(1, 0), FVector2D(-1, 0), FVector2D(0, 1), FVector2D(0, -1)
		};
		
		for (const FVector2D& CheckDir : Directions)
		{
			float NearbyDist = WallRegistry->RaycastWalls(
				FinalPos2D, CheckDir, MinWallDistance * 2.0f, 
				this, WallGracePeriod, NearbyWall);
			
			if (NearbyDist < MinWallDistance && NearbyWall.WallType != EArmaWallType::Cycle)
			{
				// Too close to a rim wall - use rubber or die
				if (CurrentRubber > 0)
				{
					CurrentRubber = FMath::Max(0.0f, CurrentRubber - 5.0f);
				}
				else if (IsVulnerable() && !bInTurnGrace)
				{
					UE_LOG(LogTemp, Error, TEXT("*** DEATH! Post-move collision check failed ***"));
					Die();
					return;
				}
			}
			else if (NearbyDist < 1.0f && NearbyWall.OwnerActor != this)
			{
				// Extremely close to someone else's wall - definitely collision
				if (CurrentRubber > 0)
				{
					CurrentRubber = FMath::Max(0.0f, CurrentRubber - 20.0f);
				}
				else if (IsVulnerable())
				{
					UE_LOG(LogTemp, Error, TEXT("*** DEATH! Inside another cycle's wall ***"));
					Die();
					return;
				}
			}
		}
	}

	// Smoothly rotate pawn to face movement direction
	float DeltaYaw = FMath::FindDeltaAngleDegrees(CurrentPawnYaw, TargetPawnYaw);
	float RotationThisFrame = FMath::Sign(DeltaYaw) * FMath::Min(FMath::Abs(DeltaYaw), 720.0f * DeltaTime);
	CurrentPawnYaw = FMath::UnwindDegrees(CurrentPawnYaw + RotationThisFrame);
	
	// Snap if close enough
	if (FMath::Abs(DeltaYaw) < 1.0f)
	{
		CurrentPawnYaw = TargetPawnYaw;
	}
	
	SetActorRotation(FRotator(0, CurrentPawnYaw, 0));

	// Update the current wall segment
	UpdateCurrentWall();

	// Update camera to follow bike rotation
	UpdateCamera(DeltaTime);

	// Draw HUD
	DrawHUD();
	
	// Draw debug visualization
	DrawDebugRays();
	DrawDebugSliders();
	
	// Draw ESC menu if open
	DrawMenu();
}

void AArmaCyclePawn::UpdateCamera(float DeltaTime)
{
	if (!CameraArm) return;

	// Camera arm is attached to root which rotates with the pawn
	// We apply pitch and the user's yaw offset relative to the pawn's facing direction
	// The pawn smoothly rotates via CurrentPawnYaw, so the camera follows smoothly
	FRotator ArmRotation = FRotator(CameraPitch, CameraYawOffset, 0);
	CameraArm->SetRelativeRotation(ArmRotation);
	
	// Update arm length for zoom
	CameraArm->TargetArmLength = FMath::FInterpTo(CameraArm->TargetArmLength, TargetArmLength, DeltaTime, 5.0f);
}

void AArmaCyclePawn::DrawHUD()
{
	if (GEngine)
	{
		// Show wall count with collision info
		GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Magenta,
			FString::Printf(TEXT("WALLS: %d (Collision Active) | ESC=Menu | Rubber: %.0f"), WallActors.Num(), CurrentRubber));
		
		// Round and status
		FString StatusStr = bIsAlive ? (IsVulnerable() ? TEXT("ALIVE") : TEXT("INVULNERABLE")) : TEXT("DEAD");
		FColor StatusColor = bIsAlive ? (IsVulnerable() ? FColor::Green : FColor::Yellow) : FColor::Red;
		GEngine->AddOnScreenDebugMessage(0, 0.0f, FColor::Cyan, 
			FString::Printf(TEXT("=== ROUND %d === Deaths: %d"), CurrentRound, DeathCount));
		
		// Rubber meter (visual bar)
		int32 RubberBars = FMath::RoundToInt((CurrentRubber / MaxRubber) * 20);
		FString RubberBar = TEXT("[");
		for (int32 i = 0; i < 20; i++)
		{
			RubberBar += (i < RubberBars) ? TEXT("|") : TEXT(" ");
		}
		RubberBar += TEXT("]");
		FColor RubberColor = CurrentRubber > 30.0f ? FColor::Cyan : (CurrentRubber > 10.0f ? FColor::Yellow : FColor::Red);
		GEngine->AddOnScreenDebugMessage(1, 0.0f, RubberColor, 
			FString::Printf(TEXT("Rubber: %s %.0f%%"), *RubberBar, (CurrentRubber / MaxRubber) * 100));
		
		// Speed and acceleration info
		FColor SpeedColor = bIsGrinding ? FColor::Orange : FColor::Green;
		GEngine->AddOnScreenDebugMessage(2, 0.0f, SpeedColor, 
			FString::Printf(TEXT("Speed: %.0f / %.0f %s"), MoveSpeed, MaxSpeed, bIsGrinding ? TEXT("(GRINDING!)") : TEXT("")));
		
		// Wall distance
		GEngine->AddOnScreenDebugMessage(3, 0.0f, FColor::White,
			FString::Printf(TEXT("Wall Dist: %.0f | Walls: %d"), DistanceToWall, WallCount));
		
		// Controls
		GEngine->AddOnScreenDebugMessage(4, 0.0f, StatusColor,
			FString::Printf(TEXT("Status: %s | A/D: Turn | Mouse: Look | Scroll: Zoom"), *StatusStr));
		
		// Death message
		if (!bIsAlive)
		{
			GEngine->AddOnScreenDebugMessage(5, 0.0f, FColor::Red, TEXT(">>> PRESS SPACE TO RESPAWN <<<"));
		}
	}
}

void AArmaCyclePawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Turn controls - direct key binding only
	PlayerInputComponent->BindKey(EKeys::A, IE_Pressed, this, &AArmaCyclePawn::TurnLeft);
	PlayerInputComponent->BindKey(EKeys::D, IE_Pressed, this, &AArmaCyclePawn::TurnRight);
	PlayerInputComponent->BindKey(EKeys::Left, IE_Pressed, this, &AArmaCyclePawn::TurnLeft);
	PlayerInputComponent->BindKey(EKeys::Right, IE_Pressed, this, &AArmaCyclePawn::TurnRight);

	// Brake
	PlayerInputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &AArmaCyclePawn::OnBrakePressed);
	PlayerInputComponent->BindKey(EKeys::SpaceBar, IE_Released, this, &AArmaCyclePawn::OnBrakeReleased);

	// Mouse look - use the axis names from DefaultInput.ini
	PlayerInputComponent->BindAxis("MouseX", this, &AArmaCyclePawn::OnMouseX);
	PlayerInputComponent->BindAxis("MouseY", this, &AArmaCyclePawn::OnMouseY);

	// Debug controls
	PlayerInputComponent->BindKey(EKeys::Tab, IE_Pressed, this, &AArmaCyclePawn::DebugNextVar);
	PlayerInputComponent->BindKey(EKeys::Q, IE_Pressed, this, &AArmaCyclePawn::DebugPrevVar);
	PlayerInputComponent->BindKey(EKeys::E, IE_Pressed, this, &AArmaCyclePawn::DebugNextVar);
	PlayerInputComponent->BindKey(EKeys::Up, IE_Pressed, this, &AArmaCyclePawn::DebugIncreaseVar);
	PlayerInputComponent->BindKey(EKeys::Down, IE_Pressed, this, &AArmaCyclePawn::DebugDecreaseVar);
	PlayerInputComponent->BindKey(EKeys::F1, IE_Pressed, this, &AArmaCyclePawn::DebugToggleDraw);

	// ESC Menu
	PlayerInputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &AArmaCyclePawn::ToggleMenu);
	PlayerInputComponent->BindKey(EKeys::W, IE_Pressed, this, &AArmaCyclePawn::MenuUp);
	PlayerInputComponent->BindKey(EKeys::S, IE_Pressed, this, &AArmaCyclePawn::MenuDown);
	PlayerInputComponent->BindKey(EKeys::Enter, IE_Pressed, this, &AArmaCyclePawn::MenuSelect);
	
	// Zoom - use mouse wheel
	PlayerInputComponent->BindKey(EKeys::MouseScrollUp, IE_Pressed, this, &AArmaCyclePawn::OnZoomIn);
	PlayerInputComponent->BindKey(EKeys::MouseScrollDown, IE_Pressed, this, &AArmaCyclePawn::OnZoomOut);
}

bool AArmaCyclePawn::CanMakeTurn() const
{
	// ========== TURN DELAY (sg_delayCycle from original Armagetron) ==========
	// Returns true if enough time has passed since the last turn AND no pending turns
	float CurrentTime = GetWorld()->GetTimeSeconds();
	return PendingTurns.Num() == 0 && (CurrentTime - LastTurnTime) >= TurnDelay;
}

void AArmaCyclePawn::ProcessPendingTurns()
{
	// ========== EXECUTE QUEUED TURNS (like original gCycleMovement::Timestep) ==========
	// Execute pending turns when their time has come
	if (PendingTurns.Num() > 0)
	{
		float CurrentTime = GetWorld()->GetTimeSeconds();
		float NextTurnTime = LastTurnTime + TurnDelay;
		
		if (CurrentTime >= NextTurnTime)
		{
			int32 Direction = PendingTurns[0];
			PendingTurns.RemoveAt(0);
			
			// Execute the turn directly (bypassing the queue check)
			if (Direction < 0)
			{
				ExecuteTurnLeft();
			}
			else if (Direction > 0)
			{
				ExecuteTurnRight();
			}
		}
	}
}

void AArmaCyclePawn::ExecuteTurnLeft()
{
	// Internal function - actually executes the turn (called from queue or directly)
	
	// ========== DIGGING MECHANIC (Armagetron advanced technique) ==========
	// When grinding against a wall, turning INTO the wall is allowed but costs extra rubber
	// This creates the "dig" that lets you escape tight situations
	if (bIsGrinding && DistanceToWall < MinWallDistance * 5.0f)
	{
		// Consume extra rubber for the dig turn
		float DiggingCost = DiggingRubberMultiplier * (MinWallDistance * 5.0f - DistanceToWall);
		CurrentRubber = FMath::Max(0.0f, CurrentRubber - DiggingCost);
		
		UE_LOG(LogTemp, Warning, TEXT("DIG TURN LEFT: Cost %.1f rubber, remaining %.1f"), DiggingCost, CurrentRubber);
		
		// Update gap on right side (we're turning left, so gap opens on right)
		GapRight = FMath::Max(GapRight, DistanceToWall);
	}
	
	// ========== TURN SPEED FACTOR (sg_cycleTurnSpeedFactor) ==========
	// Each turn costs 5% of current speed
	MoveSpeed *= TurnSpeedFactor;
	
	// Record turn time for grace period AND turn delay enforcement
	LastTurnTime = GetWorld()->GetTimeSeconds();

	FVector OldDir = MoveDirection;
	FVector CurrentPos = GetActorLocation();
	
	// Finalize the current wall segment (make it permanent)
	if (CurrentWallActor)
	{
		// Add current wall actor to the permanent list
		WallActors.Add(CurrentWallActor);
		
		// Store 2D wall segment in local array (for legacy code)
		FVector2D SegStart(CurrentWallStart.X, CurrentWallStart.Y);
		FVector2D SegEnd(CurrentPos.X, CurrentPos.Y);
		float WallTime = GetWorld()->GetTimeSeconds();
		WallSegments.Add(FWallSegment(SegStart, SegEnd, CurrentWallActor, WallTime));
		WallCount++;
		
		// Update final position of current wall in registry (it was already registered in StartNewWallSegment)
		// No need to re-register - just finalize the endpoint
		if (UArmaWallRegistry* WallRegistry = UArmaWallRegistry::Get(GetWorld()))
		{
			if (CurrentWallID != 0)
			{
				WallRegistry->UpdateWallEnd(CurrentWallID, SegEnd);
			}
		}
		
		UE_LOG(LogTemp, Warning, TEXT("TurnLeft: Wall %d finalized at time %.1f"), WallCount, WallTime);
		CurrentWallActor = nullptr;
		CurrentWallID = 0;
	}

	// Turn 90 degrees left (counter-clockwise when viewed from above)
	FVector NewDir;
	if (FMath::Abs(MoveDirection.X) > 0.5f)
	{
		NewDir = FVector(0.0f, -MoveDirection.X, 0.0f);
	}
	else
	{
		NewDir = FVector(MoveDirection.Y, 0.0f, 0.0f);
	}
	MoveDirection = NewDir.GetSafeNormal();
	
	// Set target yaw for smooth rotation
	TargetPawnYaw = MoveDirection.Rotation().Yaw;

	// Start new wall segment from current position
	StartNewWallSegment();

	UE_LOG(LogTemp, Warning, TEXT("TURN LEFT: Dir %s -> %s, Pos: %s, TargetYaw: %.1f"), 
		*OldDir.ToString(), *MoveDirection.ToString(), *CurrentPos.ToString(), TargetPawnYaw);
}

void AArmaCyclePawn::TurnLeft()
{
	if (!bIsAlive || bMenuOpen) return;
	
	// ========== TURN QUEUEING (like original gCycleMovement::DoTurn) ==========
	// If we can turn now, do it immediately. Otherwise queue it.
	if (CanMakeTurn())
	{
		ExecuteTurnLeft();
	}
	else
	{
		// Queue the turn (up to TurnMemory turns)
		if (PendingTurns.Num() < TurnMemory)
		{
			PendingTurns.Add(-1);  // -1 = left
			UE_LOG(LogTemp, Verbose, TEXT("TurnLeft queued (%d pending)"), PendingTurns.Num());
		}
		else if (PendingTurns.Num() > 0 && PendingTurns.Last() != -1)
		{
			// Opposite turns cancel out
			PendingTurns.Pop();
			UE_LOG(LogTemp, Verbose, TEXT("TurnLeft cancelled opposite turn (%d pending)"), PendingTurns.Num());
		}
	}
}

void AArmaCyclePawn::ExecuteTurnRight()
{
	// Internal function - actually executes the turn (called from queue or directly)
	
	// ========== DIGGING MECHANIC (Armagetron advanced technique) ==========
	// When grinding against a wall, turning INTO the wall is allowed but costs extra rubber
	// This creates the "dig" that lets you escape tight situations
	if (bIsGrinding && DistanceToWall < MinWallDistance * 5.0f)
	{
		// Consume extra rubber for the dig turn
		float DiggingCost = DiggingRubberMultiplier * (MinWallDistance * 5.0f - DistanceToWall);
		CurrentRubber = FMath::Max(0.0f, CurrentRubber - DiggingCost);
		
		UE_LOG(LogTemp, Warning, TEXT("DIG TURN RIGHT: Cost %.1f rubber, remaining %.1f"), DiggingCost, CurrentRubber);
		
		// Update gap on left side (we're turning right, so gap opens on left)
		GapLeft = FMath::Max(GapLeft, DistanceToWall);
	}
	
	// ========== TURN SPEED FACTOR (sg_cycleTurnSpeedFactor) ==========
	// Each turn costs 5% of current speed
	MoveSpeed *= TurnSpeedFactor;
	
	// Record turn time for grace period AND turn delay enforcement
	LastTurnTime = GetWorld()->GetTimeSeconds();

	FVector OldDir = MoveDirection;
	FVector CurrentPos = GetActorLocation();
	
	// Finalize the current wall segment (make it permanent)
	if (CurrentWallActor)
	{
		// Add current wall actor to the permanent list
		WallActors.Add(CurrentWallActor);
		
		// Store 2D wall segment in local array (for legacy code)
		FVector2D SegStart(CurrentWallStart.X, CurrentWallStart.Y);
		FVector2D SegEnd(CurrentPos.X, CurrentPos.Y);
		float WallTime = GetWorld()->GetTimeSeconds();
		WallSegments.Add(FWallSegment(SegStart, SegEnd, CurrentWallActor, WallTime));
		WallCount++;
		
		// Update final position of current wall in registry (it was already registered in StartNewWallSegment)
		// No need to re-register - just finalize the endpoint
		if (UArmaWallRegistry* WallRegistry = UArmaWallRegistry::Get(GetWorld()))
		{
			if (CurrentWallID != 0)
			{
				WallRegistry->UpdateWallEnd(CurrentWallID, SegEnd);
			}
		}
		
		UE_LOG(LogTemp, Warning, TEXT("TurnRight: Wall %d finalized at time %.1f"), WallCount, WallTime);
		CurrentWallActor = nullptr;
		CurrentWallID = 0;
	}

	// Turn 90 degrees right (clockwise when viewed from above)
	FVector NewDir;
	if (FMath::Abs(MoveDirection.X) > 0.5f)
	{
		NewDir = FVector(0.0f, MoveDirection.X, 0.0f);
	}
	else
	{
		NewDir = FVector(-MoveDirection.Y, 0.0f, 0.0f);
	}
	MoveDirection = NewDir.GetSafeNormal();
	
	// Set target yaw for smooth rotation
	TargetPawnYaw = MoveDirection.Rotation().Yaw;

	// Start new wall segment from current position
	StartNewWallSegment();

	UE_LOG(LogTemp, Warning, TEXT("TURN RIGHT: Dir %s -> %s, Pos: %s, TargetYaw: %.1f"), 
		*OldDir.ToString(), *MoveDirection.ToString(), *CurrentPos.ToString(), TargetPawnYaw);
}

void AArmaCyclePawn::TurnRight()
{
	if (!bIsAlive || bMenuOpen) return;
	
	// ========== TURN QUEUEING (like original gCycleMovement::DoTurn) ==========
	// If we can turn now, do it immediately. Otherwise queue it.
	if (CanMakeTurn())
	{
		ExecuteTurnRight();
	}
	else
	{
		// Queue the turn (up to TurnMemory turns)
		if (PendingTurns.Num() < TurnMemory)
		{
			PendingTurns.Add(1);  // +1 = right
			UE_LOG(LogTemp, Verbose, TEXT("TurnRight queued (%d pending)"), PendingTurns.Num());
		}
		else if (PendingTurns.Num() > 0 && PendingTurns.Last() != 1)
		{
			// Opposite turns cancel out
			PendingTurns.Pop();
			UE_LOG(LogTemp, Verbose, TEXT("TurnRight cancelled opposite turn (%d pending)"), PendingTurns.Num());
		}
	}
}

void AArmaCyclePawn::StartNewWallSegment()
{
	CurrentWallStart = GetActorLocation();
	
	// Create a new current wall actor if needed
	CreateCurrentWallActor();
	
	// CRITICAL FIX: Register the current wall in the global registry IMMEDIATELY
	// This ensures other players can collide with walls even before a turn is made
	// In Armagetron, currentWall->PartialCopyIntoGrid() does this continuously
	if (UArmaWallRegistry* WallRegistry = UArmaWallRegistry::Get(GetWorld()))
	{
		FVector2D SegStart(CurrentWallStart.X, CurrentWallStart.Y);
		FVector2D SegEnd(CurrentWallStart.X, CurrentWallStart.Y); // Same point initially
		CurrentWallID = WallRegistry->RegisterWall(SegStart, SegEnd, EArmaWallType::Cycle, this, CurrentWallActor);
		UE_LOG(LogTemp, Warning, TEXT("StartNewWallSegment: Registered wall ID %d at (%.1f, %.1f)"), 
			CurrentWallID, SegStart.X, SegStart.Y);
	}
}

void AArmaCyclePawn::CreateCurrentWallActor()
{
	UWorld* World = GetWorld();
	if (!World) return;
	
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	
	CurrentWallActor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), 
		CurrentWallStart, FRotator::ZeroRotator, SpawnParams);
	
	if (CurrentWallActor)
	{
		UStaticMeshComponent* MeshComp = CurrentWallActor->GetStaticMeshComponent();
		if (MeshComp)
		{
			MeshComp->SetMobility(EComponentMobility::Movable);
			
			UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
			if (CubeMesh)
			{
				MeshComp->SetStaticMesh(CubeMesh);
			}
			
			// Create bright visible material using CycleColor
			// BasicShapeMaterial uses "Color" as base color (values >1 are clamped, not emissive)
			UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr,
				TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
			if (!BaseMat)
			{
				BaseMat = LoadObject<UMaterialInterface>(nullptr,
					TEXT("/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"));
			}
			if (BaseMat)
			{
				CurrentWallMaterial = UMaterialInstanceDynamic::Create(BaseMat, CurrentWallActor);
				// Use CycleColor directly - bright but not >1 so it renders properly
				// Make it slightly brighter by boosting toward white
				FLinearColor WallColor = FLinearColor::LerpUsingHSV(CycleColor, FLinearColor::White, 0.3f);
				CurrentWallMaterial->SetVectorParameterValue(TEXT("Color"), WallColor);
				MeshComp->SetMaterial(0, CurrentWallMaterial);
				UE_LOG(LogTemp, Warning, TEXT("Wall material created: Color=(%f,%f,%f)"), WallColor.R, WallColor.G, WallColor.B);
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("CreateCurrentWallActor: Failed to load any material!"));
			}
			
			MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			MeshComp->SetVisibility(true);
			// Start hidden - will be shown when segment is long enough (prevents cube artifacts)
			MeshComp->SetHiddenInGame(true);
		}
		
		// Add glow light that follows the wall (uses CycleColor)
		UPointLightComponent* GlowLight = NewObject<UPointLightComponent>(CurrentWallActor, TEXT("WallGlow"));
		if (GlowLight)
		{
			GlowLight->SetupAttachment(CurrentWallActor->GetRootComponent());
			GlowLight->SetMobility(EComponentMobility::Movable);
			GlowLight->SetRelativeLocation(FVector(0, 0, TrailHeight / 2.0f));
			GlowLight->SetIntensity(30000.0f);
			GlowLight->SetAttenuationRadius(300.0f);
			// Use CycleColor for wall glow
			GlowLight->SetLightColor(FColor(
				FMath::Clamp(int32(CycleColor.R * 255), 0, 255),
				FMath::Clamp(int32(CycleColor.G * 255), 0, 255),
				FMath::Clamp(int32(CycleColor.B * 255), 0, 255)
			));
			GlowLight->RegisterComponent();
		}
	}
}

void AArmaCyclePawn::UpdateCurrentWall()
{
	// Update the current wall segment to stretch from start to current position
	if (!CurrentWallActor) 
	{
		UE_LOG(LogTemp, Error, TEXT("UpdateCurrentWall: No CurrentWallActor!"));
		return;
	}
	
	FVector CurrentPos = GetActorLocation();
	FVector Direction = CurrentPos - CurrentWallStart;
	float Length = Direction.Size();
	
	// CRITICAL FIX: Always update registry even for short walls (for collision detection)
	// This is how Armagetron handles it - currentWall->Update() + PartialCopyIntoGrid()
	if (CurrentWallID != 0)
	{
		if (UArmaWallRegistry* WallRegistry = UArmaWallRegistry::Get(GetWorld()))
		{
			FVector2D NewEnd(CurrentPos.X, CurrentPos.Y);
			WallRegistry->UpdateWallEnd(CurrentWallID, NewEnd);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("UpdateCurrentWall: No WallRegistry!"));
		}
	}
	else
	{
		// Log periodically if wall ID is 0
		static int WarnCounter = 0;
		if (WarnCounter++ % 120 == 0)
		{
			UE_LOG(LogTemp, Error, TEXT("UpdateCurrentWall: CurrentWallID is 0!"));
		}
	}
	
	// Hide the wall mesh if it's too short (prevents cube artifacts at corners)
	const float MinVisibleLength = 10.0f;
	if (Length < MinVisibleLength)
	{
		CurrentWallActor->SetActorHiddenInGame(true);
		return;
	}
	
	// Show and update the wall
	CurrentWallActor->SetActorHiddenInGame(false);
	
	// Calculate center and rotation
	FVector Center = (CurrentWallStart + CurrentPos) / 2.0f;
	Center.Z = TrailHeight / 2.0f;
	
	FRotator WallRotation = Direction.Rotation();
	
	// Scale: X is length, Y is width, Z is height
	float ScaleX = Length / 100.0f;
	float ScaleY = 0.15f; // 15 units wide - visible but still slim light trail
	float ScaleZ = TrailHeight / 100.0f;
	
	CurrentWallActor->SetActorLocation(Center);
	CurrentWallActor->SetActorRotation(WallRotation);
	CurrentWallActor->SetActorScale3D(FVector(ScaleX, ScaleY, ScaleZ));
}

AActor* AArmaCyclePawn::SpawnWallSegment(FVector Start, FVector End)
{
	UWorld* World = GetWorld();
	if (!World) return nullptr;

	// Calculate wall dimensions
	FVector Direction = End - Start;
	float Length = Direction.Size();
	
	if (Length < 10.0f) return nullptr; // Skip tiny segments

	FVector Center = (Start + End) / 2.0f;
	Center.Z = TrailHeight / 2.0f; // Wall center at half height above ground

	// Determine rotation based on direction
	FRotator WallRotation = Direction.Rotation();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// Use AStaticMeshActor for reliable mesh spawning
	AStaticMeshActor* WallActor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Center, WallRotation, SpawnParams);

	if (WallActor)
	{
		UStaticMeshComponent* WallMeshComp = WallActor->GetStaticMeshComponent();
		if (WallMeshComp)
		{
			WallMeshComp->SetMobility(EComponentMobility::Movable);
			
			UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
			if (CubeMesh)
			{
				WallMeshComp->SetStaticMesh(CubeMesh);
			}
			
			// Scale: X is length, Y is width, Z is height
			float ScaleX = (Length + 10.0f) / 100.0f;  // Smaller overlap
			float ScaleY = 0.15f; // 15 units wide - visible but slim
			float ScaleZ = TrailHeight / 100.0f;
			WallActor->SetActorScale3D(FVector(ScaleX, ScaleY, ScaleZ));
			
			// Use CycleColor for wall material - bright but properly clamped
			UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr,
				TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
			if (!BaseMat)
			{
				BaseMat = LoadObject<UMaterialInterface>(nullptr,
					TEXT("/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"));
			}
			if (BaseMat)
			{
				UMaterialInstanceDynamic* WallMat = UMaterialInstanceDynamic::Create(BaseMat, WallActor);
				// Use CycleColor boosted toward white for visibility
				FLinearColor WallColor = FLinearColor::LerpUsingHSV(CycleColor, FLinearColor::White, 0.3f);
				WallMat->SetVectorParameterValue(TEXT("Color"), WallColor);
				WallMeshComp->SetMaterial(0, WallMat);
			}
			
			// Enable collision so cycle can't pass through
			WallMeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			WallMeshComp->SetCollisionObjectType(ECC_WorldDynamic);
			WallMeshComp->SetCollisionResponseToAllChannels(ECR_Block);
			WallMeshComp->SetGenerateOverlapEvents(true);
			WallMeshComp->SetNotifyRigidBodyCollision(true);
			WallMeshComp->SetVisibility(true);
			WallMeshComp->SetHiddenInGame(false);
		}
		
		// Add point light for glow effect using CycleColor
		UPointLightComponent* WallLight = NewObject<UPointLightComponent>(WallActor, TEXT("WallGlow"));
		if (WallLight)
		{
			WallLight->SetupAttachment(WallActor->GetRootComponent());
			WallLight->SetMobility(EComponentMobility::Movable);
			WallLight->SetRelativeLocation(FVector(0, 0, TrailHeight / 2.0f));
			WallLight->SetIntensity(20000.0f);
			WallLight->SetAttenuationRadius(250.0f);
			WallLight->SetLightColor(FColor(
				FMath::Clamp(int32(CycleColor.R * 255), 0, 255),
				FMath::Clamp(int32(CycleColor.G * 255), 0, 255),
				FMath::Clamp(int32(CycleColor.B * 255), 0, 255)
			));
			WallLight->RegisterComponent();
		}

		WallActors.Add(WallActor);
		WallCount++;

		UE_LOG(LogTemp, Warning, TEXT("Wall %d finalized: Length=%.0f"), WallCount, Length);
	}

	return WallActor;
}

void AArmaCyclePawn::OnBrakePressed()
{
	// If dead, respawn on space press
	if (!bIsAlive)
	{
		Respawn();
		return;
	}
	
	bIsBraking = true;
	MoveSpeed = FMath::Max(200.0f, MoveSpeed * 0.5f);
}

void AArmaCyclePawn::OnBrakeReleased()
{
	if (!bIsAlive) return;
	
	bIsBraking = false;
	MoveSpeed = BaseSpeed;
}

void AArmaCyclePawn::OnMouseX(float Value)
{
	if (FMath::Abs(Value) > 0.01f)
	{
		CameraYawOffset += Value * 2.0f;
		// Allow full 360 degree rotation - no clamping, no auto-reset
	}
	// No else - camera stays where you leave it
}

void AArmaCyclePawn::OnMouseY(float Value)
{
	if (FMath::Abs(Value) > 0.01f)
	{
		CameraPitch += Value * 2.0f;
		// Allow looking from straight down (-89) to almost straight up (+60)
		CameraPitch = FMath::Clamp(CameraPitch, -89.0f, 60.0f);
	}
}

void AArmaCyclePawn::OnZoomIn()
{
	TargetArmLength = FMath::Max(200.0f, TargetArmLength - 150.0f);
	UE_LOG(LogTemp, Display, TEXT("Zoom In: TargetArmLength = %.0f"), TargetArmLength);
}

void AArmaCyclePawn::OnZoomOut()
{
	TargetArmLength = FMath::Min(3000.0f, TargetArmLength + 150.0f);
	UE_LOG(LogTemp, Display, TEXT("Zoom Out: TargetArmLength = %.0f"), TargetArmLength);
}

// ========== Physics Functions ==========

bool AArmaCyclePawn::IsVulnerable() const
{
	if (!bIsAlive) return false;
	
	float CurrentTime = GetWorld()->GetTimeSeconds();
	return (CurrentTime - SpawnTime) > SpawnInvulnerabilityTime;
}

float AArmaCyclePawn::GetDistanceToNearestWall(FVector Direction) const
{
	UWorld* World = GetWorld();
	if (!World) return 9999.0f;
	
	FVector Start = GetActorLocation();
	FVector End = Start + Direction.GetSafeNormal() * 1000.0f; // Cast 1000 units
	
	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	
	// Trace against all wall actors
	float MinDist = 9999.0f;
	
	for (AActor* WallActor : WallActors)
	{
		if (!WallActor) continue;
		
		// Use the wall's actual location for distance calculation
		FVector WallLocation = WallActor->GetActorLocation();
		FVector ToWall = WallLocation - Start;
		
		// Project onto our direction
		float ForwardDist = FVector::DotProduct(ToWall, Direction.GetSafeNormal());
		
		if (ForwardDist > 0 && ForwardDist < MinDist)
		{
			// Check if we're actually heading toward this wall
			FVector LateralOffset = ToWall - Direction.GetSafeNormal() * ForwardDist;
			if (LateralOffset.Size() < 100.0f) // Within 100 units laterally
			{
				MinDist = ForwardDist;
			}
		}
	}
	
	// Also check current wall actor
	if (CurrentWallActor)
	{
		FVector WallLocation = CurrentWallActor->GetActorLocation();
		FVector ToWall = WallLocation - Start;
		float ForwardDist = FVector::DotProduct(ToWall, Direction.GetSafeNormal());
		
		if (ForwardDist > 50.0f && ForwardDist < MinDist) // Ignore if too close (it's behind us)
		{
			FVector LateralOffset = ToWall - Direction.GetSafeNormal() * ForwardDist;
			if (LateralOffset.Size() < 100.0f)
			{
				MinDist = ForwardDist;
			}
		}
	}
	
	return MinDist;
}

void AArmaCyclePawn::UpdateRubber(float DeltaTime)
{
	// Rubber regeneration when not grinding
	// Armagetron: sg_rubberCycleTime = 10 seconds to fully regenerate
	const float RubberRegenTime = 10.0f;
	
	if (!bIsGrinding)
	{
		// Regenerate rubber when not using it
		float RegenAmount = (MaxRubber / RubberRegenTime) * DeltaTime;
		CurrentRubber = FMath::Min(MaxRubber, CurrentRubber + RegenAmount);
	}
	
	// Clamp rubber
	CurrentRubber = FMath::Clamp(CurrentRubber, 0.0f, MaxRubber);
}

void AArmaCyclePawn::UpdateWallDecay()
{
	// ========== WALL LENGTH DECAY (Armagetron WALLS_LENGTH) ==========
	// When total wall length exceeds max, remove oldest segments
	// This is how Armagetron makes walls "fall" after a certain distance
	
	// First, calculate total wall length
	TotalWallLength = 0.0f;
	for (const FWallSegment& Seg : WallSegments)
	{
		float SegLength = (Seg.End - Seg.Start).Size();
		TotalWallLength += SegLength;
	}
	
	// Add current wall segment length
	if (CurrentWallActor)
	{
		FVector CurrentPos = GetActorLocation();
		float CurrentSegLength = (FVector2D(CurrentPos.X, CurrentPos.Y) - FVector2D(CurrentWallStart.X, CurrentWallStart.Y)).Size();
		TotalWallLength += CurrentSegLength;
	}
	
	// If max length is set and we exceed it, remove oldest walls
	if (MaxWallsLength > 0.0f && TotalWallLength > MaxWallsLength)
	{
		float ExcessLength = TotalWallLength - MaxWallsLength;
		
		// Remove walls from the beginning (oldest first)
		while (ExcessLength > 0.0f && WallSegments.Num() > 0)
		{
			FWallSegment& OldestWall = WallSegments[0];
			float WallLength = (OldestWall.End - OldestWall.Start).Size();
			
			if (WallLength <= ExcessLength)
			{
				// Remove entire wall segment
				if (OldestWall.Actor && IsValid(OldestWall.Actor))
				{
					OldestWall.Actor->Destroy();
				}
				
				// Also remove from global registry
				if (UArmaWallRegistry* WallRegistry = UArmaWallRegistry::Get(GetWorld()))
				{
					// Find and remove this wall from registry
					const TArray<FArmaRegisteredWall>& AllWalls = WallRegistry->GetAllWalls();
					for (int32 i = 0; i < AllWalls.Num(); i++)
					{
						const FArmaRegisteredWall& RegWall = AllWalls[i];
						if (RegWall.OwnerActor == this && 
							FVector2D::DistSquared(RegWall.Start, OldestWall.Start) < 1.0f &&
							FVector2D::DistSquared(RegWall.End, OldestWall.End) < 1.0f)
						{
							WallRegistry->RemoveWall(RegWall.WallID);
							break;
						}
					}
				}
				
				ExcessLength -= WallLength;
				WallSegments.RemoveAt(0);
				WallCount--;
				
				UE_LOG(LogTemp, Display, TEXT("Wall decay: Removed oldest segment (Length=%.0f, Excess=%.0f)"), 
					WallLength, ExcessLength);
			}
			else
			{
				// Partial removal - shrink the wall (for smoother decay)
				// For now just break - full segment removal is simpler
				break;
			}
		}
		
		TotalWallLength = FMath::Max(0.0f, TotalWallLength - (TotalWallLength - MaxWallsLength));
	}
}

void AArmaCyclePawn::UpdateWallAcceleration(float DeltaTime)
{
	// ========== ARMAGETRON-STYLE WALL ACCELERATION ==========
	// Cast rays to LEFT and RIGHT (perpendicular to travel direction)
	// Acceleration = accel * (1/(dist+offset) - 1/(nearDist+offset))
	// NOTE: Only CYCLE walls provide acceleration, not RIM walls!
	
	FVector2D MyPos2D(GetActorLocation().X, GetActorLocation().Y);
	FVector2D MyDir2D(MoveDirection.X, MoveDirection.Y);
	FVector2D LeftDir2D(-MyDir2D.Y, MyDir2D.X);   // 90 degrees left
	FVector2D RightDir2D(MyDir2D.Y, -MyDir2D.X);  // 90 degrees right
	
	// Use member variables configured in header
	const float NearCycle = WallAccelDistance;     // sg_nearCycle - walls within this distance contribute
	const float AccelOffset = WallAccelOffset;     // sg_accelerationCycleOffs
	const float AccelBase = WallAcceleration;      // sg_accelerationCycle base value
	const float WallGracePeriod = 0.3f;
	float CurrentTime = GetWorld()->GetTimeSeconds();
	
	// Find closest wall to left and right using GLOBAL registry
	float LeftDist = NearCycle + 1.0f;
	float RightDist = NearCycle + 1.0f;
	
	UArmaWallRegistry* WallRegistry = UArmaWallRegistry::Get(GetWorld());
	if (WallRegistry)
	{
		const TArray<FArmaRegisteredWall>& AllWalls = WallRegistry->GetAllWalls();
		for (const FArmaRegisteredWall& Wall : AllWalls)
		{
			// Skip rim walls - they don't provide acceleration in Armagetron
			if (Wall.WallType == EArmaWallType::Rim)
			{
				continue;
			}
			
			// Skip very recent own walls
			if (Wall.OwnerActor == this && (CurrentTime - Wall.CreationTime) < WallGracePeriod)
			{
				continue;
			}
			
			float Dist = DistanceToLineSegment2D(MyPos2D, Wall.Start, Wall.End);
			
			if (Dist < NearCycle)
			{
				// Determine if wall is to our left or right
				FVector2D ToWall = ((Wall.Start + Wall.End) * 0.5f - MyPos2D).GetSafeNormal();
				float LeftDot = FVector2D::DotProduct(LeftDir2D, ToWall);
				float RightDot = FVector2D::DotProduct(RightDir2D, ToWall);
				
				if (LeftDot > 0.3f && Dist < LeftDist)
				{
					LeftDist = Dist;
				}
				if (RightDot > 0.3f && Dist < RightDist)
				{
					RightDist = Dist;
				}
			}
		}
	}
	
	// Calculate acceleration using Armagetron formula
	float TotalAcceleration = 0.0f;
	
	if (LeftDist < NearCycle)
	{
		// Inverse distance formula: accel * (1/(dist+offset) - 1/(nearDist+offset))
		float Factor = (1.0f / (LeftDist + AccelOffset)) - (1.0f / (NearCycle + AccelOffset));
		TotalAcceleration += AccelBase * Factor;
	}
	
	if (RightDist < NearCycle)
	{
		float Factor = (1.0f / (RightDist + AccelOffset)) - (1.0f / (NearCycle + AccelOffset));
		TotalAcceleration += AccelBase * Factor;
	}
	
	// Slingshot bonus when walls on both sides
	if (LeftDist < NearCycle && RightDist < NearCycle)
	{
		TotalAcceleration *= SlingshotMultiplier;
	}
	
	// Apply acceleration (only when not grinding into a wall ahead)
	if (TotalAcceleration > 0 && !bIsGrinding)
	{
		MoveSpeed = FMath::Min(MaxSpeed, MoveSpeed + TotalAcceleration * DeltaTime);
	}
	
	// Debug visualization
	if (bDebugDrawEnabled)
	{
		UWorld* World = GetWorld();
		if (World)
		{
			FVector Start = GetActorLocation();
			FVector LeftEnd = Start + FVector(LeftDir2D.X, LeftDir2D.Y, 0) * LeftDist;
			FVector RightEnd = Start + FVector(RightDir2D.X, RightDir2D.Y, 0) * RightDist;
			
			DrawDebugLine(World, Start, LeftEnd, LeftDist < NearCycle ? FColor::Green : FColor::White, false, -1.0f, 0, 2.0f);
			DrawDebugLine(World, Start, RightEnd, RightDist < NearCycle ? FColor::Blue : FColor::White, false, -1.0f, 0, 2.0f);
			
		}
	}
}

bool AArmaCyclePawn::CheckWallCollision(const FVector& DesiredMove, FVector& OutNewLocation)
{
	UWorld* World = GetWorld();
	if (!World) return false;
	
	// Check if we're about to hit a wall with this move
	FVector Start = GetActorLocation();
	FVector End = OutNewLocation + MoveDirection * 20.0f; // A bit past where we want to go
	
	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	// Ignore our current trail wall (it's behind us)
	if (CurrentWallActor) QueryParams.AddIgnoredActor(CurrentWallActor);
	
	// Use object type query to find WorldStatic objects (walls)
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic); // Also check dynamic objects
	
	bool bHitWall = World->LineTraceSingleByObjectType(HitResult, Start, End, ObjectParams, QueryParams);
	
	if (bDebugDrawEnabled && bHitWall)
	{
		UE_LOG(LogTemp, Warning, TEXT("COLLISION: Hit %s at dist %.1f"), 
			*HitResult.GetActor()->GetName(), HitResult.Distance);
		DrawDebugSphere(World, HitResult.ImpactPoint, 20.0f, 8, FColor::Red, false, 1.0f);
	}
	
	if (bHitWall && HitResult.GetActor())
	{
		// We're going to hit a wall!
		if (CurrentRubber > 0.0f)
		{
			// Rubber absorbs the impact - stop just before the wall
			FVector SafePos = HitResult.ImpactPoint - MoveDirection * 25.0f;
			SafePos.Z = Start.Z;
			OutNewLocation = SafePos;
			
			// Set our position to the safe spot
			SetActorLocation(SafePos);
			
			// Slow down significantly
			MoveSpeed = FMath::Max(50.0f, MoveSpeed * 0.3f);
			
			// Spawn sparks at impact point
			SpawnSpark(HitResult.ImpactPoint, HitResult.ImpactNormal);
			
			return true; // Movement blocked but handled by rubber
		}
		else
		{
			// No rubber left - die!
			if (IsVulnerable())
			{
				Die();
			}
			return true; // Movement blocked, player dead
		}
	}
	
	return false; // No collision
}

void AArmaCyclePawn::Die()
{
	if (!bIsAlive) return;
	
	UE_LOG(LogTemp, Error, TEXT("CYCLE DIED! Round %d, Deaths: %d"), CurrentRound, DeathCount + 1);
	
	bIsAlive = false;
	DeathCount++;
	
	// Stop movement
	MoveSpeed = 0.0f;
	
	// Hide cycle mesh
	if (CycleMesh)
	{
		CycleMesh->SetVisibility(false);
	}
	
	// Finalize current wall with collision
	if (CurrentWallActor)
	{
		UStaticMeshComponent* WallMesh = CurrentWallActor->GetStaticMeshComponent();
		if (WallMesh)
		{
			WallMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			WallMesh->SetCollisionObjectType(ECC_WorldDynamic);
			WallMesh->SetCollisionResponseToAllChannels(ECR_Block);
			WallMesh->SetGenerateOverlapEvents(true);
			WallMesh->SetNotifyRigidBodyCollision(true);
		}
		WallActors.Add(CurrentWallActor);
		
		// Store 2D segment with timestamp
		FVector CurrentPos = GetActorLocation();
		FVector2D SegStart(CurrentWallStart.X, CurrentWallStart.Y);
		FVector2D SegEnd(CurrentPos.X, CurrentPos.Y);
		float WallTime = GetWorld()->GetTimeSeconds();
		WallSegments.Add(FWallSegment(SegStart, SegEnd, CurrentWallActor, WallTime));
		WallCount++;
		CurrentWallActor = nullptr;
	}
	
	// Spawn explosion effect (simple flash for now)
	if (CycleGlowLight)
	{
		CycleGlowLight->SetIntensity(500000.0f); // Bright flash
		CycleGlowLight->SetLightColor(FColor::Red);
	}
}

void AArmaCyclePawn::Respawn()
{
	UE_LOG(LogTemp, Warning, TEXT("RESPAWNING! Starting Round %d"), CurrentRound + 1);
	
	CurrentRound++;
	bIsAlive = true;
	
	// Clear all walls
	ClearAllWalls();
	
	// Validate SpawnLocation is inside arena
	const float SafeBoundary = 4500.0f;  // Spawn well inside arena
	FVector SafeSpawn = SpawnLocation;
	SafeSpawn.X = FMath::Clamp(SafeSpawn.X, -SafeBoundary, SafeBoundary);
	SafeSpawn.Y = FMath::Clamp(SafeSpawn.Y, -SafeBoundary, SafeBoundary);
	
	// If spawn was way outside, use origin
	if (FMath::Abs(SpawnLocation.X) > 10000.0f || FMath::Abs(SpawnLocation.Y) > 10000.0f)
	{
		UE_LOG(LogTemp, Error, TEXT("SpawnLocation was corrupted! (%.1f, %.1f) - resetting to origin"), 
			SpawnLocation.X, SpawnLocation.Y);
		SafeSpawn = FVector(0, 0, 92.0f);
	}
	
	// Reset position and direction
	SetActorLocation(SafeSpawn);
	MoveDirection = SpawnDirection;
	TargetPawnYaw = MoveDirection.Rotation().Yaw;
	CurrentPawnYaw = TargetPawnYaw;
	SetActorRotation(FRotator(0, CurrentPawnYaw, 0));
	
	// Reset physics
	CurrentRubber = MaxRubber;
	MoveSpeed = BaseSpeed;
	bIsGrinding = false;
	DistanceToWall = 9999.0f;
	
	// Set spawn time for invulnerability
	SpawnTime = GetWorld()->GetTimeSeconds();
	
	// Allow immediate first turn after respawn (set LastTurnTime in the past)
	LastTurnTime = SpawnTime - TurnDelay;
	
	// Clear any pending turns from before death
	PendingTurns.Empty();
	
	// Show cycle mesh
	if (CycleMesh)
	{
		CycleMesh->SetVisibility(true);
	}
	
	// Reset glow light
	if (CycleGlowLight)
	{
		CycleGlowLight->SetIntensity(100000.0f);
		CycleGlowLight->SetLightColor(FColor(
			FMath::Clamp(int32(CycleColor.R * 255), 0, 255),
			FMath::Clamp(int32(CycleColor.G * 255), 0, 255),
			FMath::Clamp(int32(CycleColor.B * 255), 0, 255)
		));
	}
	
	// Start new wall segment
	StartNewWallSegment();
}

void AArmaCyclePawn::ClearAllWalls()
{
	// Remove walls from global registry (this also destroys visual actors)
	if (UArmaWallRegistry* WallRegistry = UArmaWallRegistry::Get(GetWorld()))
	{
		WallRegistry->RemoveWallsByOwner(this);
	}
	
	// Also clean up local arrays
	WallActors.Empty();
	WallSegments.Empty();
	WallCount = 0;
	
	// Destroy current wall actor
	if (CurrentWallActor)
	{
		CurrentWallActor->Destroy();
		CurrentWallActor = nullptr;
		CurrentWallID = 0;
	}
}

void AArmaCyclePawn::SpawnSpark(FVector Location, FVector Normal)
{
	UWorld* World = GetWorld();
	if (!World) return;
	
	// Simple spark effect: spawn a small glowing actor that fades quickly
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	
	AStaticMeshActor* SparkActor = World->SpawnActor<AStaticMeshActor>(
		AStaticMeshActor::StaticClass(), Location, FRotator::ZeroRotator, SpawnParams);
	
	if (SparkActor)
	{
		UStaticMeshComponent* SparkMesh = SparkActor->GetStaticMeshComponent();
		SparkMesh->SetMobility(EComponentMobility::Movable);
		
		UStaticMesh* SphereMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
		if (SphereMesh)
		{
			SparkMesh->SetStaticMesh(SphereMesh);
		}
		
		// Small spark
		SparkActor->SetActorScale3D(FVector(0.05f, 0.05f, 0.05f));
		
		// Bright emissive material
		UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		if (BaseMat)
		{
			UMaterialInstanceDynamic* SparkMat = UMaterialInstanceDynamic::Create(BaseMat, SparkActor);
			// Bright cyan/white
			SparkMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(5.0f, 10.0f, 10.0f, 1.0f));
			SparkMesh->SetMaterial(0, SparkMat);
		}
		
		SparkMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		
		// Set lifespan - spark disappears quickly
		SparkActor->SetLifeSpan(0.1f);
		
		// Add random velocity (simulate particle physics)
		// Note: For proper physics, we'd use a physics-enabled actor
		// For now, the spark just appears and fades
	}
}

void AArmaCyclePawn::UpdateInvulnerabilityBlink()
{
	if (IsVulnerable()) return;
	
	// Blink the cycle mesh during invulnerability
	float TimeSinceSpawn = GetWorld()->GetTimeSeconds() - SpawnTime;
	float BlinkRate = 10.0f; // Blinks per second
	bool bVisible = FMath::Fmod(TimeSinceSpawn * BlinkRate, 1.0f) > 0.5f;
	
	if (CycleMesh)
	{
		CycleMesh->SetVisibility(bVisible);
	}
}

float AArmaCyclePawn::DistanceToLineSegment2D(FVector2D Point, FVector2D LineStart, FVector2D LineEnd) const
{
	// Vector from line start to end
	FVector2D Line = LineEnd - LineStart;
	float LineLengthSq = Line.SizeSquared();
	
	if (LineLengthSq < 0.0001f)
	{
		// Line is basically a point
		return FVector2D::Distance(Point, LineStart);
	}
	
	// Project point onto line, clamped to segment
	float T = FMath::Clamp(FVector2D::DotProduct(Point - LineStart, Line) / LineLengthSq, 0.0f, 1.0f);
	FVector2D Projection = LineStart + T * Line;
	
	return FVector2D::Distance(Point, Projection);
}

// ========== Debug Functions ==========

void AArmaCyclePawn::DebugNextVar()
{
	const int32 NumVars = 7;  // Now includes WallsLength
	DebugSelectedVar = (DebugSelectedVar + 1) % NumVars;
	UE_LOG(LogTemp, Warning, TEXT("Debug: Selected variable %d"), DebugSelectedVar);
}

void AArmaCyclePawn::DebugPrevVar()
{
	const int32 NumVars = 7;  // Now includes WallsLength
	DebugSelectedVar = (DebugSelectedVar - 1 + NumVars) % NumVars;
	UE_LOG(LogTemp, Warning, TEXT("Debug: Selected variable %d"), DebugSelectedVar);
}

void AArmaCyclePawn::DebugIncreaseVar()
{
	switch (DebugSelectedVar)
	{
	case 0: MoveSpeed = FMath::Clamp(MoveSpeed + 100.0f, 100.0f, 5000.0f); break;
	case 1: MaxSpeed = FMath::Clamp(MaxSpeed + 100.0f, 500.0f, 10000.0f); break;
	case 2: MaxRubber = FMath::Clamp(MaxRubber + 10.0f, 10.0f, 500.0f); CurrentRubber = FMath::Min(CurrentRubber, MaxRubber); break;
	case 3: RubberDecayRate = FMath::Clamp(RubberDecayRate + 10.0f, 10.0f, 200.0f); break;
	case 4: WallAcceleration = FMath::Clamp(WallAcceleration + 5000.0f, 0.0f, 100000.0f); break;
	case 5: WallAccelDistance = FMath::Clamp(WallAccelDistance + 50.0f, 50.0f, 1000.0f); break;
	case 6: MaxWallsLength = (MaxWallsLength < 0) ? 5000.0f : FMath::Clamp(MaxWallsLength + 1000.0f, 1000.0f, 50000.0f); break;
	}
}

void AArmaCyclePawn::DebugDecreaseVar()
{
	switch (DebugSelectedVar)
	{
	case 0: MoveSpeed = FMath::Clamp(MoveSpeed - 100.0f, 100.0f, 5000.0f); break;
	case 1: MaxSpeed = FMath::Clamp(MaxSpeed - 100.0f, 500.0f, 10000.0f); break;
	case 2: MaxRubber = FMath::Clamp(MaxRubber - 10.0f, 10.0f, 500.0f); CurrentRubber = FMath::Min(CurrentRubber, MaxRubber); break;
	case 3: RubberDecayRate = FMath::Clamp(RubberDecayRate - 10.0f, 10.0f, 200.0f); break;
	case 4: WallAcceleration = FMath::Clamp(WallAcceleration - 5000.0f, 0.0f, 100000.0f); break;
	case 5: WallAccelDistance = FMath::Clamp(WallAccelDistance - 50.0f, 50.0f, 1000.0f); break;
	case 6: 
		if (MaxWallsLength > 1000.0f) 
			MaxWallsLength -= 1000.0f;
		else 
			MaxWallsLength = -1.0f;  // Go to infinite
		break;
	}
}

void AArmaCyclePawn::DebugToggleDraw()
{
	bDebugDrawEnabled = !bDebugDrawEnabled;
	UE_LOG(LogTemp, Warning, TEXT("Debug draw: %s"), bDebugDrawEnabled ? TEXT("ON") : TEXT("OFF"));
}

void AArmaCyclePawn::DrawDebugRays()
{
	if (!bDebugDrawEnabled) return;
	
	UWorld* World = GetWorld();
	if (!World) return;
	
	FVector Start = GetActorLocation();
	
	// Forward ray (wall detection) - shows actual distance to wall ahead
	float ForwardLookDist = 500.0f;
	FVector ForwardEnd = Start + MoveDirection * FMath::Min(DistanceToWall, ForwardLookDist);
	FColor ForwardColor = (DistanceToWall < 100.0f) ? FColor::Red : 
						  (DistanceToWall < 250.0f) ? FColor::Orange : FColor::White;
	DrawDebugLine(World, Start, ForwardEnd, ForwardColor, false, -1.0f, 0, 4.0f);
	if (DistanceToWall < ForwardLookDist)
	{
		// Draw sphere at wall hit point
		DrawDebugSphere(World, ForwardEnd, 15.0f, 8, FColor::Red, false, -1.0f, 0, 3.0f);
	}
	
	// Draw all wall segments as 2D lines (extruded to 3D)
	float CurrentTime = World->GetTimeSeconds();
	for (int32 i = 0; i < WallSegments.Num(); i++)
	{
		const FWallSegment& Wall = WallSegments[i];
		float Age = CurrentTime - Wall.CreationTime;
		FColor WallColor = (Age < 0.5f) ? FColor::Yellow : FColor::Purple;
		
		FVector WallStart3D(Wall.Start.X, Wall.Start.Y, GetActorLocation().Z);
		FVector WallEnd3D(Wall.End.X, Wall.End.Y, GetActorLocation().Z);
		DrawDebugLine(World, WallStart3D, WallEnd3D, WallColor, false, -1.0f, 0, 3.0f);
		
		// Show segment number
		FVector WallCenter = (WallStart3D + WallEnd3D) * 0.5f;
		DrawDebugString(World, WallCenter + FVector(0, 0, 50), FString::Printf(TEXT("%d"), i), nullptr, WallColor, 0.0f, true);
	}
	
	// Current wall (being created) - YELLOW DASHED
	if (CurrentWallActor)
	{
		FVector WallCenter = CurrentWallActor->GetActorLocation();
		FVector WallExtent = CurrentWallActor->GetComponentsBoundingBox().GetExtent();
		DrawDebugBox(World, WallCenter, WallExtent, FColor::Yellow, false, -1.0f, 0, 2.0f);
	}
	
	// On-screen status
	GEngine->AddOnScreenDebugMessage(100, 0.0f, ForwardColor,
		FString::Printf(TEXT(">>> FORWARD: %.0f units | Segments: %d | Grinding: %s <<<"),
			DistanceToWall, WallSegments.Num(), bIsGrinding ? TEXT("YES") : TEXT("NO")));
}

void AArmaCyclePawn::DrawDebugSliders()
{
	if (!GEngine) return;
	
	// Variable names - now includes wall length
	const TCHAR* VarNames[] = {
		TEXT("Speed"),
		TEXT("MaxSpeed"),
		TEXT("MaxRubber"),
		TEXT("RubberDecay"),
		TEXT("WallAccel"),
		TEXT("AccelDist"),
		TEXT("WallsLength")
	};
	
	float VarValues[] = {
		MoveSpeed,
		MaxSpeed,
		MaxRubber,
		RubberDecayRate,
		WallAcceleration,
		WallAccelDistance,
		MaxWallsLength
	};
	
	float VarMaxs[] = {
		5000.0f,
		10000.0f,
		500.0f,
		200.0f,
		100000.0f,
		1000.0f,
		50000.0f   // Max walls length slider max
	};
	
	const int32 NumVars = 7;
	
	GEngine->AddOnScreenDebugMessage(10, 0.0f, FColor::Orange, 
		TEXT("=== DEBUG (Tab/Q/E: Select, Up/Down: Adjust, F1: Toggle Rays) ==="));
	
	for (int32 i = 0; i < NumVars; i++)
	{
		bool bSelected = (i == DebugSelectedVar);
		FColor Color = bSelected ? FColor::Yellow : FColor::White;
		
		// Special display for walls length (-1 means infinite)
		float DisplayValue = VarValues[i];
		FString ValueStr;
		if (i == 6 && DisplayValue < 0)
		{
			ValueStr = TEXT("INFINITE");
		}
		else
		{
			ValueStr = FString::Printf(TEXT("%.0f"), DisplayValue);
		}
		
		// Create visual slider bar
		float BarValue = (DisplayValue < 0) ? 0.0f : DisplayValue;
		int32 BarFill = FMath::RoundToInt((BarValue / VarMaxs[i]) * 20);
		FString Bar = TEXT("[");
		for (int32 j = 0; j < 20; j++)
		{
			Bar += (j < BarFill) ? TEXT("#") : TEXT("-");
		}
		Bar += TEXT("]");
		
		FString Prefix = bSelected ? TEXT(">>> ") : TEXT("    ");
		GEngine->AddOnScreenDebugMessage(11 + i, 0.0f, Color,
			FString::Printf(TEXT("%s%s: %s %s"), *Prefix, VarNames[i], *Bar, *ValueStr));
	}
	
	// Show wall length info
	GEngine->AddOnScreenDebugMessage(19, 0.0f, FColor::Cyan,
		FString::Printf(TEXT("Total Wall Length: %.0f / %s"), TotalWallLength, 
			MaxWallsLength > 0 ? *FString::Printf(TEXT("%.0f"), MaxWallsLength) : TEXT("INF")));
	
	// Show ray hit info
	GEngine->AddOnScreenDebugMessage(20, 0.0f, bIsGrinding ? FColor::Red : FColor::Green,
		FString::Printf(TEXT("Forward Wall: %.0f | Grinding: %s"), DistanceToWall, bIsGrinding ? TEXT("YES") : TEXT("NO")));
}

// ========== Collision Callbacks ==========

void AArmaCyclePawn::OnWallHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, 
	UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (!bIsAlive) return;
	if (OtherActor == this) return;
	if (OtherActor == CurrentWallActor) return; // Ignore our current trail
	
	UE_LOG(LogTemp, Error, TEXT("WALL HIT! Actor: %s, Rubber: %.1f"), 
		*OtherActor->GetName(), CurrentRubber);
	
	// Check if it's a wall (our trail walls are AStaticMeshActor)
	if (CurrentRubber > 0.0f)
	{
		// Rubber absorbs the impact
		CurrentRubber -= 10.0f;
		MoveSpeed = FMath::Max(100.0f, MoveSpeed * 0.5f);
		
		// Push back from wall
		FVector PushDir = Hit.ImpactNormal;
		SetActorLocation(GetActorLocation() + PushDir * 30.0f);
		
		// Spawn spark
		SpawnSpark(Hit.ImpactPoint, Hit.ImpactNormal);
		
		bIsGrinding = true;
	}
	else
	{
		// No rubber - die!
		if (IsVulnerable())
		{
			Die();
		}
	}
}

void AArmaCyclePawn::OnWallOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!bIsAlive) return;
	if (OtherActor == this) return;
	if (OtherActor == CurrentWallActor) return;
	
	UE_LOG(LogTemp, Warning, TEXT("WALL OVERLAP! Actor: %s"), *OtherActor->GetName());
	
	// Treat overlap like a hit for safety
	if (CurrentRubber > 0.0f)
	{
		CurrentRubber -= 5.0f;
		bIsGrinding = true;
	}
	else if (IsVulnerable())
	{
		Die();
	}
}

// ========== ESC Menu ==========

void AArmaCyclePawn::ToggleMenu()
{
	bMenuOpen = !bMenuOpen;
	MenuSelection = 0;
	
	if (bMenuOpen)
	{
		// Pause the game
		UGameplayStatics::SetGamePaused(GetWorld(), true);
		
		// Show mouse cursor
		APlayerController* PC = Cast<APlayerController>(GetController());
		if (PC)
		{
			PC->bShowMouseCursor = true;
			PC->SetInputMode(FInputModeGameAndUI());
		}
	}
	else
	{
		ResumeGame();
	}
}

void AArmaCyclePawn::DrawMenu()
{
	if (!bMenuOpen || !GEngine) return;
	
	// Dark overlay hint
	GEngine->AddOnScreenDebugMessage(100, 0.0f, FColor::White, TEXT(""));
	GEngine->AddOnScreenDebugMessage(101, 0.0f, FColor::Cyan, TEXT("╔══════════════════════════════════════╗"));
	GEngine->AddOnScreenDebugMessage(102, 0.0f, FColor::Cyan, TEXT("║       ARMAGETRON UE5 - PAUSED        ║"));
	GEngine->AddOnScreenDebugMessage(103, 0.0f, FColor::Cyan, TEXT("╠══════════════════════════════════════╣"));
	
	const TCHAR* MenuItems[] = {
		TEXT("Resume Game"),
		TEXT("Server Browser"),
		TEXT("Settings"),
		TEXT("Quit to Desktop")
	};
	
	for (int32 i = 0; i < 4; i++)
	{
		bool bSelected = (i == MenuSelection);
		FColor Color = bSelected ? FColor::Yellow : FColor::White;
		FString Prefix = bSelected ? TEXT("║  >>  ") : TEXT("║      ");
		FString Suffix = bSelected ? TEXT("  <<  ║") : TEXT("       ║");
		
		// Pad to fixed width
		FString ItemText = FString(MenuItems[i]);
		while (ItemText.Len() < 24) ItemText += TEXT(" ");
		
		GEngine->AddOnScreenDebugMessage(104 + i, 0.0f, Color,
			FString::Printf(TEXT("%s%s%s"), *Prefix, *ItemText, *Suffix));
	}
	
	GEngine->AddOnScreenDebugMessage(108, 0.0f, FColor::Cyan, TEXT("╠══════════════════════════════════════╣"));
	GEngine->AddOnScreenDebugMessage(109, 0.0f, FColor::White, TEXT("║  W/S: Navigate | Enter: Select       ║"));
	GEngine->AddOnScreenDebugMessage(110, 0.0f, FColor::White, TEXT("║  ESC: Close Menu                      ║"));
	GEngine->AddOnScreenDebugMessage(111, 0.0f, FColor::Cyan, TEXT("╚══════════════════════════════════════╝"));
}

void AArmaCyclePawn::MenuUp()
{
	if (!bMenuOpen) return;
	MenuSelection = (MenuSelection - 1 + 4) % 4;
}

void AArmaCyclePawn::MenuDown()
{
	if (!bMenuOpen) return;
	MenuSelection = (MenuSelection + 1) % 4;
}

void AArmaCyclePawn::MenuSelect()
{
	if (!bMenuOpen) return;
	
	switch (MenuSelection)
	{
	case 0: ResumeGame(); break;
	case 1: OpenServerBrowser(); break;
	case 2: /* Settings - not implemented */ break;
	case 3: QuitToDesktop(); break;
	}
}

void AArmaCyclePawn::ResumeGame()
{
	bMenuOpen = false;
	UGameplayStatics::SetGamePaused(GetWorld(), false);
	
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC)
	{
		PC->bShowMouseCursor = false;
		PC->SetInputMode(FInputModeGameOnly());
	}
}

void AArmaCyclePawn::OpenServerBrowser()
{
	// TODO: Implement server browser with master server connection
	UE_LOG(LogTemp, Warning, TEXT("Server Browser - Coming Soon!"));
	
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(150, 5.0f, FColor::Yellow, 
			TEXT("Server Browser: Coming Soon! Master server integration pending."));
	}
}

void AArmaCyclePawn::QuitToDesktop()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC)
	{
		UKismetSystemLibrary::QuitGame(GetWorld(), PC, EQuitPreference::Quit, false);
	}
}

