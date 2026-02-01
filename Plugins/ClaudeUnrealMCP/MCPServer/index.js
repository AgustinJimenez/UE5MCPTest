#!/usr/bin/env node

import { Server } from "@modelcontextprotocol/sdk/server/index.js";
import { StdioServerTransport } from "@modelcontextprotocol/sdk/server/stdio.js";
import {
  CallToolRequestSchema,
  ListToolsRequestSchema,
} from "@modelcontextprotocol/sdk/types.js";
import * as net from "net";

const UE_HOST = process.env.UE_HOST || "127.0.0.1";
const UE_PORT = parseInt(process.env.UE_PORT || "9877");

// Send command to UE5 TCP server
async function sendToUnreal(command, params = {}) {
  return new Promise((resolve, reject) => {
    const client = new net.Socket();
    let data = "";

    // Increase socket buffer size for large responses
    client.setNoDelay(true); // Disable Nagle's algorithm
    client.setTimeout(30000); // Increase timeout to 30 seconds

    client.connect(UE_PORT, UE_HOST, () => {
      const request = JSON.stringify({ command, params });
      client.write(request);
    });

    client.on("data", (chunk) => {
      data += chunk.toString("utf8");
      // Only try to parse when we have a complete message (ends with newline)
      if (data.endsWith("\n")) {
        client.destroy();
        try {
          const trimmed = data.trim();
          console.error(`Received ${trimmed.length} bytes from Unreal Engine`);
          resolve(JSON.parse(trimmed));
        } catch (e) {
          const preview = data.length > 1000 ? data.substring(0, 1000) + "... (truncated for display)" : data;
          reject(new Error(`Invalid JSON response (${data.length} bytes): ${preview}`));
        }
      }
    });

    client.on("timeout", () => {
      client.destroy();
      reject(new Error("Connection timeout"));
    });

    client.on("error", (err) => {
      reject(new Error(`Connection error: ${err.message}`));
    });

    client.on("close", () => {
      if (data && !data.includes("\n")) {
        try {
          resolve(JSON.parse(data.trim()));
        } catch (e) {
          // Already handled
        }
      }
    });
  });
}

// Create MCP server
const server = new Server(
  {
    name: "claude-unreal-mcp",
    version: "1.0.0",
  },
  {
    capabilities: {
      tools: {},
    },
  }
);

// List available tools
server.setRequestHandler(ListToolsRequestSchema, async () => {
  return {
    tools: [
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
        description: "Read class default object (CDO) properties from a blueprint, including inherited properties like Default Pawn Class from GameMode",
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
        description: "Remove AnimBlueprintExtension_Tag objects from an animation blueprint to fix 'cannot find referenced node with tag' errors. Use when tagged nodes have been removed but extensions still reference them.",
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
        description: "Read all EditAnywhere properties from a level actor instance. Use this to preserve actor configuration before blueprint reparenting.",
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
    ],
  };
});

// Handle tool calls
server.setRequestHandler(CallToolRequestSchema, async (request) => {
  const { name, arguments: args } = request.params;

  // Handle reload_mcp_server specially (doesn't go to Unreal)
  if (name === "reload_mcp_server") {
    console.error("MCP Server: Reloading...");
    // Exit cleanly - Claude Code will restart the server
    process.exit(0);
  }

  try {
    const result = await sendToUnreal(name, args || {});

    if (result.success) {
      return {
        content: [
          {
            type: "text",
            text: JSON.stringify(result.data, null, 2),
          },
        ],
      };
    } else {
      return {
        content: [
          {
            type: "text",
            text: `Error: ${result.error}`,
          },
        ],
        isError: true,
      };
    }
  } catch (error) {
    return {
      content: [
        {
          type: "text",
          text: `Failed to communicate with Unreal Engine: ${error.message}. Make sure the editor is running with the ClaudeUnrealMCP plugin enabled.`,
        },
      ],
      isError: true,
    };
  }
});

// Start server
async function main() {
  const transport = new StdioServerTransport();
  await server.connect(transport);
  console.error("Claude Unreal MCP server started");
}

main().catch(console.error);
