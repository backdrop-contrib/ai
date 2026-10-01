# AI MCP Server

Exposes `ai_tools` as an MCP-compatible HTTP endpoint for the Backdrop CMS AI module. Allows external MCP clients (such as Claude Desktop or other AI assistants) to discover and call registered AI tools on your site.

## Requirements

- `ai` module
- `ai_tools` module

## Installation

- Install this module using the official [Backdrop CMS instructions](https://backdropcms.org/user-guide/modules).

## Configuration

1. Go to **Administration > Configuration > Services > MCP** (`admin/config/services/mcp`).
2. Configure the API key and any access restrictions for the MCP endpoint.
3. Point your MCP client at the endpoint URL shown on the configuration page.

### Write tools are off by default

MCP has no approval step on the site: a tool call runs as soon as the client
sends it, and any confirmation is left to the client. Each server therefore
exposes only read-only tools unless **Allow write tools** is checked. With it
off, write tools (marked *(writes)* in the tool list) are hidden from
`tools/list` and refused by `tools/call`. A client misled by instructions
hidden in content it read, such as text in an image, then cannot change the
site through this server.

A tool counts as a write unless it declares `operation` as `read`, `explain`,
`transform` or `propose`, and is not marked `destructive`.

## Issues

Bugs and feature requests should be reported in the [Issue Queue](https://github.com/backdrop-contrib/ai_mcp/issues).

## Current Maintainer

[Justin Keiser](https://github.com/keiserjb)

## Credits

- Created for Backdrop CMS by [Justin Keiser](https://github.com/keiserjb).
- Developed with AI assistance.

## License

This project is GPL v2 software. See the LICENSE.txt file in this directory for complete text.
