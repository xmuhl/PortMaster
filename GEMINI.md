# Gemini Project Context: PortMaster

This document provides a comprehensive overview of the PortMaster project to guide future development and analysis.

## 1. Project Overview

- **Project Name:** PortMaster (端口大师)
- **Type:** C++ MFC Desktop Application for Windows (Win7-Win11, x86/x64).
- **Core Purpose:** A multi-port data transfer and testing utility designed to validate data integrity for hardware devices like printers.
- **Key Technologies:** C++, MFC, Visual Studio 2022 (v143 toolset).

### Core Features

- **Multi-Port Support:**
  - Serial (COM)
  - Parallel (LPT)
  - USB Print
  - Network Print (RAW 9100, LPR, IPP)
  - Virtual Loopback (for local testing)
- **Transfer Modes:**
  - **Direct Mode:** Raw data read/write operations.
  - **Reliable Mode:** Implements a custom sliding-window protocol with ACK/NAK, retransmissions, and CRC32 checks to ensure data integrity.
- **User Interface:**
  - A single-dialog MFC interface for all operations.
  - Port configuration, connection management.
  - Data input via manual entry or file drag-and-drop.
  - Synchronized Hex/Text view for sent and received data.
  - Real-time statistics (speed, byte counts, errors).
  - Operation and event logging.
- **Configuration:**
  - All settings (port parameters, UI state, recent files) are persisted in a `PortMaster.json` file located either in the application directory or `%LOCALAPPDATA%`.

### Architecture

The application follows a clean, layered architecture:

1.  **UI Layer (`PortMasterDlg`)**: Handles all user interaction, displays data, and delegates operations.
2.  **Protocol Layer (`ReliableChannel`)**: Manages the reliable transfer protocol, including framing, windowing, and state management. This layer is used when "Reliable Mode" is active.
3.  **Transport Layer (`ITransport`)**: An abstract interface defining core communication operations (`Open`, `Close`, `Read`, `Write`).
    - **Implementations**: `SerialTransport`, `ParallelTransport`, `UsbPrintTransport`, `NetworkPrintTransport`, and `LoopbackTransport` provide concrete implementations for each port type.
4.  **Common Utilities**:
    - `TransportFactory`: Creates transport instances based on type.
    - `ConfigStore`: A singleton that manages loading/saving the JSON configuration.
    - `FrameCodec`: Handles the serialization and deserialization of frames for the reliable protocol.

### Current Status

Analysis of project documents (`docs/PortMaster_代码全量分析报告.md`) reveals that the project is under active development and some key features are incomplete or buggy. High-priority tasks include:
- Correctly implementing the reliable protocol's handshake and state machine.
- Fixing hardware-specific timeout and status-checking logic.
- Ensuring all port configurations are correctly saved and loaded.

## 2. Building and Running

### Building the Project

- **Environment:** Visual Studio 2022 with the C++/MFC toolset (v143).
- **Primary Build Script:** Use `autobuild_x86_debug.bat` to build the Win32 Debug configuration. This is the preferred script for development iterations.
- **Full Build Script:** `autobuild.bat` builds all four configurations (Debug/Release for Win32/x64).
- **Process:** The scripts use `msbuild.exe` to build the `PortMaster.sln` solution. The goal is to produce a single, statically-linked executable with zero build errors or warnings.
- **Command:**
  ```shell
  # For a standard debug build
  ./autobuild_x86_debug.bat

  # For a full build of all targets
  ./autobuild.bat
  ```

### Running the Application

- The output is a single executable file (e.g., `PortMaster.exe`).
- Run the executable from the corresponding build output directory (e.g., `build\Debug\`).

## 3. Development Conventions

- **Language:** All source code, comments, documentation, and UI text are in **Simplified Chinese**.
- **File Encoding:** All text-based files must be **UTF-8 with BOM**. The pragma `#pragma execution_character_set("utf-8")` is used in source files.
- **Workflow:** The project follows a strict workflow documented in `PortMaster 项目开发.md` and `任务要求.md`. This includes:
    1.  Creating a markdown work log (`修订工作记录*.md`) before starting.
    2.  Building locally with `autobuild_x86_debug.bat` and ensuring no errors/warnings.
    3.  Committing changes with a conventional commit message format (e.g., `fix: ...`, `feat: ...`).
- **Code Style:** The code follows standard C++/MFC conventions, including PascalCase for classes/methods and the use of `CString`.
