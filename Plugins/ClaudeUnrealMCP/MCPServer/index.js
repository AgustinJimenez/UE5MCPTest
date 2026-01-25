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

    client.setTimeout(10000);

    client.connect(UE_PORT, UE_HOST, () => {
      const request = JSON.stringify({ command, params });
      client.write(request);
    });

    client.on("data", (chunk) => {
      data += chunk.toString();
      if (data.includes("\n")) {
        client.destroy();
        try {
          resolve(JSON.parse(data.trim()));
        } catch (e) {
          reject(new Error(`Invalid JSON response: ${data}`));
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
        name: "list_actors",
        description: "List all actors in the current level",
        inputSchema: {
          type: "object",
          properties: {},
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
    ],
  };
});

// Handle tool calls
server.setRequestHandler(CallToolRequestSchema, async (request) => {
  const { name, arguments: args } = request.params;

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
