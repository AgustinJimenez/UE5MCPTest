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

Or via command line (Windows):
```bash
"E:\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe" "E:\repo\unreal_engine\UE5MCPTest\UETest1.uproject"
```

Rebuild C++ modules (macOS):
```bash
/Users/Shared/Epic\ Games/UE_5.7/Engine/Build/BatchFiles/Mac/Build.sh -ModuleWithSuffix=UETest1,9050 UETest1Editor Mac Development -Project="$(pwd)/UETest1.uproject" "$(pwd)/UETest1.uproject" -architecture=arm64 -IgnoreJunk
```

Rebuild C++ modules (Windows):
```bash
powershell.exe -Command "& 'E:\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' UETest1Editor Win64 Development 'E:\repo\unreal_engine\UE5MCPTest\UETest1.uproject' -waitmutex"
```

Note: Unreal Engine 5.7 is installed at `E:\Epic Games\UE_5.7` on this system.

## MCP Server Setup

**IMPORTANT**: Before using the MCP server, you MUST install npm dependencies:

```bash
cd Plugins/ClaudeUnrealMCP/MCPServer
npm install
```

This installs `@modelcontextprotocol/sdk` which is required for the MCP server to run. Without this, the MCP server will fail with `ERR_MODULE_NOT_FOUND`.

The MCP server:
- Runs on TCP port 9877 when UE5 is open (C++ plugin auto-starts)
- Node.js bridge at `Plugins/ClaudeUnrealMCP/MCPServer/index.js` connects to it
- Must be configured in `~/.claude.json` (see CLAUDE.md)

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
- `read_event_graph_detailed` - Read event graph with pin default values
- `read_function_graphs` - Read function graphs with pagination support
- `read_timelines` - Read timeline templates, tracks, and keyframes
- `read_interface` - Read Blueprint Interface function signatures
- `read_user_defined_struct` - Read struct fields
- `read_user_defined_enum` - Read enum entries
- `list_actors` - List actors in current level

**Write Commands:**
- `add_component` - Add a component to a blueprint
- `set_component_property` - Set a property on a component
- `add_input_mapping` - Add key mapping to an input context
- `reparent_blueprint` - Change blueprint parent class
- `compile_blueprint` - Compile a blueprint and return detailed errors/warnings
- `save_asset` - Save a specific asset to disk
- `save_all` - Save all modified assets in the project
- `delete_interface_function` - Delete a function from a Blueprint Interface
- `delete_function_graph` - Delete a function graph from a Blueprint or Blueprint Function Library
- `refresh_nodes` - Refresh/reconstruct all nodes in a blueprint to fix stale pin errors
- `set_blueprint_compile_settings` - Modify blueprint compilation settings (thread-safe execution, etc.)
- `modify_function_metadata` - Modify function metadata flags (BlueprintThreadSafe, BlueprintPure)
- `capture_screenshot` - Capture screenshot of active viewport (saved to `Saved/AutoScreenshot.png`)

## Recent MCP Improvements

**Connection Management (Fixed)**
- Implemented one-request-per-connection pattern to prevent stale CLOSE_WAIT connections
- Added socket linger and no-delay settings for reliable connection cleanup
- Added 5-second timeout for receiving data to prevent hanging connections
- No more connection accumulation or manual editor restarts needed
- **MCP Reconnection**: If MCP connection drops during work, use `/mcp` command in Claude Code CLI to reconnect without restarting the entire session. This is faster than restarting Claude CLI.

**Error Reporting (Enhanced)**
- `compile_blueprint` now returns detailed error and warning messages
- Response includes:
  - `compiled`: boolean success status
  - `status`: Blueprint status (Error, UpToDate, etc.)
  - `errors`: Array of error messages with severity
  - `warnings`: Array of warning messages with severity
  - `error_count` / `warning_count`: Counts for quick overview

**Save Functionality (New)**
- `save_all` command saves all modified assets in the project
- Returns counts of saved/failed packages
- Lists any packages that failed to save
- Eliminates need to manually click Save in the editor

**Advanced Error Fixing (New - 2026-01-29)**
- `delete_function_graph` - Removes function graphs from Blueprint Function Libraries that conflict with C++ implementations
- `refresh_nodes` - Reconstructs all nodes in a blueprint to fix stale pin errors (WorldContext pins, removed pins, etc.)
- `break_orphaned_pins` - Aggressively removes orphaned pins and breaks their connections. Use when refresh_nodes doesn't fix orphaned pin errors. Added 2026-01-30 to fix stubborn "In use pin X no longer exists" errors.
- `delete_user_defined_struct` - Deletes blueprint struct assets when replacing with C++ versions. Cannot delete structs currently in use.
- `clear_event_graph` - **⚠️ DEPRECATED - DO NOT USE** - Clears event graph nodes but can corrupt blueprint files (save crash). Use manual cleanup in editor instead.
- `set_blueprint_compile_settings` - Modifies blueprint compilation options (construction script behavior, const class generation)
- `modify_function_metadata` - Changes function flags like BlueprintThreadSafe to fix thread-safety violations in animation blueprints
- `modify_struct_field` - **⚠️ INCOMPLETE/NOT WORKING FOR C++ STRUCTS** - Attempts to modify struct field types in user-defined structs. **Critical Limitations:**
  - **Cannot find C++ structs at runtime**: Even with correct FindPackage approach, C++ structs (USTRUCT) don't appear in UE5's reflection object system (TObjectIterator, FindObject, LoadObject) until they're actually instantiated somewhere in the running code. This is a fundamental UE5 limitation.
  - **Only handles simple struct fields**: Does not properly handle Map/Array/Set container value types. For Maps, the code modifies the wrong property (container itself rather than value type).
  - **Web research findings**: FindPackage(nullptr, TEXT("/Script/ModuleName")) + FindObject pattern is correct for C++ types, but structs still don't appear if not instantiated. See [Jonas Reich - Finding the Right Paths](https://jonasreich.de/blog/008-unreal-paths.html).
  - **Workaround**: All struct modifications involving C++ struct types must be done manually in the Unreal Editor. Open the struct asset, change field/value types, compile and save.

