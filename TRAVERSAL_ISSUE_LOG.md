# Traversal Issue Log

Date: 2026-02-05

## Symptom
Character can no longer grab ledge edges on traversable blocks. Sprint and general movement are functional, but traversal grab does not trigger.

## Environment Notes
- Project: `E:\repo\unreal_engine\UE5MCPTest`
- Reference project: `E:\repo\unreal_engine\GameAnimationSample`
- UE version: 5.7

## Actions Taken (Chronological)

### 1) Restarts/Cleans
- Repeated cycles of `npm run clean:win` and `npm run restart:ue:win`.
- GameMode sometimes reset after clean/restart; fixed via config:
  - `Config/DefaultEngine.ini` now sets:
    - `GlobalDefaultGameMode=/Script/UETest1.GM_Sandbox`
    - `GlobalDefaultServerGameMode=/Script/UETest1.GM_Sandbox`

### 2) Visibility / Mesh Restoration
- Traversable blocks became invisible or had wrong bounds.
- `LevelBlock` C++ updated to:
  - Re-resolve StaticMesh/TextRender components if TRASH.
  - Force visibility on all mesh components.
  - Reassign mesh if missing (`SM_Cube`).
  - Refresh dynamic materials on construction.
- `LevelBlock_Traversable` updated to:
  - Use `FindSplineByName` rather than fixed component pointers.
  - Refresh ledges on construction and `BeginPlay`.
  - Draw debug bounds and spline points each tick.
  - Enable tick for runtime visibility checks.

### 3) Traversal Component Initialization (CMC)
- `SandboxCharacter_CMC` updated to:
  - Find traversal component via reflection.
  - Set traversal component `Mesh` to character mesh.
  - Force `Activate()` and enable tick.
  - Log init info (not always observed in logs; may need verification).

### 4) MCP Extensions Added (for debugging)
Added new MCP tools:
- `read_actor_components`
- `read_actor_component_properties`
- `set_actor_component_property`

### 5) Component Inspection via MCP
Checked `LevelBlock_Traversable_C_19` and spline components:
- Spline points exist (`Ledge_1` / `Ledge_2` etc. contain points).
- StaticMesh component exists and is visible.

### 6) Collision Profile Differences (Likely Root Cause)
Comparison of CDO values:
- `LevelBlock_ORIGINAL.StaticMesh` CDO `CollisionProfileName=BlockAll`
- `LevelBlock_Traversable.StaticMesh` CDO `CollisionProfileName=BlockAllDynamic`

### 7) Forced Collision Profile Fixes
Applied via C++ and MCP:
- C++ change: `LevelBlock` StaticMesh now sets:
  - `SetCollisionProfileName("BlockAll")`
  - `SetCanEverAffectNavigation(false)`
  in constructor and `OnConstruction`.
- MCP: `set_actor_component_property` called for all 28 `LevelBlock_Traversable` instances:
  - `CollisionProfileName = BlockAll`
- `save_all` executed to persist changes.

### 8) Result
After all above changes, **ledge grabbing still fails**.

## Open Questions / Next Steps
1. Verify traversal detection channel:
   - Does `AC_TraversalLogic` trace against a custom channel (e.g., `Traversable`) rather than `BlockAll`?
2. Verify tags and component filters:
   - Are traversal components relying on specific tags or component classes not present after conversion?
3. Validate traversal component references on the player:
   - Confirm `AC_TraversalLogic` has correct `Mesh` / character references at runtime.
4. Inspect overlap/trace settings:
   - Check `CollisionResponse` for `Traversable` channel on traversable blocks.
5. Compare reference project at runtime:
   - Capture traversal component config from `GameAnimationSample` to compare.

## Files Modified During Investigation
- `Source/UETest1/LevelBlock.cpp`
- `Source/UETest1/LevelBlock_Traversable.cpp`
- `Source/UETest1/LevelBlock_Traversable.h`
- `Source/UETest1/SandboxCharacter_CMC.cpp`
- `Source/UETest1/SandboxCharacter_CMC.h`
- `Config/DefaultEngine.ini`
- MCP server additions:
  - `Plugins/ClaudeUnrealMCP/MCPServer/toolDefinitions.js`
  - `Plugins/ClaudeUnrealMCP/Source/ClaudeUnrealMCP/Public/MCPServer.h`
  - `Plugins/ClaudeUnrealMCP/Source/ClaudeUnrealMCP/Private/MCPServerCore.cpp`
  - `Plugins/ClaudeUnrealMCP/Source/ClaudeUnrealMCP/Private/MCPServerReadCommands.cpp`
  - `Plugins/ClaudeUnrealMCP/Source/ClaudeUnrealMCP/Private/MCPServer.cpp`

