# ArmagetronUE5

A faithful port of **Armagetron Advanced** to **Unreal Engine 5.7**, bringing the classic lightcycle combat game into a modern engine with enhanced graphics and physics while preserving the original gameplay mechanics.

## Overview

ArmagetronUE5 recreates the iconic lightcycle arena combat where players navigate a grid battlefield, leaving deadly walls behind them. The goal is to force opponents to crash into walls while avoiding them yourself. This project ports the core systems from the original C++ Armagetron codebase to Unreal Engine's architecture.

## Features

### Core Gameplay Systems
- **Lightcycle Physics** - Complete port of `gCycleMovement` including:
  - Grid-aligned movement with 4-directional turning
  - Speed, acceleration, and braking systems
  - "Rubber" mechanic for bouncing off walls
  - Turn delay and timing mechanics
  
- **Wall System** - Dynamic trail walls with:
  - Procedural mesh generation
  - Hole mechanics for explosions
  - Time-based wall decay
  - Collision detection and danger queries

- **Grid Collision System** - Port of `eGrid` with:
  - Half-edge data structure for efficient spatial queries
  - Face/edge/point topology
  - Ray casting against grid edges
  - Dynamic wall insertion

### AI System
- **AI Controller** - Port of `gAIPlayer` with multiple behavior states:
  - **Survive** - Basic survival mode, avoiding walls
  - **Trace** - Follow walls at a safe distance
  - **Path** - Navigate to specific targets
  - **Close Combat** - Aggressive opponent targeting
  - **Route** - Follow predefined waypoints

- **AI Sensing** - Three-ray sensor system (front, left, right)
- **Decision Making** - Emergency reactions and tactical positioning
- **Auto-respawn** - AI cycles automatically respawn after death

### Visual & Audio
- Procedural wall mesh generation with dynamic materials
- Niagara particle effects for cycle trails
- Dynamic material colors for teams/players
- Engine sound system

## Project Structure

```
ArmagetronUE5/
├── Content/
│   ├── Blueprints/       # Blueprint assets
│   ├── Maps/             # Game levels (TestArena, TestArma)
│   ├── Materials/        # Wall and cycle materials
│   ├── Meshes/           # Cycle and arena meshes
│   ├── Particles/        # Niagara trail effects
│   ├── Textures/         # Visual assets
│   └── UI/               # User interface
│
└── Source/ArmagetronUE5/
    ├── Core/             # Core type definitions
    │   ├── ArmaTypes.h/cpp       # FArmaCoord, FArmaColor, game settings
    │   └── ArmaGrid.h/cpp        # Grid system, arena, collision
    │
    ├── Game/             # Gameplay actors
    │   ├── ArmaCycle.h/cpp              # Main lightcycle pawn
    │   ├── ArmaCycleMovement.h/cpp      # Physics component
    │   ├── ArmaCyclePawn.h/cpp          # Base pawn class
    │   ├── ArmaWall.h/cpp               # Trail wall actor
    │   ├── ArmaWallRegistry.h/cpp       # Wall management
    │   └── ArmaTestGameMode.h/cpp       # Game mode with AI spawning
    │
    ├── AI/               # AI systems
    │   ├── ArmaAIController.h/cpp       # AI decision making
    │   ├── ArmaAICycle.h/cpp            # AI cycle implementation
    │   └── ArmaAICharacter.h/cpp        # AI personality/difficulty
    │
    ├── Debug/            # Debug utilities
    └── UI/               # UI components
```

## Key Classes

### Core Types (`Core/ArmaTypes.h`)
- **FArmaCoord** - 2D coordinate/vector (port of `eCoord`)
- **FArmaColor** - RGB color for cycles and walls
- **FArmaGameSettings** - Configurable game rules and physics
- **EArmaAIState** - AI behavior state enum

### Grid System (`Core/ArmaGrid.h`)
- **UArmaGridSubsystem** - World subsystem managing collision grid
- **FArmaGridPoint/HalfEdge/Face** - Half-edge mesh data structures
- **AArmaArena** - Arena actor with spawn points and boundaries
- **FArmaAxis** - Grid direction and winding system

