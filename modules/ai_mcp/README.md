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

## Issues

Bugs and feature requests should be reported in the [Issue Queue](https://github.com/backdrop-contrib/ai_mcp/issues).

## Current Maintainer

[Justin Keiser](https://github.com/keiserjb)

## Credits

- Created for Backdrop CMS by [Justin Keiser](https://github.com/keiserjb).
- Developed with AI assistance.

## License

This project is GPL v2 software. See the LICENSE.txt file in this directory for complete text.
