# Chooser Table Migration — BP Structs → C++ USTRUCTs

## Goal
Migrate CHT_TraversalMontages_CMC and CHT_TraversalMontages_Mover from BP UserDefinedStructs to C++ USTRUCTs.
This enables AC_TraversalLogic to eventually be reparented to C++.

## Architecture Problem (Discovered 2026-02-20)

The migration creates a **type conflict chain**:
```
CHT uses C++ structs
  → Evaluate Chooser nodes need C++ struct input
    → Make S_TraversalChooserInputs has C++ enum fields (E_MovementMode, E_Gait)
      → needs C++ enum sources
        → Break S_CharacterPropertiesForTraversal must output C++ enums
          → S_CharacterPropertiesForTraversal struct must be C++ type
            → K2Node_Message must call via C++ interface (to return C++ struct)
              → CMC/Mover must implement C++ interface (not just BP interface)
```

### Failed Approach: Migrate AC_TraversalLogic First
- `migrate_interface_references` on AC_TraversalLogic changed K2Node_Message from BP→C++ interface
- Compiles with 0 errors (types are consistent within AC_TraversalLogic)
- **Runtime failure**: C++ interface dispatch calls `Get_PropertiesForTraversal_Implementation()`
  which has no BP override (BP function graphs are registered for the BP interface, not C++)
- Returns empty struct → Capsule=None → runtime crash

### Correct Approach: Convert CMC/Mover Interface First (Bottom-Up)
1. Remove BP interface from CMC/Mover (C++ parent already has C++ interface)
2. Convert function graphs to C++ interface overrides
3. Update Return Node struct types from BP→C++
4. THEN migrate AC_TraversalLogic (interface refs, struct refs, chooser nodes, enums)
5. THEN migrate CHTs (ContextData + column bindings)

## Critical Discovery (2026-02-20)

**No C++ parent classes exist for CMC or Mover!**
- CMC parent: `ACharacter` (engine class) — NO custom `ASandboxCharacter_CMC`
- Mover parent: `APawn` (engine class) — NO custom `ASandboxCharacter_Mover`
- Both implement BP interface `BPI_SandboxCharacter_Pawn` only
- NO C++ class implements `IBPI_SandboxCharacter_Pawn` — zero implementors
- The C++ interface exists but is unused for dispatch
- `migrate_interface_references` was fundamentally wrong — nothing to dispatch to

## Correct Approach: Enum Migration (No Interface Changes)

**Don't touch the interface. Don't touch S_CharacterPropertiesForTraversal. Only migrate what's needed.**

### Steps
1. Migrate CHTs (ContextData + column bindings) → C++ structs
2. Migrate S_TraversalCheckResult, S_TraversalChooserInputs, S_TraversalChooserOutputs → C++
3. Migrate E_MovementMode + E_Gait + E_TraversalActionType enums globally
   - This makes Break S_CharacterPropertiesForTraversal output C++ enum types
   - Which matches Make S_TraversalChooserInputs expectations
   - No interface change needed!
4. fix_struct_sub_pins for the 3 migrated structs
5. Fix Evaluate Chooser nodes (reconstruct + break_orphaned_pins)
6. Fix PSC_DistanceToTraversalObject
7. Compile, save, restart, verify

### Why This Works
- K2Node_Message stays on BP interface → BP dispatch works → runtime OK
- S_CharacterPropertiesForTraversal stays as BP struct → CMC/Mover Return Nodes untouched
- BUT its enum FIELDS (E_MovementMode, E_Gait) get migrated to C++ enum type
- Break node outputs C++ enum types → Make node accepts C++ enum types → match!
- The verified migration pipeline from earlier sprints did this exact enum migration with 0 errors

### Execution Log

#### Attempt 1 (Failed — CppForm crash)
- Steps 1-6 succeeded, all 10 BPs compiled OK
- Saved struct assets (S_TraversalCheckResult etc.) after enum migration
- **CppForm assertion crash on restart** — saving BP structs with C++ enum SubCategoryObject pointers
  corrupts the asset. UE expects UserDefinedEnum in struct field SubCategoryObject.
- Fix: NEVER save BP struct assets after `migrate_enum_references`. Only save BPs.

