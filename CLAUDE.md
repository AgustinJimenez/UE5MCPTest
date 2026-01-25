# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is an Unreal Engine 5.7 animation and locomotion showcase project. It is a **blueprint-only project** with no C++ source code.

## Development Commands

Open project in Unreal Editor:
```bash
open UETest1.uproject
```

Or via command line (macOS):
```bash
/Users/Shared/Epic\ Games/UE_5.7/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor "$(pwd)/UETest1.uproject"
```

## Architecture

### Plugin Dependencies

The project relies on these key plugin systems:
- **Animation:** AnimationWarping, AnimationLocomotionLibrary, MotionWarping, PoseSearch
- **AI/Behavior:** SmartObjects, Locomotor, GameplayInteractions, StateTree
- **Character:** RigLogic, LiveLink, LiveLinkControlRig
- **Rendering:** HairStrands (for MetaHuman grooms)

### Content Organization

- `/Content/Blueprints/` - Core game logic
  - `AnimNotifies/` - Foley event triggers (footsteps, lands, slides)
  - `Data/` - Enums (E_*), Structs (S_*), and curves for movement/animation
  - `MovementModes/` - Walking, Falling, Sliding locomotion modes
  - `Cameras/` - Camera director, rigs, modes, and styles
  - `SmartObjects/` - Interactive world objects with animation payloads
  - `AI/StateTree/` - NPC patrol and smart object interaction behaviors
- `/Content/Characters/` - Character skeletons (Echo, Paragon, UE5_Mannequins, MetaHumans)
- `/Content/Levels/` - DefaultLevel, LocomotorLevel, NPCLevel
- `/Content/Audio/Foley/` - Dynamic movement sounds

### Naming Conventions

| Prefix | Type |
|--------|------|
| BP_ | Blueprint |
| ABP_ | Animation Blueprint |
| AC_ | Actor Component |
| AIC_ | AI Controller |
| CR_ | Control Rig |
| E_ | Enum |
| S_ | Struct |
| ST_ | StateTree |
| GM_ | Game Mode |
| PC_ | Player Controller |
| BPI_ | Blueprint Interface |

### Key Data Structures

Movement system uses these interconnected structures:
- `E_Gait`, `E_MovementDirection`, `E_MovementMode`, `E_MovementState`, `E_RotationMode`, `E_Stance` - Movement enums
- `S_CharacterPropertiesForAnimation` - Animation state data
- `S_CharacterPropertiesForCamera` - Camera behavior data
- `S_CharacterPropertiesForTraversal` - Traversal system data
- `S_PlayerInputState`, `S_MoverCustomInputs` - Input handling

### Engine Configuration

Key settings in `/Config/`:
- Enhanced Input System enabled (EnhancedPlayerInput, EnhancedInputComponent)
- Lumen GI, Virtual Shadow Maps, Nanite enabled
- Gameplay Tags system with animation state tags and foley event tags
