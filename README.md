<div align="center">
  <img src="resources/icons/stratacopy.png" width="112" alt="StrataCopy logo">
  <h1>StrataCopy</h1>
  <p><strong>A compact Windows copy utility. Powered by Robocopy.</strong></p>
  <p>Modern controls, native Explorer integration, and transfer settings you can inspect.</p>
  <p>
    <img alt="Version 2.0.0" src="https://img.shields.io/badge/version-2.0.0-64b5ff">
    <img alt="Windows x64" src="https://img.shields.io/badge/platform-Windows_x64-0078D4">
    <img alt="C++20" src="https://img.shields.io/badge/C%2B%2B-20-00599C">
    <img alt="Qt 6.8.3" src="https://img.shields.io/badge/Qt-6.8.3-41CD52">
  </p>
  <p><a href="https://github.com/SheedaMusalli/StrataCopy/releases/latest"><strong>Download latest release</strong></a> · <a href="#getting-started">Getting started</a> · <a href="#explorer-integration">Explorer integration</a> · <a href="#building-from-source">Build from source</a></p>
</div>

---

## What is StrataCopy?

StrataCopy brings Windows' built-in Robocopy engine into a small desktop interface. Add files or folders, pick a destination, and copy. Expand Details when you need the activity list and completion actions. Recent transfers appear on the main screen.

**v2.0.0 is the current release build.** Copy, move, mirror and dry-run operations run through the system `robocopy.exe`. StrataCopy manages the queue, process lifecycle, interface and optional readback verification. This version adds a handler choice for Explorer actions, concurrent Explorer transfers, elevated retries and faster same-volume moves.

## Features

| Area | Included in v2.0.0 |
| --- | --- |
| Compact interface | Compact dark copy window; expandable details; original icon; native move/resize and keep-on-top |
| Explorer integration | Optional handling of Ctrl+C/Ctrl+X → Ctrl+V, native Paste menus, command-bar Paste, and filesystem drag-and-drop |
| Transfer control | Pause, resume, cancel, estimated progress and speed graph; closes after success by default |
| Robocopy settings | Copy/move/mirror; existing-file rules; threading, retries, attributes, access, links, size, age and exclusions |
| Integrity checks | Source-only checksum generation and optional XXH3-64, BLAKE3 or SHA-256 readback verification, with optional sidecars |
| Recovery and activity | Searchable bounded activity list, complete logs, SQLite queue recovery and 100 recent runs |
| Reusable setup | Built-in profiles, saved presets and persistent interface preferences |
| Distribution | Administrator-approved Windows x64 installer in Program Files; buildable source in this repository |

## Getting started

### Installer

