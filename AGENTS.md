# AGENTS.md

This file provides guidance to AI coding assistants when working with code in this repository.

## Project Overview

This is an Unreal Engine 5.7 animation and locomotion showcase project with both Blueprints and C++ code.

## Development Commands

Open project in Unreal Editor:
```bash
open UETest1.uproject
```

Or via command line (macOS):
```bash
/Users/Shared/Epic\ Games/UE_5.7/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor "$(pwd)/UETest1.uproject"
```

Rebuild C++ modules (macOS):
```bash
/Users/Shared/Epic\ Games/UE_5.7/Engine/Build/BatchFiles/Mac/Build.sh -ModuleWithSuffix=UETest1,9050 UETest1Editor Mac Development -Project="$(pwd)/UETest1.uproject" "$(pwd)/UETest1.uproject" -architecture=arm64 -IgnoreJunk
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

## MCP Integration (AI Assistants)

This project includes ClaudeUnrealMCP, a custom MCP plugin for AI assistant integration with Unreal Engine.

### Plugin Location

`Plugins/ClaudeUnrealMCP/` - Custom MCP plugin with:
- UE5 TCP server (port 9877)
- Node.js MCP server for AI assistants

### Setup

1. Enable the ClaudeUnrealMCP plugin in Unreal Editor (Edit → Plugins → search "Claude")

2. Restart Unreal Editor - the TCP server starts automatically on port 9877

3. Install MCP server dependencies:
   ```bash
   cd Plugins/ClaudeUnrealMCP/MCPServer && npm install
   ```

4. Configure your AI assistant to use the MCP server at:
   ```
   /Users/agusj/repo/unreal_engine_5/UETest1/Plugins/ClaudeUnrealMCP/MCPServer/index.js
   ```

5. Restart your AI assistant

### Available MCP Commands

**Read Commands:**
- `ping` - Test connection to Unreal Engine
- `list_blueprints` - List all blueprints (optional path filter)
- `read_blueprint` - Get blueprint overview (parent class, counts)
- `read_variables` - Read blueprint variables
- `read_components` - Read component hierarchy
- `read_event_graph` - Read nodes and connections
- `list_actors` - List actors in current level

**Write Commands:**
- `add_component` - Add a component to a blueprint
- `set_component_property` - Set a property on a component
- `add_input_mapping` - Add key mapping to an input context
- `compile_blueprint` - Compile a blueprint
- `save_asset` - Save an asset to disk

## Workflow Notes (Current)
- We’re converting Blueprints to C++ in order from easiest to hardest (see `CURRENT_TASK.md`).
- New MCP tool added: `read_timelines` (reads timeline templates/tracks/keys). If it isn’t visible in `/mcp`, restart UE + Codex CLI so the MCP server reloads.
- When reparenting BPs to C++, avoid C++ component properties that collide with existing BP component names (e.g., `Spinner`, `Arrow`). Use runtime component lookup by name instead.
- For timeline curves created at runtime, mark the curve UPROPERTYs as `Transient` and create/bind them in `BeginPlay` to avoid “Illegal reference to private object” save errors.

## TODO

- [ ] Convert blueprints to C++ using the MCP to read blueprint logic and translate to equivalent C++ code
