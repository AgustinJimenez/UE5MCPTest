# CURRENT_TASK

Goal: Convert Blueprints to C++ in order from easiest to hardest.

---

## CURRENT STATUS (2026-02-21)

**AC_TraversalLogic fully converted to C++ (2026-02-21)**: Last remaining unconverted blueprint. ~450 LOC C++ implementation covering 5-trace detection pipeline, Chooser Table evaluation, motion warping, montage playback, and CMC/Mover abstraction. BP reparented to C++ class, event graph (51 nodes) cleared, 4 function graphs deleted. Also migrated S_TraversalCheckInputs struct in caller BPs (SandboxCharacter_CMC, SandboxCharacter_Mover). 0 blueprint errors.

**Traversal CHT struct migration complete (2026-02-21)**: S_TraversalCheckResult, S_TraversalChooserInputs, S_TraversalChooserOutputs migrated to C++ USTRUCTs. E_TraversalActionType, E_MovementMode, E_Gait migrated in AC_TraversalLogic. Both CHTs (including 4 nested choosers each) fully migrated with 23 bindings per CHT. Traversal works at runtime. See CHOOSER_TABLE_MIGRATION.md for details.

### Walk/Sprint Fix (2026-02-13) — COMPLETE
After Sprint 8 enum migration, walk and sprint animations stopped playing:
- **Root cause (walk)**: CDO Gait and CameraStyle were `(INVALID)` — old BP enum names couldn't deserialize as C++ enum values. Fixed by setting CDO defaults via MCP (`Gait=Run`, `CameraStyle=Medium`).
- **Root cause (sprint)**: BP event graph's "Set members in S_PlayerInputState" node lost WantsToSprint pin after struct migration, so pressing Shift never set WantsToSprint. Also, BP `SandboxCharacter_CMC` inherits from `ACharacter` (not `ASandboxCharacter_CMC` — reparenting was reverted), so C++ sprint logic never runs.
- **Fix**: PC_Sandbox runtime fallback uses reflection to set WantsToSprint, Gait (FEnumProperty), AND MaxWalkSpeed on the CMC every tick based on `IsInputKeyDown(EKeys::LeftShift)`. Also reads SprintSpeeds/RunSpeeds via reflection for correct speed values.
- Key insight: `Cast<ASandboxCharacter_CMC>` always fails — must use reflection for all property access from PC_Sandbox.

### Struct/Enum Migration Pipeline (Sprint 8) — COMPLETE, 0 ERRORS
Successfully migrated all 4 BP UserDefinedStructs and 7 BP UserDefinedEnums to C++ equivalents:
- 132 struct nodes migrated across ~18 blueprints
- 511+ enum pins migrated, 28 enum variables updated, 18 struct field enum refs fixed
- 735+ split struct sub-pins renamed from GUID-suffixed to clean names
- 35 PropertyAccess nodes fixed
- Custom MCP commands built: `migrate_struct_references`, `migrate_enum_references`, `fix_property_access_paths`, `fix_struct_sub_pins`, `fix_pin_enum_type`, `rename_local_variable`, `fix_asset_struct_reference`, `fix_enum_defaults`, `set_pin_default`

### All Errors Resolved
- CameraDirector Chooser Table error: resolved by compilation after struct migration
- Select node OrientToMovement/E_CameraMode error: fixed by `set_pin_default` (changed invalid "OrientToMovement" to "E_CameraMode::FreeCam")
- Sprint/E_Gait reversion after restart: fixed by updating both `SubCategoryObject` AND compiled `FByteProperty::Enum` in struct field definitions, then saving structs

### MCP Server Refactored (2026-02-05)
- Extracted tool definitions to `toolDefinitions.js` and Unreal client to `unrealClient.js`
- C++ side split into `MCPServerCore.cpp` and `MCPServerReadCommands.cpp`
- Added new tools: `delete_node`, `read_input_mapping_context`, `remove_implemented_interface`

### Traversal Investigation (2026-02-05) — Reverted
- Investigated ledge grab failure after C++ conversion changes
- Changes to LevelBlock, LevelBlock_Traversable, and SandboxCharacter_CMC did not fix the issue
- All traversal-related files restored to original state (see TRAVERSAL_ISSUE_LOG.md)

### Blueprint Conversion Assessment (2026-02-11)
- Audited all remaining blueprints against existing C++ files
- Most "small" unconverted assets are **data assets** (PoseSearchSchema, SmartObjectDefinition, CameraAsset, etc.) — already C++ instances with data, not Blueprint classes needing conversion
- **Only 1 remaining feasible unconverted blueprint**: `STT_PlayAnimMontage`
- STT_PlayAnimMontage depends on `AC_SmartObjectAnimation_C` (unconverted, too complex)

