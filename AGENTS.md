# Repository Guidelines

## Project Structure & Modules
- Solution/UI: `PortMaster.sln`, `PortMaster.cpp`, `PortMasterDlg.*` in repo root.
- Core libraries:
  - `Common/` (DeviceManager, ConfigManager, RingBuffer, AsyncMessageManager, etc.)
  - `Protocol/` (CRC32, FrameCodec, ReliableChannel, ProtocolManager)
  - `Transport/` (TcpTransport, UdpTransport, SerialTransport, UsbPrinterTransport, LoopbackTransport)
- Assets: `res/` (RC resources), `PortMaster.rc`, icons; images under `image/`.
- Scripts: `autobuild*.bat`, `scripts/*.ps1`, `wsl_auto_workflow*.sh`.
- Tests: example `sliding_window_test.cpp`; add new tests as `*_test.cpp`.

## Build, Run, and Test
- Use Visual Studio 2019/2022 with Windows 10 SDK.
- Fast builds:
  - `./autobuild.bat` — default solution build.
  - `./autobuild_x86_debug.bat` — 32-bit Debug build.
- MSBuild examples:
  - `msbuild PortMaster.sln /m /p:Configuration=Debug;Platform=x64`
  - `devenv PortMaster.sln /Build Release /Project PortMaster`
- Run app: `x64/Debug/PortMaster.exe` or `x64/Release/PortMaster.exe` (path depends on config).
- Quick test compile (example):
  - `cl /std:c++17 /EHsc /I Common /I Protocol sliding_window_test.cpp /Fe:sliding_window_test.exe && .\\sliding_window_test.exe`

## Coding Style & Naming
- C++17, Windows; 4-space indent, CRLF, UTF-8.
- Files mirror types: `Foo.h`/`Foo.cpp` (e.g., `Common/DeviceManager.h/.cpp`).
- Classes/types: PascalCase. Functions/variables: camelCase. Constants: kCamelCase (e.g., `kMaxFrameSize`).
- Prefer RAII and smart pointers; add `#pragma once` in headers.
- Keep headers lean; prefer forward declarations in headers and includes in `.cpp`.
- Use VS "Format Document"; `clang-format` optional (no repo config yet). Avoid drive-by reformatting.

## Testing Guidelines
- Place tests as `*_test.cpp` next to code or under `tests/`.
- Keep tests deterministic; focus on `Protocol/` edges and `Transport/` I/O boundaries.
- Ensure Debug and Release builds pass locally before PRs.

## Commit & Pull Request Guidelines
- Commits: `<Module>: imperative summary` (e.g., `Protocol: fix CRC32 table`), with brief body and issue refs (e.g., `Refs #123`). English or Chinese is fine.
- PRs include: clear description, linked issues, screenshots/GIFs for UI, test notes, and any doc updates.
- Do not commit build outputs (`x64/`, `.vs/`) or secrets. Use `ConfigManager` and local ignored files for environment-specific settings.

