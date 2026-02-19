export const MCP_TOOL_DEFINITIONS = [
      {
        name: "ping",
        description: "Test connection to Unreal Engine",
        inputSchema: {
          type: "object",
          properties: {},
        },
      },
      {
        name: "reload_mcp_server",
        description: "Reload the MCP server (useful after code changes to index.js)",
        inputSchema: {
          type: "object",
          properties: {},
        },
      },
      {
        name: "list_blueprints",
        description: "List all blueprints in the project",
        inputSchema: {
          type: "object",
          properties: {
            path: {
              type: "string",
              description: "Filter by path prefix (e.g., /Game/Blueprints)",
            },
          },
        },
      },
      {
        name: "check_all_blueprints",
        description: "Compile all blueprints and return a list of those with errors or warnings. Useful for finding broken blueprints after code changes.",
        inputSchema: {
          type: "object",
          properties: {
            path: {
              type: "string",
              description: "Filter by path prefix (e.g., /Game/Blueprints). Defaults to /Game/",
            },
            include_warnings: {
              type: "boolean",
              description: "Include blueprints with warnings (not just errors). Default: false",
            },
          },
        },
      },
      {
        name: "read_blueprint",
        description: "Get overview of a blueprint (name, parent class, counts)",
        inputSchema: {
          type: "object",
          properties: {
            path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
          },
          required: ["path"],
        },
      },
      {
        name: "read_variables",
        description: "Read all variables defined in a blueprint",
        inputSchema: {
          type: "object",
          properties: {
            path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
          },
          required: ["path"],
        },
      },
      {
        name: "read_class_defaults",
        description: "Read Blueprint Class Default Object (CDO) properties, including inherited properties. IMPORTANT: This reads class defaults from the Blueprint asset (.uasset), NOT level instance property overrides. For actual working values from a placed actor, use read_actor_properties instead. CDO values are often placeholder/first-iteration values and may not match actual gameplay behavior.",
        inputSchema: {
          type: "object",
          properties: {
            path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
          },
          required: ["path"],
        },
      },
      {
        name: "read_components",
        description: "Read all components in a blueprint's component hierarchy",
        inputSchema: {
          type: "object",
          properties: {
            path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
          },
          required: ["path"],
        },
      },
      {
        name: "read_component_properties",
        description: "Read all properties of a specific component in a blueprint's CDO (Class Default Object). Use this to inspect component configuration including class references, default values, and settings.",
        inputSchema: {
          type: "object",
          properties: {
            path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
            component_name: {
              type: "string",
              description: "Name of the component to read properties from (e.g., CharacterMover, SkeletalMesh)",
            },
          },
          required: ["path", "component_name"],
        },
      },
      {
        name: "read_event_graph",
        description:
          "Read the event graph nodes and their connections from a blueprint",
        inputSchema: {
          type: "object",
          properties: {
            path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
          },
          required: ["path"],
        },
      },
      {
        name: "read_event_graph_detailed",
        description:
          "Read the event graph nodes, connections, and pin default values from a blueprint",
        inputSchema: {
          type: "object",
          properties: {
            path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
            max_nodes: {
              type: "integer",
              description: "Optional max nodes to return (for large graphs, use pagination)",
            },
            start_index: {
              type: "integer",
              description: "Optional start index into graph nodes (pagination)",
            },
          },
          required: ["path"],
        },
      },
      {
        name: "read_function_graphs",
        description:
          "Read function graph nodes, connections, and pin default values from a blueprint",
        inputSchema: {
          type: "object",
          properties: {
            path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
            name: {
              type: "string",
              description: "Optional function graph name to filter (exact match)",
            },
            max_nodes: {
              type: "integer",
              description: "Optional max nodes per graph (for large graphs)",
            },
            start_index: {
              type: "integer",
              description: "Optional start index into graph nodes (pagination)",
            },
          },
          required: ["path"],
        },
      },
      {
        name: "read_timelines",
        description: "Read timeline templates, tracks, and keys from a blueprint",
        inputSchema: {
          type: "object",
          properties: {
            path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
          },
          required: ["path"],
        },
      },
      {
        name: "read_interface",
        description: "Read function signatures from a Blueprint Interface",
        inputSchema: {
          type: "object",
          properties: {
            path: {
              type: "string",
              description: "Full path to the interface asset",
            },
          },
          required: ["path"],
        },
      },
      {
        name: "read_user_defined_struct",
        description: "Read fields from a User Defined Struct asset",
        inputSchema: {
          type: "object",
          properties: {
            path: {
              type: "string",
              description: "Full path to the struct asset",
            },
          },
          required: ["path"],
        },
      },
      {
        name: "read_user_defined_enum",
        description: "Read entries from a User Defined Enum asset",
        inputSchema: {
          type: "object",
          properties: {
            path: {
              type: "string",
              description: "Full path to the enum asset",
            },
          },
          required: ["path"],
        },
      },
      {
        name: "list_actors",
        description: "List all actors in the current level",
        inputSchema: {
          type: "object",
          properties: {},
        },
      },
      {
        name: "read_actor_components",
        description: "List components on a level actor instance (name, class, active state). Use this to discover component names for read_actor_component_properties.",
        inputSchema: {
          type: "object",
          properties: {
            actor_name: {
              type: "string",
              description: "Name of the actor in the level (e.g., LevelBlock_Traversable_C_24)",
            },
          },
          required: ["actor_name"],
        },
      },
      {
        name: "read_actor_component_properties",
        description: "Read EditAnywhere/BlueprintVisible properties from a specific component on a level actor instance.",
        inputSchema: {
          type: "object",
          properties: {
            actor_name: {
              type: "string",
              description: "Name of the actor in the level (e.g., LevelBlock_Traversable_C_24)",
            },
            component_name: {
              type: "string",
              description: "Component name to inspect (use read_actor_components to find names)",
            },
          },
          required: ["actor_name", "component_name"],
        },
      },
      {
        name: "find_actors_by_name",
        description: "Search for actors by name pattern (supports wildcards: * for any characters, ? for single character)",
        inputSchema: {
          type: "object",
          properties: {
            name_pattern: {
              type: "string",
              description: "Name pattern to search for (e.g., 'LevelBlock*', '*Traversable*', 'Player?')",
            },
            actor_class: {
              type: "string",
              description: "Optional: Filter by actor class (e.g., 'LevelBlock_C', 'StaticMeshActor')",
            },
          },
          required: ["name_pattern"],
        },
      },
      {
        name: "get_actor_material_info",
        description: "Get detailed material information from an actor's components (materials, textures, parameters)",
        inputSchema: {
          type: "object",
          properties: {
            actor_name: {
              type: "string",
              description: "Name of the actor in the level (e.g., LevelBlock_C_0)",
            },
          },
          required: ["actor_name"],
        },
      },
      {
        name: "get_scene_summary",
        description: "Get a comprehensive overview of the current level (actor counts by class, level info, performance stats)",
        inputSchema: {
          type: "object",
          properties: {
            include_details: {
              type: "boolean",
              description: "Include detailed breakdown of actor types (default: true)",
            },
          },
        },
      },
      // Write commands
      {
        name: "add_component",
        description: "Add a component to a blueprint",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
            component_class: {
              type: "string",
              description: "Class name of the component to add (e.g., CameraToggleComponent, StaticMeshComponent)",
            },
            component_name: {
              type: "string",
              description: "Name for the new component (optional, auto-generated if not provided)",
            },
          },
          required: ["blueprint_path", "component_class"],
        },
      },
      {
        name: "set_component_property",
        description: "Set a property value on a component in a blueprint",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
            component_name: {
              type: "string",
              description: "Name of the component to modify",
            },
            property_name: {
              type: "string",
              description: "Name of the property to set",
            },
            property_value: {
              type: "string",
              description: "Value to set (for object references, use full asset path)",
            },
          },
          required: ["blueprint_path", "component_name", "property_name", "property_value"],
        },
      },
      {
        name: "set_blueprint_cdo_class_reference",
        description: "Set a class reference property on a component in a blueprint CDO (Class Default Object). Use this to change blueprint class references to C++ classes (e.g., change BP_MovementMode_Falling to FallingMode C++ class).",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
            component_name: {
              type: "string",
              description: "Name of the component to modify (e.g., CharacterMover)",
            },
            property_name: {
              type: "string",
              description: "Name of the class property to set (e.g., FallingModeClass)",
            },
            class_name: {
              type: "string",
              description: "Full path or name of the C++ class to reference (e.g., /Script/UETest1.FallingMode or FallingMode)",
            },
          },
          required: ["blueprint_path", "component_name", "property_name", "class_name"],
        },
      },
      {
        name: "replace_component_map_value",
        description: "Replace an object instance in a component's map property with a new instance of a different class. Use this to replace blueprint movement mode instances with C++ class instances in the MovementModes map.",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
            component_name: {
              type: "string",
              description: "Name of the component to modify (e.g., CharacterMover)",
            },
            property_name: {
              type: "string",
              description: "Name of the map property (e.g., MovementModes)",
            },
            map_key: {
              type: "string",
              description: "The map key to modify (e.g., 'Falling', 'Walking')",
            },
            target_class: {
              type: "string",
              description: "Full path or name of the C++ class to instantiate (e.g., /Script/Mover.FallingMode or FallingMode)",
            },
          },
          required: ["blueprint_path", "component_name", "property_name", "map_key", "target_class"],
        },
      },
      {
        name: "replace_blueprint_array_value",
        description: "Replace an object instance in a blueprint CDO's array property with a new instance of a different class. Use this to replace blueprint transition instances with C++ class instances in the Transitions array.",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
            property_name: {
              type: "string",
              description: "Name of the array property (e.g., Transitions)",
            },
            array_index: {
              type: "integer",
              description: "Index of the array element to replace (0-based)",
            },
            target_class: {
              type: "string",
              description: "Full path or name of the C++ class to instantiate (e.g., /Script/Mover.BaseMovementModeTransition or BaseMovementModeTransition)",
            },
          },
          required: ["blueprint_path", "property_name", "array_index", "target_class"],
        },
      },
      {
        name: "add_input_mapping",
        description: "Add a key mapping to an input mapping context",
        inputSchema: {
          type: "object",
          properties: {
            context_path: {
              type: "string",
              description: "Full path to the input mapping context asset",
            },
            action_path: {
              type: "string",
              description: "Full path to the input action asset",
            },
            key: {
              type: "string",
              description: "Key name (e.g., O, SpaceBar, LeftMouseButton)",
            },
          },
          required: ["context_path", "action_path", "key"],
        },
      },
      {
        name: "reparent_blueprint",
        description: "Reparent a blueprint to a new parent class",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
            parent_class: {
              type: "string",
              description: "Parent class name or path (e.g., /Script/Engine.Actor or /Game/Blueprints/BP_MyBase.BP_MyBase_C)",
            },
          },
          required: ["blueprint_path", "parent_class"],
        },
      },
      {
        name: "compile_blueprint",
        description: "Compile a blueprint",
        inputSchema: {
          type: "object",
          properties: {
            path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
          },
          required: ["path"],
        },
      },
      {
        name: "save_asset",
        description: "Save an asset to disk",
        inputSchema: {
          type: "object",
          properties: {
            path: {
              type: "string",
              description: "Full path to the asset",
            },
          },
          required: ["path"],
        },
      },
      {
        name: "save_all",
        description: "Save all modified assets in the project",
        inputSchema: {
          type: "object",
          properties: {},
        },
      },
      {
        name: "delete_interface_function",
        description: "Delete a function from a Blueprint Interface",
        inputSchema: {
          type: "object",
          properties: {
            interface_path: {
              type: "string",
              description: "Full path to the interface asset",
            },
            function_name: {
              type: "string",
              description: "Name of the function to delete",
            },
          },
          required: ["interface_path", "function_name"],
        },
      },
      {
        name: "remove_implemented_interface",
        description: "Remove an implemented interface from a blueprint. Use this when a blueprint has interface function overrides that conflict with C++ parent implementations.",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
            interface_name: {
              type: "string",
              description: "Name of the interface to remove (e.g., 'BPI_SandboxCharacter_Pawn')",
            },
          },
          required: ["blueprint_path", "interface_name"],
        },
      },
      {
        name: "list_structs",
        description: "Debug command: List all registered UScriptStruct objects matching a pattern. Useful for discovering discoverable struct names.",
        inputSchema: {
          type: "object",
          properties: {
            pattern: {
              type: "string",
              description: "Pattern to filter struct names (e.g., 'FS_', 'CharacterProperties'). Default: 'FS_'",
            },
          },
        },
      },
      {
        name: "modify_interface_function_parameter",
        description: "Modify a parameter type in a Blueprint Interface function. Note: UE strips the 'F' prefix from C++ struct names in reflection (e.g., FS_PlayerInputState becomes S_PlayerInputState). Use list_structs to find discoverable names.",
        inputSchema: {
          type: "object",
          properties: {
            interface_path: {
              type: "string",
              description: "Full path to the interface asset",
            },
            function_name: {
              type: "string",
              description: "Name of the function to modify",
            },
            parameter_name: {
              type: "string",
              description: "Name of the parameter to modify (e.g., 'ReturnValue' for return type)",
            },
            new_type: {
              type: "string",
              description: "New type path (e.g., '/Script/UETest1.S_PlayerInputState' for C++ struct, '/Game/Blueprints/Data/S_PlayerInputState.S_PlayerInputState' for Blueprint struct)",
            },
            is_output: {
              type: "boolean",
              description: "True if modifying an output/return parameter, false for input parameters. Default: false",
            },
          },
          required: ["interface_path", "function_name", "parameter_name", "new_type"],
        },
      },
      {
        name: "delete_function_graph",
        description: "Delete a function graph from a Blueprint or Blueprint Function Library",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
            function_name: {
              type: "string",
              description: "Name of the function graph to delete",
            },
          },
          required: ["blueprint_path", "function_name"],
        },
      },
      {
        name: "clear_event_graph",
        description: "Clear all nodes from a blueprint's event graph (useful for full C++ conversions to remove blueprint logic)",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
          },
          required: ["blueprint_path"],
        },
      },
      {
        name: "empty_graph",
        description: "TEST: Empty all nodes from event graph (alias for clear_event_graph)",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
          },
          required: ["blueprint_path"],
        },
      },
      {
        name: "refresh_nodes",
        description: "Refresh/reconstruct all nodes in a blueprint to fix stale pin errors",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
          },
          required: ["blueprint_path"],
        },
      },
      {
        name: "break_orphaned_pins",
        description: "Aggressively remove orphaned pins and break their connections. Use when refresh_nodes doesn't fix orphaned pin errors.",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
          },
          required: ["blueprint_path"],
        },
      },
      {
        name: "delete_user_defined_struct",
        description: "Delete a user-defined struct asset. Useful when replacing blueprint structs with C++ versions.",
        inputSchema: {
          type: "object",
          properties: {
            struct_path: {
              type: "string",
              description: "Full path to the struct asset",
            },
          },
          required: ["struct_path"],
        },
      },
      {
        name: "modify_struct_field",
        description: "Modify a field type in a user-defined struct. Useful for updating blueprint structs to use C++ struct types.",
        inputSchema: {
          type: "object",
          properties: {
            struct_path: {
              type: "string",
              description: "Full path to the struct asset",
            },
            field_name: {
              type: "string",
              description: "Name of the field to modify (e.g., BlockColors_19_BD7B5F9248A47F4BA4AEE2BCADEEA20F)",
            },
            new_type: {
              type: "string",
              description: "New type for the field (e.g., FS_GridMaterialParams for C++ struct, S_GridMaterialParams for blueprint struct)",
            },
          },
          required: ["struct_path", "field_name", "new_type"],
        },
      },
      {
        name: "set_blueprint_compile_settings",
        description: "Modify blueprint compilation settings (e.g., thread-safe execution, construction script behavior)",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
            run_construction_script_on_drag: {
              type: "boolean",
              description: "Whether to run construction script when dragging in editor",
            },
            generate_const_class: {
              type: "boolean",
              description: "Whether to generate const class",
            },
            force_full_editor: {
              type: "boolean",
              description: "Whether to force full editor",
            },
          },
          required: ["blueprint_path"],
        },
      },
      {
        name: "modify_function_metadata",
        description: "Modify function metadata flags (e.g., BlueprintThreadSafe, BlueprintPure)",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
            function_name: {
              type: "string",
              description: "Name of the function to modify",
            },
            blueprint_thread_safe: {
              type: "boolean",
              description: "Whether the function is thread-safe (can be called from animation worker threads)",
            },
            blueprint_pure: {
              type: "boolean",
              description: "Whether the function is pure (no execution pins)",
            },
          },
          required: ["blueprint_path", "function_name"],
        },
      },
      {
        name: "capture_screenshot",
        description: "Capture a screenshot of the active Unreal Engine viewport",
        inputSchema: {
          type: "object",
          properties: {
            filename: {
              type: "string",
              description: "Optional filename (without extension). Defaults to 'MCP_Screenshot'. A timestamp will be appended automatically.",
            },
          },
        },
      },
      {
        name: "remove_error_nodes",
        description: "Automatically identify and remove nodes causing compilation errors in a blueprint. Useful for cleaning up after C++ conversions.",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
            auto_rewire: {
              type: "boolean",
              description: "Attempt to reconnect execution flow around deleted nodes (default: true)",
            },
          },
          required: ["blueprint_path"],
        },
      },
      {
        name: "clear_animation_blueprint_tags",
        description: "Remove AnimBlueprintExtension_Tag objects from an animation blueprint to fix 'cannot find referenced node with tag' errors. Use when tagged nodes have been removed but extensions still reference them. Use remove_extension: true to completely remove the tag extension (more aggressive fix).",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the animation blueprint asset",
            },
            remove_extension: {
              type: "boolean",
              description: "If true, completely removes the tag extension from the blueprint instead of just clearing its data. Use this for persistent tag errors that don't clear with the default behavior.",
            },
          },
          required: ["blueprint_path"],
        },
      },
      {
        name: "clear_anim_graph",
        description: "Delete all nodes from an animation blueprint's AnimGraph, leaving only the root output node. Use when rebuilding an AnimGraph from scratch or clearing corrupted graph state.",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the animation blueprint asset",
            },
          },
          required: ["blueprint_path"],
        },
      },
      // Sprint 1: Blueprint Function Creation Commands
      {
        name: "create_blueprint_function",
        description: "Create a new function in a blueprint with optional metadata flags (BlueprintThreadSafe, BlueprintPure, Const)",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
            function_name: {
              type: "string",
              description: "Name of the new function to create",
            },
            is_pure: {
              type: "boolean",
              description: "Whether the function is pure (no execution pins, returns value only). Default: false",
            },
            is_thread_safe: {
              type: "boolean",
              description: "Whether the function is thread-safe (can be called from animation worker threads). Default: false",
            },
            is_const: {
              type: "boolean",
              description: "Whether the function is const (doesn't modify object state). Default: false",
            },
          },
          required: ["blueprint_path", "function_name"],
        },
      },
      {
        name: "add_function_input",
        description: "Add an input parameter to an existing blueprint function",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
            function_name: {
              type: "string",
              description: "Name of the function to modify",
            },
            parameter_name: {
              type: "string",
              description: "Name of the input parameter to add",
            },
            parameter_type: {
              type: "string",
              description: "Type of the parameter (e.g., int, float, bool, string, FVector, FRotator, FTransform, or full path to object/struct type)",
            },
          },
          required: ["blueprint_path", "function_name", "parameter_name", "parameter_type"],
        },
      },
      {
        name: "add_function_output",
        description: "Add an output parameter (return value) to an existing blueprint function",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
            function_name: {
              type: "string",
              description: "Name of the function to modify",
            },
            parameter_name: {
              type: "string",
              description: "Name of the output parameter to add",
            },
            parameter_type: {
              type: "string",
              description: "Type of the parameter (e.g., int, float, bool, string, FVector, FRotator, FTransform, or full path to object/struct type)",
            },
          },
          required: ["blueprint_path", "function_name", "parameter_name", "parameter_type"],
        },
      },
      {
        name: "rename_blueprint_function",
        description: "Rename an existing blueprint function",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
            old_function_name: {
              type: "string",
              description: "Current name of the function",
            },
            new_function_name: {
              type: "string",
              description: "New name for the function",
            },
          },
          required: ["blueprint_path", "old_function_name", "new_function_name"],
        },
      },
      {
        name: "read_actor_properties",
        description: "Read all EditAnywhere properties from a level actor instance, including property overrides set in the Details panel. This returns ACTUAL working values from the level file (.umap), not Blueprint class defaults. Use this to: (1) Preserve actor configuration before blueprint reparenting, (2) Get correct reference values from a working project, (3) Read actual gameplay-tested property values. When copying reference data from another project, copy the level file (.umap) and use this command instead of read_class_defaults.",
        inputSchema: {
          type: "object",
          properties: {
            actor_name: {
              type: "string",
              description: "Name of the actor in the level (e.g., LevelVisuals_2, LevelBlock_3)",
            },
          },
          required: ["actor_name"],
        },
      },
      {
        name: "set_actor_properties",
        description: "Set EditAnywhere properties on a level actor instance. Use this to restore actor configuration after blueprint reparenting.",
        inputSchema: {
          type: "object",
          properties: {
            actor_name: {
              type: "string",
              description: "Name of the actor in the level (e.g., LevelVisuals_2, LevelBlock_3)",
            },
            properties: {
              type: "object",
              description: "Object containing property name-value pairs (as returned by read_actor_properties)",
            },
          },
          required: ["actor_name", "properties"],
        },
      },
      {
        name: "set_actor_component_property",
        description: "Set a property on a specific component of a level actor instance. Use for instance-specific component overrides (e.g., collision profile).",
        inputSchema: {
          type: "object",
          properties: {
            actor_name: {
              type: "string",
              description: "Name of the actor in the level (e.g., LevelBlock_Traversable_C_24)",
            },
            component_name: {
              type: "string",
              description: "Component name to modify (use read_actor_components to find names)",
            },
            property_name: {
              type: "string",
              description: "Property name to set (e.g., CollisionProfileName)",
            },
            property_value: {
              type: "string",
              description: "Value to set (as text, e.g., BlockAll)",
            },
          },
          required: ["actor_name", "component_name", "property_name", "property_value"],
        },
      },
      {
        name: "reconstruct_actor",
        description: "Trigger OnConstruction on a level actor by calling RerunConstructionScripts(). Use this after reparenting blueprints to C++ to apply C++ initialization logic (e.g., UpdateLevelVisuals, UpdateMaterials) to existing level instances.",
        inputSchema: {
          type: "object",
          properties: {
            actor_name: {
              type: "string",
              description: "Name of the actor in the level (e.g., LevelVisuals_C_6)",
            },
          },
          required: ["actor_name"],
        },
      },
      {
        name: "clear_component_map_value_array",
        description: "Clear an array property within an object stored in a component's map property. Use this to clear stale data from sub-objects (e.g., clear Transitions array in movement mode instances stored in CharacterMover's MovementModes map).",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
            component_name: {
              type: "string",
              description: "Name of the component (e.g., CharacterMover)",
            },
            map_property_name: {
              type: "string",
              description: "Name of the map property (e.g., MovementModes)",
            },
            map_key: {
              type: "string",
              description: "The key in the map (e.g., 'Walking', 'Sliding')",
            },
            array_property_name: {
              type: "string",
              description: "Name of the array property in the map value object (e.g., Transitions)",
            },
          },
          required: ["blueprint_path", "component_name", "map_property_name", "map_key", "array_property_name"],
        },
      },
      {
        name: "replace_component_class",
        description: "Replace a component's class with a different class (e.g., replace blueprint component class with C++ class). Use this to convert blueprint component references to C++ classes without losing the component configuration.",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
            component_name: {
              type: "string",
              description: "Name of the component to replace (e.g., AC_VisualOverrideManager, BP_VisualOverrideManager)",
            },
            new_class: {
              type: "string",
              description: "Full name of the new component class (e.g., AC_VisualOverrideManager for C++ class, or full path for blueprint class)",
            },
          },
          required: ["blueprint_path", "component_name", "new_class"],
        },
      },
      {
        name: "set_blueprint_cdo_property",
        description: "Set an object reference property on a blueprint's Class Default Object (CDO). Use this to set TObjectPtr properties like UInputAction or UInputMappingContext that are defined in C++ but need values assigned in the blueprint. This sets properties on the blueprint class itself, not on components.",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
            property_name: {
              type: "string",
              description: "Name of the property to set (e.g., IA_Sprint, IMC_Sandbox)",
            },
            property_value: {
              type: "string",
              description: "Full asset path of the object to assign (e.g., /Game/Input/IA_Sprint, /Game/Input/IMC_Sandbox)",
            },
          },
          required: ["blueprint_path", "property_name", "property_value"],
        },
      },
      // Sprint 5: Blueprint Node Manipulation
      {
        name: "connect_nodes",
        description: "Connect two nodes in a blueprint graph by wiring an output pin to an input pin. Use this to programmatically wire up blueprint logic. You must first use read_event_graph to get the node IDs and pin names.",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
            source_node_id: {
              type: "string",
              description: "GUID of the source node (from read_event_graph 'id' field)",
            },
            source_pin: {
              type: "string",
              description: "Name of the output pin on the source node (e.g., 'Triggered', 'ActionValue', 'ReturnValue')",
            },
            target_node_id: {
              type: "string",
              description: "GUID of the target node (from read_event_graph 'id' field)",
            },
            target_pin: {
              type: "string",
              description: "Name of the input pin on the target node (e.g., 'execute', 'Condition', 'Value')",
            },
            graph_name: {
              type: "string",
              description: "Name of the graph to modify (default: 'EventGraph'). Use for function graphs or other named graphs.",
            },
          },
          required: ["blueprint_path", "source_node_id", "source_pin", "target_node_id", "target_pin"],
        },
      },
      {
        name: "disconnect_pin",
        description: "Break all connections from/to a specific pin on a blueprint node. Use this to remove unwanted wiring before creating new connections.",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
            node_id: {
              type: "string",
              description: "GUID of the node (from read_event_graph 'id' field)",
            },
            pin_name: {
              type: "string",
              description: "Name of the pin to disconnect (e.g., 'Completed', 'ActionValue', 'WantsToSprint_1_xxx')",
            },
            graph_name: {
              type: "string",
              description: "Name of the graph (default: 'EventGraph')",
            },
          },
          required: ["blueprint_path", "node_id", "pin_name"],
        },
      },
      {
        name: "add_set_struct_node",
        description: "Add a new 'Set members in Struct' node to a blueprint event graph. Use this to create nodes that set struct field values.",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
            struct_type: {
              type: "string",
              description: "Name or path of the struct type (e.g., 'S_PlayerInputState' for blueprint structs)",
            },
            graph_name: {
              type: "string",
              description: "Name of the graph (default: 'EventGraph')",
            },
            x: {
              type: "integer",
              description: "X position for the node (optional)",
            },
            y: {
              type: "integer",
              description: "Y position for the node (optional)",
            },
            fields: {
              type: "array",
              items: { type: "string" },
              description: "Array of struct field names to expose as input pins on the node (e.g., ['WantsToSprint_1_840C190D4B23289C5C46E0B5A4C5C936']). Use read_user_defined_struct to get the full field names with GUIDs.",
            },
          },
          required: ["blueprint_path", "struct_type"],
        },
      },
      // Sprint 7: Struct Migration
      {
        name: "migrate_struct_references",
        description: "Migrate all blueprint references from a UserDefinedStruct (BP struct) to a C++ USTRUCT across all blueprints. Handles variable types, graph node pins (Break/Make/Set struct nodes), and GUID-to-clean field name remapping. Preserves pin connections by saving and rewiring after node reconstruction. Use dry_run=true to preview changes without modifying anything.",
        inputSchema: {
          type: "object",
          properties: {
            source_struct_path: {
              type: "string",
              description: "Full asset path to the BP UserDefinedStruct to migrate FROM (e.g., '/Game/Blueprints/Data/S_PlayerInputState')",
            },
            target_struct_path: {
              type: "string",
              description: "Path to the C++ UScriptStruct to migrate TO (e.g., '/Script/UETest1.S_PlayerInputState'). UE5 strips the F prefix from C++ struct names.",
            },
            dry_run: {
              type: "boolean",
              description: "If true, report what would change without modifying anything. Default: false",
            },
          },
          required: ["source_struct_path", "target_struct_path"],
        },
      },
      // Sprint 7b: Enum Migration
      {
        name: "migrate_enum_references",
        description: "Migrate all blueprint references from a UserDefinedEnum (BP enum) to a C++ UENUM across all blueprints. Handles variable types, function parameters, graph node pins, and enum literal nodes. Use dry_run=true to preview changes.",
        inputSchema: {
          type: "object",
          properties: {
            source_enum_path: {
              type: "string",
              description: "Full asset path to the BP UserDefinedEnum to migrate FROM (e.g., '/Game/Blueprints/Data/E_Gait')",
            },
            target_enum_path: {
              type: "string",
              description: "Path to the C++ UEnum to migrate TO (e.g., '/Script/UETest1.E_Gait')",
            },
            dry_run: {
              type: "boolean",
              description: "If true, report what would change without modifying anything. Default: false",
            },
          },
          required: ["source_enum_path", "target_enum_path"],
        },
      },
      {
        name: "fix_optional_struct_pin_defaults",
        description: "Fix invalid enum default values stored in optional struct pins (hidden SetFieldsInStruct/MakeStruct pins). Replaces NewEnumeratorN with the enum value names.",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
          },
          required: ["blueprint_path"],
        },
      },
      {
        name: "force_fix_enum_pin_defaults",
        description: "Force-fix enum pin defaults that still use NewEnumeratorN by remapping to the enum's value names, even when the enum subcategory object is missing.",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
            enum_path: {
              type: "string",
              description: "Full path to the enum (e.g., /Script/UETest1.E_TraversalActionType)",
            },
            pin_name_contains: {
              type: "string",
              description: "Optional: only fix pins whose names contain this substring (e.g., ActionType)",
            },
          },
          required: ["blueprint_path", "enum_path"],
        },
      },
      {
        name: "set_struct_field_default",
        description: "Set the default value for a field in a UserDefinedStruct (by field name or friendly name).",
        inputSchema: {
          type: "object",
          properties: {
            struct_path: {
              type: "string",
              description: "Full path to the UserDefinedStruct asset",
            },
            field_name: {
              type: "string",
              description: "Field VarName (with GUID) or friendly name",
            },
            new_default: {
              type: "string",
              description: "New default value string",
            },
          },
          required: ["struct_path", "field_name", "new_default"],
        },
      },
      {
        name: "clean_property_access_paths",
        description: "Remove invalid path segments (default: 'None') from K2Node_PropertyAccess Path arrays in a blueprint.",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
            remove_segment: {
              type: "string",
              description: "Segment value to remove from property access paths (default: 'None')",
            },
          },
          required: ["blueprint_path"],
        },
      },
      {
        name: "fix_property_access_paths",
        description: "Fix PropertyAccess paths after struct migration or invalid field references (e.g., Control Rig warnings).",
        inputSchema: {
          type: "object",
          properties: {
            blueprint_path: {
              type: "string",
              description: "Full path to the blueprint asset",
            },
          },
          required: ["blueprint_path"],
        },
      },
      // Sprint 6: Input System Reading
      {
        name: "read_input_mapping_context",
        description: "Read the contents of an Input Mapping Context asset, including all input action mappings with their keys, modifiers, and triggers. Use this to inspect what keys are bound to which input actions.",
        inputSchema: {
          type: "object",
          properties: {
            path: {
              type: "string",
              description: "Full path to the Input Mapping Context asset (e.g., '/Game/Input/IMC_Sandbox')",
            },
          },
          required: ["path"],
        },
      },
];
