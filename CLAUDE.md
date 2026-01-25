# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

See [AGENTS.md](./AGENTS.md) for project architecture, MCP integration, and technical documentation.

## Claude Code MCP Setup

Add to `~/.claude.json` under `mcpServers`:
```json
"unreal-engine": {
  "type": "stdio",
  "command": "node",
  "args": ["/Users/agusj/repo/unreal_engine_5/UETest1/Plugins/ClaudeUnrealMCP/MCPServer/index.js"],
  "env": {}
}
```