#### Attempt 2 (Failed — SetFieldsInStruct pin loss)
- Skipped saving struct assets
- **AC_TraversalLogic compile error**: "Chosen Montage" and "Hit Component" pins missing on Set Members nodes
- Root cause: `K2Node_SetFieldsInStruct::ShowPinForProperties` stores GUID-suffixed field names.
  After `migrate_struct_references` changes StructType to C++, `ReconstructNode()` creates pins
  from C++ properties but ShowPinForProperties still references GUID names → pins silently dropped.
- Fix: Added ShowPinForProperties remapping to MCPServerMigrationStruct_Migrate.inc (line 338-348)

#### Attempt 3 (COMPILED — 0 errors, but runtime failure → REVERTED)
Pipeline executed in order:
1. **CHT Migration**: CHT_TraversalMontages_CMC + Mover → C++ struct ContextData (field_name_map + struct_map)
2. **Struct Migration**: S_TraversalCheckResult (2 BPs, 95 nodes), S_TraversalChooserInputs (2 BPs, 5 nodes), S_TraversalChooserOutputs (1 BP, 4 nodes)
3. **Enum Migration**: E_TraversalActionType (6 pins), E_MovementMode (134 pins, 5 vars), E_Gait (82 pins, 7 vars)
4. **fix_struct_sub_pins**: S_TraversalCheckResult (512 pins), S_TraversalChooserInputs (292 pins), S_TraversalChooserOutputs (36 pins)
5. **Reconstruct + break_orphaned_pins**: EvalChooser CMC (10 orphaned), Mover (0), Make (0), Break x2 (0), PSC (1)
6. **Compile**: All 10 BPs → OK
7. **Save**: 12 assets saved individually (BPs + CHTs — NOT struct assets)
8. **Restart + verify**: 0 LogBlueprint errors

**Runtime failure**: Traversal did not trigger in-game. Root causes:

1. **CHT column bindings incomplete**: `migrate_chooser_table` only walked `ColumnsStructs` (6 columns) and `ResultsStructs`. Additional `PropertyBindingChain` arrays existed in other CHT properties (e.g., nested within column `InputValue.Binding`). Properties like ObstacleHeight, Speed, MovementMode, PoseHistory on `S_TraversalChooserInputs` still had GUID-suffixed names, causing `LogChooser: Error: Could not find property` at runtime.

2. **Evaluate Chooser node connections severed**: `break_orphaned_pins` on the EvalChooser CMC node removed 4 struct pins (S_TraversalChooserInputs input, S_TraversalChooserOutputs output) and their connections to Make/Break struct nodes AND Result (AnimMontage) output connections. The connections could not be automatically restored because `read_function_graphs` doesn't return `linked_to` connection data, and the pin types changed from BP to C++ (connections are rejected when types mismatch).

3. **Fundamental connection preservation problem**: There is no way to atomically change both sides of a pin connection from BP to C++ type. When Make/Break nodes are migrated first, their connections to Evaluate Chooser nodes break (Eval still has BP types). When Eval nodes are reconstructed later, the connections are already gone and can't be restored. The reverse order has the same problem.

**REVERTED** (2026-02-21): All traversal assets git-restored to original BP UserDefinedStruct state:
- AC_TraversalLogic.uasset
- CHT_TraversalMontages_CMC.uasset
- CHT_TraversalMontages_Mover.uasset
- LevelBlock_Traversable.uasset
- PSC_DistanceToTraversalObject.uasset

After reversion: 0 LogBlueprint errors, 0 LogChooser errors.

#### Attempt 4 (SUCCESS — 2026-02-21)

**Key discovery**: `read_function_graphs` ALREADY returns connection data in a `connections` array per node (format: `from_pin`, `to_node`, `to_pin`). And `connect_nodes` supports function graphs via `graph_name` parameter.

**Key insight**: `fix_struct_sub_pins` renames pins in-place preserving ALL connections — including cross-node connections to Evaluate Chooser nodes. Combined with `reconstruct_node` on Eval nodes (which also preserves connections where pin names match), the 10 critical connections survived the migration WITHOUT needing manual `connect_nodes` reconnection.

