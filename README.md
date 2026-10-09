# Unique/Set Scrapper

Unique/Set Scrapper is a D2RLoader client plugin that adds a native-feeling
Horadric Cube transmutation:

```text
1 unique or set item by itself -> 0, 1, 2, or 3 random runes
```

The plugin accepts any item whose runtime quality is Unique or Set. It accepts
one cube item only. By default each transmutation has a 30% chance to fail and
consume the input item without producing runes. On success, the output count
is 1 rune with 70% probability, 2 runes with 25%, or 3 runes with 5%. Each
output rune is selected independently, so duplicates are possible. The
default rune weights step down 20% at each rune from El through Zod, then are
normalized across all 33 runes. The default per-roll group odds are
El–Dol 95.66%, Hel–Ko 2.60%, Fal–Um 1.06%, Mal–Sur 0.58%, and Ber–Zod 0.09%;
Zod is about 0.0159%. Socket contents are consumed along with the input item.

All probabilities are configurable in
`d2rloader/config/unique-rune-cube.toml` under the active mod. The plugin
creates this file on first load. `failure-chance-percent` is from 0 to 100;
`output-count-weights.one`, `.two`, and `.three` are percentages that must sum
to 100; each key under `rune-weights` is a relative weight that is normalized
across the runes. Set a rune's weight to `0` to exclude it. Restart D2R after
editing the file.

It handles the Cube's `HoradricCubePanelMessage:Convert` event and performs the
exchange as one D2RLoader `ItemService` transaction on the game thread. If the
item changed after the Convert action was received, or if the transaction
cannot create and place every rune, the transaction leaves the input item
untouched. The plugin does not edit save files or `cubemain.txt` directly; the
game records the resulting inventory normally.

## Install

Build the DLL and copy `d2rl-unique-rune-cube.dll` to the active mod's
`d2rloader/plugins/` directory (or the loader's shared `d2rloader/plugins/`
directory). Restart D2R through D2RLoader. Edit the generated TOML file to
adjust probabilities, then restart again. Place exactly one unique or set item
in the Horadric Cube and press Transmute.

This is a client plugin and requires an authoritative local game or TCP/IP
host. Remote clients cannot perform the item transaction.

## Build

Requires Windows x64, CMake 3.29 or newer, Visual Studio 2022 or newer with the
Windows SDK, and D2RLoader PluginSDK ABI 4. CMake fetches the pinned SDK commit
`6eb8f8b6192868214706bd6d528c5294f2f551b7` on the first configure. To use a
local SDK checkout:

```powershell
cmake -S . -B ..\unique-rune-cube-build -A x64 `
  -DUNIQUE_RUNE_CUBE_LOCAL_SDK_DIR="<PluginSDK checkout>"
cmake --build ..\unique-rune-cube-build --config Release --target unique_rune_cube
```

The output is `../unique-rune-cube-build/Release/d2rl-unique-rune-cube.dll`.
The build fetches TOML++ v3.4.0. The manual build commands below need the
generated `default_config.hpp` from the CMake configure step and the TOML++
include directory, in addition to the SDK.
On this development host, MSBuild may fail because the process environment
contains both `PATH` and `Path`. In that case, open an x64 Visual Studio
Developer Command Prompt, then run the following from the project directory,
replacing `<SDK>`, `<TOML++>`, and `<BUILD>` with the local include roots and an
output directory outside this source tree:

```powershell
cl.exe /nologo /std:c++20 /W4 /permissive- /EHsc /utf-8 `
  /DWIN32_LEAN_AND_MEAN /DNOMINMAX /DUNICODE /D_UNICODE `
  /I"<SDK>\include" /I"<TOML++>\include" /I"<BUILD>\generated" `
  /c src\plugin.cpp /Fo"<BUILD>\plugin.obj"
rc.exe /nologo /I"<SDK>\include" /fo"<BUILD>\plugin.res" src\plugin.rc
link.exe /nologo /DLL /INCREMENTAL:NO /Brepro /MACHINE:X64 `
  /OUT:"<BUILD>\d2rl-unique-rune-cube.dll" `
  "<BUILD>\plugin.obj" "<BUILD>\plugin.res"
```

## Validation status

Tested with D2R 3.3.93847 and D2RLoader 1.3.1-beta (PluginSDK ABI 4) on
Windows x64. The user confirmed the unique and set item recipe works in-game
on v0.3.0. Version 0.3.1 changes the displayed name and author metadata; the
updated plugin loaded successfully. Socketed unique and set item behavior
remain unverified.

Third-party notices for the TOML++ and D2RLoader PluginSDK dependencies are in
[`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md). This repository does not
assign a license to the plugin's own source code.
