## StrataCopy v1.0.0

The first official release of StrataCopy: a compact Windows x64 copy utility built with C++20 and Qt, powered by the system Robocopy engine.

### Included

- Compact dark copy window with expandable activity, speed graph, pause/resume and cancellation. Closes after a successful transfer by default; choose Keep open in Details to change it.
- Per-file conflict prompts with **Replace Existing File**, **Skip This File**, and **Make Duplicate**, including nested folder conflicts. Duplicate names are numbered without replacing the original.
- Optional Explorer handling for keyboard Paste, native Paste menus and filesystem drag-and-drop. Switch back to the Windows copier with the checkbox in Settings.
- Copy, move and mirror profiles; threading, retries, filters and existing-file policies; saved presets and command/script export.
- Optional XXH3-64, BLAKE3 or SHA-256 readback verification, durable queue recovery, transfer history and complete logs.
- Original StrataCopy icon and modern per-user installer.
- **Check for Updates** compares against GitHub's latest stable version. It confirms when already current and opens Releases only when an update is available.

### Downloads

Download **StrataCopy-1.0.0-Setup-x64.exe** below. The source and build instructions are in the public repository. GitHub displays the installer's SHA-256 digest beside the download.

### Validation and limits

All **34 engine/native checks passed with zero failures or skips**, including real transfers, all conflict choices, update-check errors/timeouts, the app-to-Robocopy handoff and resource-growth checks. Installer validation covers install, upgrade, optional integration and uninstall cleanup.

Explorer integration handles normal filesystem operations; unsupported or virtual operations use Windows. Intercepted transfers do not create Explorer Undo entries. Failed duplicate publication retains staged data and reports its recovery path. Progress and ETA are estimates; cancellation does not undo completed copies.

See the README for setup, integration details and build instructions.
