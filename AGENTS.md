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

**Restarting Unreal Engine after MCP updates:**
- **IMPORTANT**: Always run `save_all` MCP command BEFORE restarting to save blueprint changes
- After modifying C++ code in the MCP plugin and recompiling, Unreal Engine must be restarted to load the new DLL
- macOS: `npm run restart:ue` (from project root)
- Windows: `npm run restart:ue:win` (from project root)
- Note: `/mcp` command in Claude Code only restarts the Node.js MCP server, not Unreal Engine itself

### MCP Server Extensibility

**IMPORTANT**: The MCP server is designed to be extended without limitations. If you encounter a limitation while working with the project, you should add new MCP tools to remove that limitation.

The goal of the MCP server is to provide unrestricted access to Unreal Engine functionality. To add a new tool:

1. Add the tool definition to `Plugins/ClaudeUnrealMCP/MCPServer/index.js` in the `tools` array
2. Add the command handler in `Plugins/ClaudeUnrealMCP/Source/ClaudeUnrealMCP/Private/MCPServer.cpp` (ProcessCommand)
3. Implement the handler function in `MCPServer.cpp`
4. Add the function declaration to `Plugins/ClaudeUnrealMCP/Source/ClaudeUnrealMCP/Public/MCPServer.h`
5. Recompile the plugin

Examples of custom tools added to the MCP server:
- `delete_function_graph` - Remove blueprint function graphs
- `refresh_nodes` - Reconstruct nodes to fix stale pin errors
- `break_orphaned_pins` - Aggressively remove orphaned pins
- `read_class_defaults` - Read Class Default Object properties including inherited properties
- `remove_error_nodes` - Automatically identify and remove nodes causing compilation errors (2026-01-31)
- `clear_animation_blueprint_tags` - Remove AnimBlueprintExtension_Tag objects to fix tag reference errors (2026-01-31)
- `clear_anim_graph` - Delete all AnimGraph nodes to rebuild from scratch (2026-01-31)
- **Sprint 1 - Blueprint Function Creation (2026-02-01):** `create_blueprint_function`, `add_function_input`, `add_function_output`, `rename_blueprint_function` - Programmatically build blueprint functions with parameters

Do not treat the MCP server as a black box with fixed capabilities - modify it as needed to accomplish your tasks.

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
- `remove_error_nodes` - Automatically identify and remove nodes causing compilation errors with optional execution flow rewiring
- `clear_animation_blueprint_tags` - Remove AnimBlueprintExtension_Tag objects from animation blueprints to fix 'cannot find referenced node with tag' errors (2026-01-31)
- `clear_anim_graph` - Delete all nodes from an animation blueprint's AnimGraph, leaving only the root output node (2026-01-31)

**Blueprint Function Creation Commands (Sprint 1 - 2026-02-01):**
- `create_blueprint_function` - Create a new function in a blueprint with optional metadata flags (is_pure, is_thread_safe, is_const)
- `add_function_input` - Add an input parameter to an existing blueprint function (supports int, float, bool, string, FVector, FRotator, FTransform, and custom types)
- `add_function_output` - Add an output parameter (return value) to an existing blueprint function
- `rename_blueprint_function` - Rename an existing blueprint function

**Level Actor Property Preservation Commands (Sprint 2 - 2026-02-01):**
- `read_actor_properties` - Read all EditAnywhere properties from a level actor instance. Returns JSON object with property name-value pairs. Use this to preserve actor configuration before blueprint reparenting.
- `set_actor_properties` - Set EditAnywhere properties on a level actor instance. Accepts JSON object with property name-value pairs (as returned by read_actor_properties). Use this to restore actor configuration after blueprint reparenting.

**IMPORTANT:** After compiling C++ changes to the MCP plugin, you must restart Unreal Editor to load the updated plugin DLL:
```bash
# From project root
npm run restart:ue:win
```
This command stops UnrealEditor.exe, waits 2 seconds, then reopens the project. After restart, reconnect MCP with `/mcp` command in Claude Code.

**Purpose:** These commands enable full automated blueprint-to-C++ conversion by preserving level instance data. When reparenting a blueprint to C++, C++ constructor defaults don't persist to existing level instances - these commands solve that by:
1. Reading instance properties before reparenting (save to JSON)
2. Reparenting blueprint to C++
3. Restoring instance properties after reparenting
4. Saving the level