1. Download **StrataCopy-2.0.0-Setup-x64.exe** from [Releases](https://github.com/SheedaMusalli/StrataCopy/releases/latest).
2. Run the installer and approve the Windows administrator prompt. It installs StrataCopy under Program Files. The app itself runs normally as your account; transfers that require protected access can be retried as administrator.
3. Choose whether to create a desktop shortcut. Explorer copy and move integration is enabled by default on a fresh install.
4. Open StrataCopy, add files or folders, select a destination, and press **Copy**.

### Verify downloads

GitHub displays a SHA-256 digest beside the installer asset. Compare it with:

```powershell
Get-FileHash .\StrataCopy-2.0.0-Setup-x64.exe -Algorithm SHA256
```

## Using the copy window

**+ Files** and **+ Folder** add sources; dropping items onto the window also adds them. **Browse** chooses the destination. A selected source folder retains its name at the destination; loose files go directly into the destination folder.

**Details** opens the activity list, filters and completion actions. **Settings** includes compact/detailed startup layout, pinning, completion defaults, Explorer integration and transfer parameters. Drop additional files during a transfer to queue them for the same destination. The source list clears and transfer mode returns to All subfolders, including empty after each run.

The default **After copying** action is **Close window**, which runs only after a successful transfer. Choose **Keep open** when you want to inspect the finished activity. A window opened from the tray stays open by default. Sleep and eject actions ask for confirmation. Failed or cancelled transfers remain open.

Copy and move start without a command confirmation. Mirror asks before deleting destination extras. Existing-file decisions include a Do this for all conflicts checkbox, scoped to one transfer. Cancel stops further work but does not roll back completed copies.

### Existing files

The default **Ask for each existing file** mode pauses on filename conflicts, including files inside merged folders. The dialog shows the incoming and existing paths, sizes and dates, with three choices:

- **Replace Existing File** copies the incoming file over that destination file.
- **Skip This File** preserves the existing destination and leaves the source untouched.
- **Make Duplicate** preserves the existing destination and creates a numbered incoming copy, such as `report (2).txt`. Occupied numbers are skipped without overwriting them.

Cancel stops the remaining transfer. Source contents are copied by Robocopy; duplicate naming uses an atomic rename within the destination filesystem. If a duplicate cannot be published after copying/moving, its staged data is retained and the recovery path is reported. Settings → **Existing files** also offers explicit automatic policies: delta copy, keeping newer destination files, skipping existing files, or overwriting without individual prompts.

## Explorer integration

Enable **Settings → Use StrataCopy for Explorer copy and move**. Uncheck it to restore Windows copying for subsequent Explorer operations. The choice persists across sign-ins and installer upgrades. Separate **Copy with StrataCopy** and **Paste with StrataCopy** actions can be added under Windows 11's **Show more options** menu.

Native Copy and Cut still populate Explorer's clipboard. The helper loads `StrataCopyNative-2.0.0-p5.dll` into x64 Explorer using thread-specific message hooks. The DLL intercepts normal `IFileOperation` copy/move requests and queues them for StrataCopy. No Windows system files are replaced, and there is no global keyboard hook.

Requests are saved atomically before handoff. Each Explorer copy or move can run independently, so starting another transfer while one is active opens a separate StrataCopy window. Each window reports its own progress and can be cancelled without stopping other operations. Files dropped into the main window during a transfer remain queued for that window's next run. Windows handles all delete and recycle actions.

### Integration boundaries

- Normal filesystem copy/move is supported. Phones, ZIP namespaces and other virtual shell items use Windows.
- Unsupported changed target names, same-folder duplicate naming, reparse-point sources, overlapping trees, mixed batches, multiple destinations and over-limit requests use Windows.
- Other apps and privileged helpers may use different copy paths. This does not replace every file operation in every application.
- Intercepted transfers do not create Explorer Undo entries. Review saved queue/history after a crash.
- Disabling stops interception immediately. Native modules stay loaded until Explorer exits to avoid unloading code still in use. Upgrades or uninstall may require sign-out/restart to remove a locked module.

## Updates

**Settings → Check for Updates** requests GitHub's latest stable release and compares its version with the installed app. If current, StrataCopy displays an up-to-date message. If a newer version exists, it opens [the latest release](https://github.com/SheedaMusalli/StrataCopy/releases/latest) in your browser. Connection errors and rate limits are reported separately. Checking is asynchronous, has a 15-second timeout, and does not silently install software.

## Building from source

The scripts use Qt 6.8.3 and MinGW 13.1 in the workspace-local `.tools` directory. On a fresh checkout, provide Python 3.10+:

```powershell
.\scripts\bootstrap.ps1 -Python 'C:\path\to\python.exe'
.\scripts\build.ps1 -Deploy
```

Alternatively use CMake 3.24+ with matching Qt/compiler installations. Required Qt components: Core, Gui, Quick, QuickControls2, ShaderTools, Network, Sql, Concurrent and Test. The renderer probe uses version-specific `Qt6::GuiPrivate` headers; build and deploy against matching Qt libraries. Set `QT_ROOT` for an alternate Qt installation. An MSVC build requires an MSVC Qt kit.

For the installer, install Inno Setup at `.tools/InnoSetup/ISCC.exe` or make it available on PATH:

```powershell
.\scripts\build-installer.ps1 -SkipBuild
.\scripts\test-installer.ps1
.\scripts\package.ps1 -SkipBuild
```

The default build runs both suites and rejects skipped tests. Stop the Explorer helper before testing the actual app handoff. Timed UI QA controls are compiled out of production; `-UiTestHooks` enables them for a diagnostic build.

### Source layout

```text
src/Core/          Commands, process supervision, scanning and telemetry
src/Platform/      Windows integration, resource ownership and Explorer hook
src/UI/            Qt Quick interface, models and coordinator
src/Persistence/   SQLite queue, presets and history
src/Verification/  Post-copy hashing
resources/         Original artwork, Windows resources and shaders
tests/             Engine, coordinator and native integration checks
installer/         Inno Setup project
scripts/           Bootstrap, build, packaging and installer validation
third_party/       Vendored dependencies and license notices
```

## Memory management and validation

Native handles and retained COM arguments use RAII. Shutdown joins worker threads; process jobs terminate owned children. Queue/model budgets limit retained activity, while complete output streams to disk. Graphs retain 240 samples, incomplete parser lines are capped, and hashing reuses scoped buffers.

The v2.0.0 engine suite passed 27 checks. A native Explorer test run had failures and its live handoff was skipped while an Explorer helper was active; Explorer integration needs further validation. Resource-growth checks are not a full heap-profiler proof of zero leaks. See [validation](VALIDATION.md) and [ownership review](MEMORY_REVIEW.md).

## Data and diagnostics

The default journal and logs are under `%LOCALAPPDATA%/StrataCopy/StrataCopy/`. The UI diagnostic log is `%TEMP%/stratacopy-ui.log`. History timestamps use UTC.

| Variable | Purpose |
| --- | --- |
| `STRATACOPY_DATA_DIR` | Override journal and request location |
| `STRATACOPY_DIAGNOSTICS` | Override the UI diagnostic log |
| `STRATACOPY_RENDERER` | Select a renderer, such as `d3d11` or `software` |
| `STRATACOPY_EXPLORER_TRACE=1` | Enable bounded diagnostics when starting the helper |

## Known limitations

Progress, ETA and process-I/O rates are estimates. Activity classification recognizes English Robocopy labels; other languages retain raw logs. Source reparse points are excluded from pre-scan/verification; copying follows the chosen Robocopy link flags. Hash verification is unavailable for moves and skeleton copies. An unavailable network share can delay shutdown while a filesystem call is blocked.

The bundled MinGW Qt build uses D3D11 or software rendering after probing; it has no working D3D12 backend. DWM effects depend on Windows support. No fixed speed or frame-rate guarantee is made.

## Dependencies and notices

Qt is dynamically linked. License texts accompany the distribution; matching Qt source locations are listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Vendored xxHash, BLAKE3 and MinHook retain their own licenses. GCC runtime notices are included. Windows supplies Robocopy, which is not redistributed. The editable original logo is in [resources/icons](resources/icons).

Report bugs in [Issues](https://github.com/SheedaMusalli/StrataCopy/issues) with your Windows version, reproduction steps and relevant errors. Review logs for personal paths before sharing them.

Right-click the StrataCopy tray icon and select **Show window only during transfers** for a Windows-style workflow. This preference hides the idle window and its Copy button; progress and conflict windows appear during transfers. Clear the option for the normal interface with its Copy button. Open StrataCopy and Settings remain available in the tray menu. Successful Explorer transfers clear the destination as well as the source list.

Successful transfers show a small Steam-style completion notification near the bottom-right of the transfer screen, with total elapsed time. It slides in without taking focus, stays for about five seconds and can be dismissed. It also works when tray-only mode hides the copy window.

### Hardware-aware performance
The installer records local CPU, RAM and fixed-drive types in the application data folder. Auto (recommended) rechecks source and destination storage for each transfer and uses the lower thread limit: HDD/unknown up to 4, USB/removable up to 4, SSD/network up to 8, NVMe up to 32. Low-core and low-memory machines receive lower limits. These are conservative heuristics, not measured optimal throughput or guaranteed lag-free settings.

Settings → Performance profile offers Auto, Safe / Silent (4), NVMe Extreme (32, no retries), and Manual (1–128). Changing worker threads selects Manual. Explorer transfers retain the selected performance policy. Auto does not run benchmarks or change its thread count in the middle of a transfer. General defaults use buffered I/O; unbuffered I/O remains available for large files. File activity logging, conflicts and verification remain supported. Archive streaming is not part of this feature.

### Deleting files
Delete, Shift+Delete and Recycle Bin operations always use Windows Explorer. StrataCopy handles Explorer copy and move only.
