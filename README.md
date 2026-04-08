<!--
SPDX-FileCopyrightText: 2025-2026 Neptuwunium

SPDX-License-Identifier: EUPL-1.2
-->

# ![rivet icon](Resources/icon32.png) rivet_hook

rivet_hook is a multipurpose modding framework for Luna Engine, specializing on rift apart

check out the [develop branch](https://github.com/neptuwunium/rivet/tree/develop) for research.

## Notice

This project is not authorized, affiliated or endorsed by Sony Interactive Entertainment LLC, or Insomniac Games.

"Ratchet and Clank", and "Ratchet and Clank: Rift Apart" are registered trademarks or trademarks of Sony Interactive Entertainment LLC.

## Attribution

This project is in part possible due to research and code written and provided by several other talented programmers.

- [macton/DDLParser](https://github.com/macton/DDLParser)

## Features

- asset replacement without modifying archives
- ddl and asset manager version dumping
- disable crash handler (crs-handler.exe)
- re-attach internal engine logger
- automatic renderdoc attaching

### Future

I am currently investigating if the following is possible in the engine;

- in-game actor viewer
- hot reloading of assets
- no-clip
- freecam

### Caveats

Due to how texture streaming is handled in Rift Apart, DirectStorage is disabled when enabling the mod loader.

At the moment only Rift Apart has full support.