**Example Workflow:**
```javascript
// Step 1: Read actor properties
const props = await read_actor_properties({ actor_name: "LevelVisuals_C_6" });

// Step 2: Reparent blueprint to C++
await reparent_blueprint({
  blueprint_path: "/Game/Levels/LevelPrototyping/LevelVisuals",
  parent_class: "ALevelVisuals"
});
await compile_blueprint({ path: "/Game/Levels/LevelPrototyping/LevelVisuals" });

// Step 3: Restore properties
await set_actor_properties({
  actor_name: "LevelVisuals_C_6",
  properties: props.properties
});

// Step 4: Save level
await save_all();
```

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
- **Animation Blueprint Extension Tag Errors (2026-01-31) - RESOLVED**: Errors like "cannot find referenced node with tag 'OffsetRoot'" or 'State Machine Blend Stack' are caused by AnimBlueprintExtension_Tag objects looking for tagged nodes that don't exist. The extensions keep getting recreated during compilation, making them impossible to remove permanently. **Solution**: Use `clear_anim_graph` command to delete all AnimGraph nodes, then manually compile in the editor. This removes the nodes that request the tags, preventing extension recreation. Successfully fixed SandboxCharacter_CMC_ABP and SandboxCharacter_Mover_ABP using this approach. The `clear_animation_blueprint_tags` command can also remove tag extensions, but they will be recreated if the requesting nodes still exist.
- **Orphaned Pin Errors (Fixed 2026-01-30)**: Orphaned pin errors ("In use pin X no longer exists") that persist after `refresh_nodes` can now be fixed with the new `break_orphaned_pins` command. This aggressively removes orphaned pins from blueprint nodes. Successfully used to fix SandboxCharacter_CMC and SandboxCharacter_Mover after AC_FoleyEvents signature changes.
- **Struct Type Mismatches (2026-01-30) - RESOLVED**: When a blueprint struct (S_*) is replaced with a C++ struct (FS_*), and the blueprint struct is used as a Map/Array/Set value type in another struct, the `modify_struct_field` MCP command **cannot** handle this automatically due to fundamental UE5 reflection limitations (C++ structs don't appear in object system until instantiated).
  - **Resolution**: Convert the containing struct to C++ as well. Example: S_LevelStyle contained Map<FName, S_GridMaterialParams>, which blocked changing to FS_GridMaterialParams. Solution: Created FS_LevelStyle in C++, then converted LevelVisuals blueprint to C++ to use FS_LevelStyle.
  - **Lesson**: Full C++ conversion (following the project goal) resolves these type mismatch blockers. Don't attempt partial conversions with blueprint/C++ type mixing.
- **Screenshot Capture (New - 2026-01-30)**: Added `capture_screenshot` MCP command to capture the active Unreal Editor viewport. Screenshots are saved to `ProjectDir/Saved/AutoScreenshot.png`. This enables AI assistants to visually see the editor viewport, level layout, and visual state for debugging and verification. Successfully tested with viewport capture showing level geometry and Game Animation Sample content.

## Blueprint to C++ Conversion Lessons (2026-01-31)

**Context**: Attempted to convert SandboxCharacter_CMC_ABP animation blueprint to C++ using MCP tools.

**What Worked:**
- ✅ C++ skeleton with 76 variables compiles successfully
- ✅ Main update flow (NativeUpdateAnimation) implemented with console variable caching
- ✅ Thread-safe function metadata: `UFUNCTION(BlueprintCallable, Category = "Animation", meta = (BlueprintThreadSafe))`
- ✅ Deleting simple blueprint function graphs via `delete_function_graph` MCP command
- ✅ Reading blueprint logic via `read_function_graphs`, `read_variables`, `read_event_graph` MCP commands
- ✅ Removing error nodes automatically with `remove_error_nodes` command

**What Caused Crashes/Corruption:**
- ❌ Clearing AnimGraph nodes via `clear_anim_graph` leaves orphaned state machine transition nodes that crash on compile
- ❌ Clearing AnimBlueprintExtension_Tag objects via `clear_animation_blueprint_tags` then compiling via MCP causes crashes
- ❌ Converting enum types to uint8 in C++ breaks all blueprint helper functions that reference those variables
- ❌ Attempting to compile blueprint via MCP after tag/AnimGraph cleanup triggers editor crashes
- ❌ Hot-reload after C++ compilation can trigger automatic blueprint recompilation, causing crashes if blueprint is in unstable state

**Critical Insights:**
1. **AnimGraph is separate from update logic**: AnimGraph drives animation blending/pose selection. EventGraph/functions drive update logic. Don't confuse them.
2. **Complex nested state machines can't be safely cleared programmatically**: Animation blueprints with state machines have deeply nested transition graphs that create orphaned references when cleared.
3. **Enum dependencies are fundamental**: Many blueprint helper functions depend on enum types. Converting enums to uint8 cascades errors throughout the blueprint.
4. **MCP compile triggers crashes on corrupted blueprints**: Use manual editor compilation after structural changes to blueprints.
5. **Save before restart**: Always `save_all` before restarting editor to avoid losing changes.

**Recommended Approach for Future Conversions:**
1. **Keep blueprint working** - Don't delete AnimGraph or all function graphs at once
2. **Convert data types first** - Enums and structs should be converted to C++ before converting logic that uses them
3. **Implement C++ alongside blueprint** - Add C++ functions that coexist with blueprint logic
4. **Migrate incrementally** - One function at a time, test after each change
5. **Test frequently** - Verify character/system still works after each conversion step
6. **Don't compile via MCP after major blueprint changes** - Use manual compilation in editor to avoid crash loops
7. **Use git checkpoints** - Commit working states frequently during conversion process

**Alternative**: For complex animation blueprints, consider keeping blueprint AnimGraph intact and only converting EventGraph/function logic to C++, or wait for automated tools like NodeToCode to support UE 5.7+.

## MCP Enhancement Sprints (2026-02-01)

**Background**: Research into other UE5 MCP projects (chongdashu/unreal-mcp, flopperam/unreal-engine-mcp, ChiR24/Unreal_mcp, ayeletstudioindia/unreal-analyzer-mcp) identified gaps in our MCP server capabilities. Key findings documented in `FEATURE_RESEARCH.md` and implementation plan in `IMPLEMENTATION_PLAN.md`.

**Sprint 1: Blueprint Function Creation** ✅ IMPLEMENTED
- **Goal**: Programmatically create and modify blueprint functions to support automated refactoring and hybrid C++/Blueprint workflows
- **Commands Added**:
  - `create_blueprint_function` - Create new function with metadata flags (BlueprintThreadSafe, BlueprintPure, Const)
  - `add_function_input` - Add input parameters with type validation (int, float, bool, string, FVector, FRotator, FTransform, custom types)
  - `add_function_output` - Add output parameters (return values)
  - `rename_blueprint_function` - Rename existing functions
- **Implementation**: Uses FBlueprintEditorUtils::CreateNewGraph(), UEdGraphSchema_K2::CreateDefaultNodesForGraph(), and UK2Node_FunctionEntry/Result pin creation
- **Status**: Implemented 2026-02-01, testing pending

**Sprint 2: Blueprint Node Manipulation** (PLANNED)
- `add_node_to_graph` - Add any node type to blueprint graphs
- `connect_nodes` - Wire pins between nodes with type validation
- `delete_node_from_graph` - Remove specific nodes
- `set_node_property` - Modify node properties and pin defaults

**Sprint 3: Blueprint Graph Analysis** (PLANNED)
- `analyze_graph_complexity` - Calculate metrics (node count, depth, branching)
- `find_conversion_candidates` - Identify simple functions suitable for C++ conversion
- `detect_graph_patterns` - Pattern matching for common logic structures

**Priority**: These enhancements directly support our blueprint-to-C++ conversion workflow by enabling automated refactoring and analysis.

## TODO

- [ ] Test Sprint 1 blueprint function creation commands
- [ ] Implement Sprint 2: Node manipulation
- [ ] Implement Sprint 3: Graph analysis
- [ ] Convert blueprints to C++ using incremental approach (data types first, then logic)
- [ ] Consider converting enums (E_MovementMode, E_Gait, E_Stance, etc.) to C++ as foundation for further conversion