**UE5 Reflection System Research (2026-01-30)**

Investigation into why C++ structs can't be found via MCP tools revealed key insights about UE5's reflection system:

- **For compile-time known types**: Use `FStructName::StaticStruct()` - most efficient approach
- **For runtime name lookup**: Use `LoadObject<UScriptStruct>(nullptr, TEXT("/Script/ModuleName.StructName"))` or `FindObject<UScriptStruct>(Package, TEXT("StructName"))` where Package = `FindPackage(nullptr, TEXT("/Script/ModuleName"))`
- **StaticFindObject UE5.7 signature**: `StaticFindObject(UClass* Class, UObject* InOuter, FStringView Name, EFindObjectFlags Flags)`
- **/Script/ paths**: Contain type info for all C++ classes/structs, one package per code module
- **Fundamental limitation**: UScriptStruct objects for C++ USTRUCT types may not appear in TObjectIterator or FindObject until they're instantiated/used in running code

**Sources**: [UE4 Reflection Overview](https://ikrima.dev/ue4guide/engine-programming/uobject-reflection/uobject-reflection/), [Unreal Property System Wiki](https://unrealcommunity.wiki/revisions/6179da6f65f766208636d1eb), [Jonas Reich - Finding Paths](https://jonasreich.de/blog/008-unreal-paths.html), [UE5.7 StaticFindObject API](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/CoreUObject/StaticFindObject)

## Workflow Notes (Current)
- We're converting Blueprints to C++ in order from easiest to hardest (see `CURRENT_TASK.md`).
- New MCP tool added: `read_timelines` (reads timeline templates/tracks/keys). If it isn't visible in `/mcp`, restart UE + Codex CLI so the MCP server reloads.
- When reparenting BPs to C++, avoid C++ component properties that collide with existing BP component names (e.g., `Spinner`, `Arrow`). Use runtime component lookup by name instead.
- For timeline curves created at runtime, mark the curve UPROPERTYs as `Transient` and create/bind them in `BeginPlay` to avoid "Illegal reference to private object" save errors.
- **Known Issue - Save Crash (2026-01-29)**: MCP `save_asset` can crash the editor after reparent+compile operations (crash in MCPServer.cpp:1743 during UPackage::SavePackage). The save usually completes successfully before the crash. **Workaround: Always use `save_all` instead of `save_asset` after blueprint conversions** - this is more stable and avoids the crash.
- **CRITICAL - clear_event_graph Corrupts Blueprints (2026-01-30)**: The `clear_event_graph` command can corrupt blueprint files, making them unloadable ("The end of package tag is not valid"). **DO NOT USE THIS COMMAND**. If you need to remove event graph logic after C++ conversion, leave the blueprint nodes in place (they won't execute if C++ handles the logic) or manually delete them in the editor.
- **Orphaned Pin Errors (Fixed 2026-01-30)**: Orphaned pin errors ("In use pin X no longer exists") that persist after `refresh_nodes` can now be fixed with the new `break_orphaned_pins` command. This aggressively removes orphaned pins from blueprint nodes. Successfully used to fix SandboxCharacter_CMC and SandboxCharacter_Mover after AC_FoleyEvents signature changes.
- **Struct Type Mismatches (2026-01-30) - RESOLVED**: When a blueprint struct (S_*) is replaced with a C++ struct (FS_*), and the blueprint struct is used as a Map/Array/Set value type in another struct, the `modify_struct_field` MCP command **cannot** handle this automatically due to fundamental UE5 reflection limitations (C++ structs don't appear in object system until instantiated).
  - **Resolution**: Convert the containing struct to C++ as well. Example: S_LevelStyle contained Map<FName, S_GridMaterialParams>, which blocked changing to FS_GridMaterialParams. Solution: Created FS_LevelStyle in C++, then converted LevelVisuals blueprint to C++ to use FS_LevelStyle.
  - **Lesson**: Full C++ conversion (following the project goal) resolves these type mismatch blockers. Don't attempt partial conversions with blueprint/C++ type mixing.
- **Screenshot Capture (New - 2026-01-30)**: Added `capture_screenshot` MCP command to capture the active Unreal Editor viewport. Screenshots are saved to `ProjectDir/Saved/AutoScreenshot.png`. This enables AI assistants to visually see the editor viewport, level layout, and visual state for debugging and verification. Successfully tested with viewport capture showing level geometry and Game Animation Sample content.

## TODO

- [ ] Convert blueprints to C++ using the MCP to read blueprint logic and translate to equivalent C++ code
