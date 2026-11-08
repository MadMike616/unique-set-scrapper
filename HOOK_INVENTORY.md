# Hook Inventory — Unique/Set Scrapper

## Plugin metadata

| Field | Value |
|---|---|
| Plugin name | Unique/Set Scrapper |
| Plugin ID | unique-rune-cube |
| Release version | 0.3.2 |
| Inventory updated | 2026-11-08 |
| Source revision | Tag v0.3.2 (release commit) |
| D2R build(s) audited | D2R 3.3.93847 (user validation reported for v0.3.1; v0.3.2 not validated) |
| D2RLoader SDK / Plugin ABI | PluginSDK 0.4.0 / Plugin ABI 4 |
| Audit status | Complete |

## Scope and method

Inspected the complete plugin source tree, including `src/plugin.cpp`, `src/plugin.rc`, `src/default_config.hpp.in`, `CMakeLists.txt`, the TOML template, and the Release workflow. Reviewed native-memory, detour, patch, vtable, shared-slot, and Win32 input/message hook installation patterns. The plugin uses D2RLoader lifecycle, ItemService, InventoryService, ThreadService, and SharedEventService callbacks; these SDK callbacks do not install or replace a native or operating-system target.

## Hook sites

| Site ID | Type | Target | Address / identifier | Operation | Guard / validation | Source reference | Notes |
|---|---|---|---|---|---|---|---|

No direct hook sites found in the audit.

## Known collision overlaps

| Other plugin | Shared target | Relationship | Evidence / notes |
|---|---|---|---|

None identified in the current inventory; this is not a compatibility guarantee.

## Audit notes

No hook-site changes from v0.3.1. The v0.3.2 release changes version metadata and rebuilds in Release configuration. In-game validation of this exact release remains pending.