---

## BLOCKER: Blueprint vs C++ Interface/Struct Type Mismatch

**This is the main blocker for SandboxCharacter_CMC conversion.**

### Problem
The project has **duplicate interface and struct definitions** - one set in Blueprints and one in C++:

| Blueprint Asset | C++ Definition |
|----------------|----------------|
| `S_PlayerInputState` | `FS_PlayerInputState` |
| `S_CharacterPropertiesForAnimation` | `FS_CharacterPropertiesForAnimation` |
| `S_CharacterPropertiesForCamera` | `FS_CharacterPropertiesForCamera` |
| `S_CharacterPropertiesForTraversal` | `FS_CharacterPropertiesForTraversal` |
| `BPI_SandboxCharacter_Pawn` (BP) | `IBPI_SandboxCharacter_Pawn` (C++) |

When `SandboxCharacter_CMC` is reparented to C++:
- C++ class implements `IBPI_SandboxCharacter_Pawn` returning `FS_` structs
- But blueprints like `AC_TraversalLogic` call the BP interface expecting `S_` structs
- Unreal treats these as **completely different types**
- Result: "Accessed None" runtime errors

### Attempted Migrations (All Reverted)

**Attempt 1: Reparent + Delete Functions**
- Deleted 9 conflicting functions from SandboxCharacter_CMC
- Removed interface implementation from blueprint
- Reparented blueprint to C++ class
- **Failed**: Runtime errors in AC_TraversalLogic due to struct type mismatch
- **Reverted**: SandboxCharacter_CMC.uasset restored from git

**Attempt 2: Interface Type Swap via MCP (2026-02-11)**
- Modified BP interface `BPI_SandboxCharacter_Pawn` to return C++ structs (`/Script/UETest1.*`)
- Refreshed nodes in all affected blueprints
- **Failed**: 26 errors across 8 blueprints — "Only exactly matching structures are considered compatible"
- UE treats UserDefinedStruct and USTRUCT as fundamentally incompatible, even with identical fields
- **Reverted**: All 10 affected .uasset files restored from git

**Attempt 3: CoreRedirects (2026-02-11)**
- Added `[CoreRedirects]` to DefaultEngine.ini with StructRedirects + PropertyRedirects
- Mapped all 33 GUID-suffixed BP field names to clean C++ field names
- Deleted the 4 BP struct .uasset files, restarted editor
- **Failed**: 217 errors — all struct references became `<unknown struct>`
- CoreRedirects do NOT work across UserDefinedStruct → USTRUCT boundary
- **Reverted**: Struct files restored from git, CoreRedirects removed, full rebuild

### Root Cause
- BP UserDefinedStruct fields use GUID-suffixed names (e.g., `Mesh_15_D47797BD4F40417B966E3BB7E0AC62D3`)
- C++ USTRUCT fields use clean FNames (e.g., `Mesh`)
- UE's serialization, CoreRedirects, and pin matching systems cannot bridge this gap
- Affected blueprints: SandboxCharacter_CMC, SandboxCharacter_Mover, AC_TraversalLogic, SandboxCharacter_CMC_ABP, SandboxCharacter_Mover_ABP, CameraDirector_SandboxCharacter, BP_NotifyState_MontageBlendOut, STT_SetCharacterInputState

### Resolution: Custom MCP Migration Pipeline (Sprint 8)
Built automated migration pipeline that programmatically replaces all struct/enum references:
1. `migrate_struct_references` — updates PinSubCategoryObject, StructType, variables, local variables, UserDefinedPins
2. `migrate_enum_references` — updates enum references on pins, variables, Switch nodes
3. `fix_property_access_paths` — updates PropertyAccess node path segments
4. `fix_struct_sub_pins` — renames GUID-suffixed split sub-pins (including parent-prefixed names)
5. `fix_pin_enum_type` — targeted enum type correction with node_guid filter
6. `break_orphaned_pins` — removes stale pins from Return Nodes

Pipeline reduces errors from hundreds to 0. Key insight: struct field enum references have two layers (SubCategoryObject metadata + compiled FByteProperty::Enum) — both must be updated for persistence.

---

## CONVERSION SUMMARY

