PortMaster Interaction Code Analysis (2025-09-22)
=================================================

Scope
-----
- Reviewed UI interaction flow, encoding helpers, and transport glue in src/PortMasterDlg.cpp with supporting resources/headers to validate control wiring.
- Spot-checked configuration and transport utilities for behaviours tied to hex mode, encoding, and user-triggered operations.

Key Findings
------------
1. Hex mode pipeline is not reversible and blocks sending.
   - src/PortMasterDlg.cpp:414-438 only strips spaces/CR/LF from the edit buffer before parsing; the formatter leaves offsets such as "00000000:" and ASCII gutters like "|...|", so _stscanf_s fails and surfaces the reported "hex data format error" message.
   - src/PortMasterDlg.cpp:751-789 reuses the formatted view when toggling modes; HexToString never removes colons, pipes, or offsets, so text-mode restores are corrupted or empty.
   - src/PortMasterDlg.cpp:856-865 strips spaces but not newlines or formatting markers; parsing silently drops or mangles bytes.

2. Unicode data is truncated throughout the hex/text conversions.
   - src/PortMasterDlg.cpp:798-808 casts each UTF-16 code unit to BYTE, discarding the high byte for Chinese characters and producing the wrong ASCII gutter.
   - src/PortMasterDlg.cpp:863-865 writes those low bytes back into a TCHAR, so any non-ASCII data reappears garbled.
   - src/PortMasterDlg.cpp:445 sends text via CT2A without an explicit code page, so behaviour depends on the local ANSI locale.
   - src/PortMasterDlg.cpp:1429 builds the receive window text with CString(reinterpret_cast<const char*>(...)), truncating on embedded NULs and mis-decoding UTF-8.

3. File import encoding fallback is unsafe.
   - src/PortMasterDlg.cpp:553-599 allocates raw bytes with new BYTE[(size_t)fileLength + 2] and, on UTF-8 decode failure, reinterprets them as LPCTSTR; under Unicode builds this treats 8-bit data as wchar_t*, which matches the reported Chinese corruption and risks reading past the buffer terminator.

4. Several interaction handlers are stubs or miswired.
   - src/PortMasterDlg.cpp:523-524 the Stop button only shows a message box; it never cancels outstanding writes or threads.
   - src/PortMasterDlg.cpp:872-877 and src/PortMasterDlg.cpp:997-1005 toggle radio state text but skip reconfiguring transports or persisting the choice.
   - src/PortMasterDlg.cpp:490-499 reads/writes IDC_EDIT_SEND_HISTORY and clears IDC_EDIT_SEND, neither of which exist in resources/PortMaster.rc, so sent data is not cleared and history logging is a no-op.

5. Error handling and resilience gaps remain.
   - src/PortMasterDlg.cpp:1368-1376 posts heap allocations across threads without checking PostMessage success; a full message queue leaks memory.
   - src/PortMasterDlg.cpp:553-560 loads entire files into memory with no size cap, allowing untrusted drops to exhaust memory (DoS risk).
   - Common/ConfigStore.cpp:877-915 relies on a hand-written string parser for JSON; crafted values with escaped quotes or nested objects break deserialization and could overwrite config fields unexpectedly.

Security Considerations
-----------------------
- Unbounded file imports coupled with naive encoding conversion (src/PortMasterDlg.cpp:553-599) let untrusted inputs drive large allocations or trigger decode failures that surface as UI freezes.
- Manual JSON parsing in Common/ConfigStore.cpp:877-915 lacks validation/escaping and should be replaced with a vetted parser to avoid malformed-config injection.

Recommended Next Steps
----------------------
1. Redesign the hex/text conversion path to operate on byte buffers, emit plain byte pairs, and accept formatted input safely.
2. Introduce UTF-8 (or user-selectable) conversions for send/receive paths and file I/O; remove raw CT2A/CString(char*) usage.
3. Fix miswired controls and implement missing handlers (stop flow, mode toggles, send-history logging) before toggling features on.
4. Add defensive limits and error reporting for file drops/imports and replace the JSON parser with a hardened alternative.