**Pipeline executed:**
1. **CHT Migration**: Both CHTs — 2 context data + 6 column bindings each (GUID→clean names)
2. **Struct Migration**: S_TraversalCheckResult (95 nodes, 180 conn), S_TraversalChooserInputs (5 nodes, 29 conn), S_TraversalChooserOutputs (5 nodes, 19 conn) — 228 connections preserved, 0 failed
3. **Enum Migration**: E_TraversalActionType (global), E_MovementMode + E_Gait (AC_TraversalLogic only, `skip_struct_fields: true`)
4. **fix_struct_sub_pins**: 884 pins renamed across 3 structs. `fix_property_access_paths`: 0 fixes needed
5. **Reconstruct Evaluate Chooser nodes**: Both Eval CMC + Mover reconstructed — all connections survived (pin names matched post-migration)
6. **break_orphaned_pins**: AC_TraversalLogic (6 removed), PSC_DistanceToTraversalObject (1 removed), LevelBlock_Traversable (2 removed)

**Post-success fix: Nested Chooser bindings (2026-02-21)**
After initial success (0 compile errors), runtime testing revealed `LogPoseSearch: Error: FPoseSearchColumn::Filter, missing IPoseHistory` and 28 `LogChooser` errors. Root cause: each CHT has 4 **nested UChooserTable objects** stored in a `NestedObjects` (`TArray<UObject*>`) property. These nested choosers have their own `ColumnsStructs` with `PropertyBindingChain` arrays that still had GUID-suffixed names.

The original walker only walked `TArray<FInstancedStruct>` properties on the top-level CHT. It missed `NestedObjects` because it's `TArray<UObject*>` (not FInstancedStruct).

**Fix**: Enhanced `migrate_chooser_table` C++ code (MCPServerMigrationAssets.cpp) to:
1. Walk ALL property types on the CHT (not just TArray<FInstancedStruct>)
2. Detect `TArray<UObject*>` properties, cast entries to UChooserTable
3. Walk each nested chooser's properties recursively
4. Update nested chooser ContextData struct pointers via struct_map

Result: 17 additional bindings fixed per CHT (in nested choosers [1]-[3]), plus 4 nested context data struct pointer updates. After save + restart: 0 LogChooser errors, 0 LogPoseSearch errors. Traversal works at runtime.

7. **Compile**: 111 BPs — 0 errors, 0 warnings
8. **Save**: AC_TraversalLogic, PSC, LevelBlock_Traversable, CHT_CMC, CHT_Mover (individually, NOT save_all, NOT struct assets)
9. **Restart + verify**: 0 LogBlueprint errors, 0 LogChooser errors

**Connection topology preserved** (all 10 verified post-migration):
- Make (B2526B37) `S_TraversalChooserInputs` → both Eval nodes
- Both Eval → respective Break nodes (`S_TraversalChooserOutputs`)
- Both Eval `Result` → respective SetMembers (`ChosenMontage`)
- Both Break `ActionType`/`MontageStartTime` → respective SetMembers

**Why this worked (unlike Attempt 3)**:
- `fix_struct_sub_pins` renames pins on Make/Break/SetMembers nodes in-place, so connections to Eval nodes (which still had old GUID pin names at that point) were preserved
- `reconstruct_node` on Eval nodes recreates pins from C++ struct types; since Make/Break already had C++ types, connections were accepted
- Did NOT use `break_orphaned_pins` on Eval nodes (which destroyed connections in Attempt 3)
- Orphaned GUID pins on SetMembers/PSC were cleaned up AFTER all connections were established

---

## Struct/Enum Field Mappings

### S_CharacterPropertiesForTraversal (BP → C++)
| BP Field (GUID) | C++ Field |
|---|---|
| Capsule_21_D1F3797D47A5FB49C3DFAE8FAB15AFCC | Capsule |
| Mesh_15_D47797BD4F40417B966E3BB7E0AC62D3 | Mesh |
| MotionWarping_18_C9F4AD1440C92128F649A7BA8B49094B | MotionWarping |
| MovementMode_22_F39395024EEBA400FBE9FB8AE0EF7350 | MovementMode |
| Gait_12_9093F6E14192D37D2F0223B94E64FF71 | Gait |
| Speed_26_C6C3500C4C3030A343948F80DFD60AEE | Speed |