### Successfully Converted (~57 blueprints)
- Level actors: LevelBlock, LevelBlock_Traversable, LevelVisuals, LevelButton, SpinningArrow, TargetDummy, StillCam
- Teleporter system: Teleporter_Level, Teleporter_Sender, Teleporter_Destination
- Characters: BP_Manny, BP_Quinn, BP_Twinblast, BP_UE4_Mannequin, BP_Echo, BP_Walker
- Controllers: PC_Sandbox, GM_Sandbox, PC_Locomotor, GM_Locomotor
- Smart Objects: BP_SmartObject_Base, BP_SmartBench, DistanceToSmartObject, STT_FindSmartObject, STT_PlayAnimFromBestCost, AC_SmartObjectAnimation
- Movement: BP_MovementMode_Walking, BP_MovementMode_Slide, BP_MovementMode_Falling
- StateTree: All STT_* (including STT_PlayAnimMontage), STE_*, STC_* classes
- AnimNotifies: All BP_AnimNotify_FoleyEvent variants, BP_NotifyState_*
- Components: AC_FoleyEvents, AC_PreCMCTick, AC_VisualOverrideManager, AC_TraversalLogic
- Cameras: CameraDirector_SandboxCharacter
- PoseSearch Channels: PSC_Traversal_Head, PSC_Traversal_Pos, PSC_DistanceToTraversalObject
- Interfaces: BPI_SandboxCharacter_ABP, BPI_InteractionTransform, I_FoleyAudioBankInterface
- Data: BFL_HelpfulFunctions, DABP_FoleyAudioBank

### Recently Converted (2026-02-21)
- **AC_TraversalLogic** — Full C++ port (~450 LOC): 5-trace detection pipeline, Chooser Table evaluation via FChooserEvaluationContext, motion warping with curve sampling, montage playback, CMC/Mover abstraction, RPCs. Also migrated S_TraversalCheckInputs struct in callers. Added AnimationWarpingRuntime module dep.
- **AC_SmartObjectAnimation** — Implemented `EvaluateDistanceAndMotionMatch()` using ProxyTable/Chooser API. Loads CHPA_SmartObject ProxyAsset, creates FChooserEvaluationContext with PoseHistory/distance/angle inputs, evaluates via MakeLookupProxyWithOverrideTable. Added Chooser+ProxyTable+StructUtils module deps.
- **STT_FindSmartObject** — StateTree task, C++ EnterState override takes precedence. BP event graph (61 nodes) and function graph cleared.
- **STT_PlayAnimFromBestCost** — StateTree task, already fully C++ (empty event graph). Confirmed working.
- **STT_PlayAnimMontage** (2026-02-11) — Uses reflection to call PlayMontage_Multi on AC_SmartObjectAnimation (BP class)

### Cannot Convert - Unexported Engine Symbols (~17 blueprints)
- All AM_* AnimModifiers — parent classes (UMotionExtractorModifier, UCopyBonesModifier,
  UFootstepAnimEventsModifier) lack DLL export macros in AnimationModifierLibrary plugin
- C++ subclassing causes LNK2019 unresolved externals; Blueprints work via reflection
- These must remain as Blueprints unless Epic exports the symbols in a future UE version

### High Priority - Blocked by Type Mismatch
- **SandboxCharacter_CMC** - Main character class
- **SandboxCharacter_Mover** - Mover-based character

### Low Priority
- BP_Kellan (MetaHuman character)
- Various animation blueprints (SandboxCharacter_CMC_ABP, SandboxCharacter_Mover_ABP)

---

## REFERENCE

**Original Sample Project**: `E:\repo\unreal_engine\GameAnimationSample\`
- Use for checking original blueprint configurations and default values

**Backup Folder**: `E:\repo\unreal_engine\backup\`
- Contains Content folder snapshots from various development points

**Reference Blueprints**: `Content/Levels/LevelPrototyping/*_ORIGINAL.uasset`
- 11 original blueprints preserved for property reference

---

## KEY LEARNINGS

### Blueprint CDO vs Level Instance Properties
- `read_class_defaults` reads Blueprint CDO values, NOT level instance overrides
- Use `read_actor_properties` for actual level instance values stored in .umap files
- CDO values are often placeholder values that don't match actual gameplay behavior

### Blueprint Sub-Object Property Updates
- Changes to blueprint CDOs don't automatically propagate to existing sub-object instances
- Use `clear_component_map_value_array` to modify properties within map value objects

### Actor Construction Dependencies
- OnConstruction execution order is undefined between actors
- Use `FTSTicker::GetCoreTicker()` for deferred updates when `GetWorld()` is null
- Only create `DynamicMaterialInstance` once, reuse on subsequent constructions

### Full C++ Conversion Principle
When encountering dependencies (e.g., blueprint struct inside another), convert **both** to C++ rather than attempting workarounds. Partial conversion blocks progress.