### Gameplay (`Game/`)
- **AArmaCycle** - Complete lightcycle pawn with:
  - Wall building and collision
  - Death/kill system
  - Visual effects and animation
  - Camera positioning

- **UArmaCycleMovementComponent** - Physics simulation:
  - Speed and acceleration
  - Grid-aligned turning with delay
  - Rubber mechanics
  - Braking system
  - Destination interpolation

- **AArmaWall** - Trail wall with:
  - Procedural mesh generation
  - Segment tracking for holes
  - Time-based danger queries
  - Alpha-based position interpolation

### AI (`AI/`)
- **AArmaAIController** - AI brain with:
  - State machine (Survive/Trace/Path/Combat/Route)
  - Sensor-based perception
  - Emergency reactions
  - Target tracking

- **AArmaAICycle** - AI-controlled cycle:
  - Auto-respawn on death
  - Configurable IQ/difficulty
  - Think interval timing

## Building & Running

### Requirements
- **Unreal Engine 5.7** or later
- **Visual Studio 2022** (Windows)
- **C++ Development Tools**

### Setup
1. Clone or download this repository
2. Right-click `ArmagetronUE5.uproject` and select "Generate Visual Studio project files"
3. Open `ArmagetronUE5.sln` in Visual Studio
4. Build the solution (Development Editor configuration)
5. Launch the editor from Visual Studio or by opening the `.uproject` file

### Quick Start
1. Open one of the test maps: `Content/Maps/TestArena.umap`
2. Press **Play** to start
3. **WASD** or **Arrow Keys** to turn the cycle
4. **Space** to brake
5. Avoid walls and force AI opponents to crash

## Game Configuration

### Physics Constants (`ArmaPhysics` namespace)
```cpp
DefaultSpeed = 20.0f
DefaultRubberSpeed = 60.0f
DefaultTurnDelay = 0.2f
DefaultWallsLength = 300.0f
DefaultExplosionRadius = 4.0f
DefaultArenaSize = 500.0f
```

### AI Settings
- **NumAIs** - Number of AI opponents (default: 1)
- **AI_IQ** - Intelligence level 0-100 (default: 100)
- **AIThinkInterval** - Decision frequency (default: 0.1s)
- **SensorRange** - Vision distance (default: 500.0)

## Development Notes

### Original Armagetron Port Status
This is a faithful architectural port maintaining the original's design:
- ✅ Core grid system (`eGrid` → `UArmaGridSubsystem`)
- ✅ Cycle physics (`gCycleMovement` → `UArmaCycleMovementComponent`)
- ✅ Wall system (`gPlayerWall` → `AArmaWall`)
- ✅ AI framework (`gAIPlayer` → `AArmaAIController`)
- ✅ Basic game modes
- ⚠️ Networking (planned - original used `nNetObject`)
- ⚠️ Advanced game modes (zones, fortress, etc.)
- ⚠️ Full UI system

### Code Style
The codebase uses Unreal Engine's coding standards with original Armagetron variable names preserved where possible for easier cross-referencing. Original source files are referenced in header comments.

### Module Dependencies
```
Core, CoreUObject, Engine, InputCore, EnhancedInput
AIModule, NavigationSystem
Niagara, ProceduralMeshComponent
UMG, Slate, SlateCore
```

## Original Credits

Armagetron Advanced: https://www.armagetronad.org/
- Original game by Manuel Moos and the Armagetron development team
- Licensed under GPL v2+

This UE5 port maintains the spirit of the original while adapting to modern engine architecture.

## License

This project follows the original Armagetron Advanced GPL v2+ licensing.

## Future Roadmap

- [ ] Multiplayer networking
- [ ] Zone and fortress game modes
- [ ] Advanced AI personalities
- [ ] Menu system and HUD
- [ ] Server browser
- [ ] Custom arena editor
- [ ] Sound effects and music
- [ ] Particle effect polish
- [ ] Performance optimization

## Contributing

When contributing, please maintain consistency with:
- Original Armagetron architecture and naming conventions
- Unreal Engine coding standards
- Comment references to original source files

## Contact & Links

- Original Armagetron: https://www.armagetronad.org/
- Original Source: https://gitlab.com/armagetronad/armagetronad

---

*Built with Unreal Engine 5.7 | A tribute to the classic lightcycle combat game*