### S_TraversalCheckResult (BP → C++)
16 fields — see TraversalTypes.h for full mapping

### S_TraversalChooserInputs (BP → C++)
| BP Field (GUID) | C++ Field |
|---|---|
| ActionType_3_18E96CC94880BFEFDC529CA9084C7794 | ActionType |
| HasFrontLedge_19_52F8C45A46BED06CD777A5A6EFBB34F4 | HasFrontLedge |
| HasBackLedge_20_9D55861C4948BF48E881ECA3E3805530 | HasBackLedge |
| HasBackFloor_21_F68D405E481F582DB3E2AA9355C0C405 | HasBackFloor |
| ObstacleHeight_6_E759F92D423D05AD1DDE4CA66F6F1500 | ObstacleHeight |
| ObstacleDepth_14_BD8858FC4AED728E870ED28C69562B88 | ObstacleDepth |
| BackLedgeHeight_28_5615150C48092DA79C69C89831F5B95C | BackLedgeHeight |
| DistanceToLedge_35_09AB8E3E477D88FB9D2D538C1C3B315F | DistanceToLedge |
| MovementMode_29_44989D3F44E046AE98C6A9BBD4F50600 | MovementMode |
| Gait_15_00BB4D6246CE4D99490807AAB3792C91 | Gait |
| Speed_12_7B2F6B2946E5852C3FB47E80642F1F46 | Speed |
| PoseHistory_32_7A4B979D40CEF39007714DA77DD4EE5F | PoseHistory |
| MontageStartTime_18_F6195BE049D6E5251B9518B127EC61DA | MontageStartTime |

### S_TraversalChooserOutputs (BP → C++)
3 fields — see TraversalTypes.h

## Key Learnings
- **NEVER trust `compile_blueprint` returning OK** — always grep editor log
- **`migrate_interface_references` breaks runtime** if implementors still use BP interface
- **FunctionResult sub-pin names** derive from the interface function signature, NOT from pin SubCategoryObject
- **`fix_struct_sub_pins`** renames pins in-place preserving connections — best approach for struct migration
- **`reconstruct_node`** on FunctionResult in interface graphs recreates pins from interface function signature (dangerous)
- **UK2Node_SetFieldsInStruct inherits from UK2Node_MakeStruct** — must check SetFieldsInStruct BEFORE MakeStruct in Cast chains, otherwise MakeStruct cast matches first and SetFieldsInStruct-specific logic (ShowPinForProperties remapping) never runs
- **UE struct pointer comparison can fail** — UE may load multiple instances of the same UserDefinedStruct. Use `GetName()` as fallback alongside pointer equality
- **`break_orphaned_pins` on Evaluate Chooser data pins is DESTRUCTIVE** — removes connected struct parameter pins AND AnimMontage Result connections, breaking the entire Chooser evaluation flow. Only safe on isolated nodes with no data pin connections
- **CHT `PropertyBindingChain` exists in nested structures** — not just in `ColumnsStructs[i].InputValue.Binding.PropertyBindingChain` but potentially in other `TArray<FInstancedStruct>` properties. Must use reflection to walk all arrays
- **Evaluate Chooser node pins come from CHT ContextData** — cannot be directly set; node must be reconstructed to pick up changed CHT context data. This creates the connection preservation problem described above
- **CHT `NestedObjects` contains nested UChooserTable instances** — `TArray<UObject*>` property on UChooserTable. The generic `TArray<FInstancedStruct>` walker misses these entirely. Must cast UObject entries to UChooserTable and walk their ColumnsStructs/ContextData recursively. Each nested chooser can have its own columns with PropertyBindingChain arrays
- **`migrate_chooser_table` must walk ALL property types** — not just TArray<FInstancedStruct>. CHTs have struct properties (FallbackResult), object arrays (NestedObjects), and other non-instanced-struct arrays that may contain or reference bindings
- **Runtime testing is essential after CHT migration** — 0 compile errors + 0 LogBlueprint errors does NOT mean the Chooser works at runtime. Must also check LogChooser and LogPoseSearch errors, and test in-game
