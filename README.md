# Unique/Set Scrapper

Unique/Set Scrapper is a D2RLoader client plugin that adds a Horadric Cube
transmutation:

```text
1 unique or set item by itself -> 0, 1, 2, or 3 random runes
```

The plugin accepts any item whose runtime quality is Unique or Set, and accepts
one cube item only. Each attempt has a configurable chance to consume the input
without producing a rune. When it succeeds, the configured output-count weights
choose 1, 2, or 3 runes. Each rune is selected independently, so duplicates are
possible. Socket contents are consumed with the input item.

## Probabilities

All probabilities are configurable in
`d2rloader/config/unique-rune-cube.toml` under the active mod. The default
failure chance is 30%. Successful transmutations produce 1 rune 70% of the
time, 2 runes 25% of the time, and 3 runes 5% of the time. Default rune weights
step down 20% at each rune from El through Zod, then normalize across all 33
runes. Set a rune weight to `0` to exclude it.

`failure-chance-percent` accepts values from 0 to 100. The output-count weights
must sum to 100. Rune weights are relative values. Restart D2R after editing
the configuration.

## Install

Copy `d2rl-unique-rune-cube.dll` to the active mod's
`d2rloader/plugins/` directory (or the loader's shared
`d2rloader/plugins/` directory). Copy `unique-rune-cube.toml` to that mod's
`d2rloader/config/` directory, then start D2R through D2RLoader. Place one
unique or set item in the Horadric Cube and press Transmute.

The plugin requires an authoritative local game or TCP/IP host. Remote clients
cannot perform the item transaction.

## Compatibility

Windows x64 · D2R 3.3.93847 · D2RLoader 1.3.1-beta · PluginSDK 0.4.0 / ABI 4

The v0.3.0 recipe was confirmed in-game by the user; v0.3.1 metadata was also
confirmed to load. The v0.3.2 Release build must be validated in-game by
restarting D2RLoader and checking the plugin loads before using the recipe.

## Build

Requires Windows x64, CMake 3.29 or newer, Visual Studio 2022 or newer with the
Windows SDK, D2RLoader PluginSDK ABI 4, and TOML++ v3.4.0. CMake fetches the
pinned PluginSDK revision and TOML++ on first configure.

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release --target unique_rune_cube
```

The Release DLL is `build/Release/d2rl-unique-rune-cube.dll`. The
`Release build` workflow builds with CMake's Release configuration and
publishes the versioned GitHub release with the DLL and TOML template.

## License

This plugin is licensed under the MIT License. See [LICENSE](LICENSE).
Third-party dependency notices are in
[`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).
