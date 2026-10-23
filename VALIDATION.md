# StrataCopy v1.1.0 validation

Release environment: Windows x64, Qt 6.8.3, MinGW 13.1, Release configuration. Timed UI test hooks are disabled in the distributable.

## Required checks

The Explorer helper was stopped before the release tests. All 29 engine/coordinator checks and all 11 native integration checks passed: **40 passed, zero failed, zero skipped**. The build script rejects skipped tests.

Checks cover command generation, Unicode argument round trips, batching, overlap rejection, copy/dry-run output, excluded siblings, errors, cancellation, pause/resume, paused shutdown, hashing/mismatches, SQLite recovery, bounded models/graphs, destructive confirmation and durable Explorer requests.

Native checks exercise real IFileOperation objects in the test process: single items, arrays, IDataObject, uncloneable enumerators, IPersistIDList, same-name targets, failed handoff fallback, mixed-operation replay, final IUnknown release, object reuse and concurrent apartments. The end-to-end check launches an isolated offscreen app, hands off a request, verifies the extra launcher exits, verifies actual Robocopy output and confirms request consumption.

## Memory and resources

Resource measurements are recorded in build/test-results.txt and build/native-test-results.txt, and are packaged in the installer's validation folder. The engine assertion allows bounded handle variation; the native soak performs 1,200 lifecycle iterations and a separate apartment check performs 800 operations. Both must leave zero pending native operations. Additional checks exercise pending conflict cancellation and network-request destruction. These are resource-growth regressions, not a proof of zero leaks.

Registry objects and stop-request processes are scoped. Window filters unregister on destruction. Retained COM arguments have RAII owners; fallback and abandonment release them. The update check owns its network manager/reply, limits the response to 256 KiB, and applies an absolute 15-second deadline. Duplicate publication never replaces an occupied numbered name. File manifests stream to scoped temporary files. See MEMORY_REVIEW.md for ownership and budgets.

## Installer

The isolated regression installs all optional tasks, checks executable hashes, shortcuts and registrations, runs the deployed helper self-test, upgrades without respecifying tasks, and uninstalls. It verifies cleanup and asserts that the real startup registration is unchanged. The QA installer uses a separate identity and registry namespace.

The production app, helper, Windows version resource, manifest, deployment folder and installer identify the product as 1.1.0. Native protocol generations are internal compatibility identifiers, not product release versions.

## Practical limits

Explorer interception covers normal filesystem copy/move. Unsupported/virtual operations use Windows; intercepted transfers do not create Explorer Undo records. The default conflict dialog offers Replace Existing File, Skip This File and Make Duplicate. Progress and ETA are estimates. Cancellation does not undo completed files.

Current UI revision: tested busy source queueing, move without routine confirmation, reset to copy-all mode and an empty source list, and apply-all decisions resetting between runs. Engine handles 226→226 and private bytes 4,055,040→4,055,040; native handles 372→372, pending objects zero. Installed UI checked for empty sources and complete compact layout.

Tray revision: Explorer request integration tests assert an empty destination after success, including successive queued moves. Manual-mode sources retain their chosen destination. Qt Widgets is deployed for the native tray menu.

Live tray-only fixture: progress/conflict window appeared automatically, then disappeared on success while the process stayed running. Persisted source list and destination were both empty. Normal mode retains Copy. Installer validation passed with real startup registrations unchanged.

Completion notification: engine check asserts exactly one successful timing signal and a nonnegative elapsed duration. All 29 engine/coordinator and 10 native tests passed; installer install/upgrade/uninstall validation passed. The toast keeps the event loop alive briefly when normal completion closes the main window.

Hardware-aware revision: 26 engine/coordinator plus 10 native checks passed. Tests cover CPU/RAM bounds, all storage tiers, conservative unknown fallback, explicit no-retry Extreme arguments, Manual preservation through Explorer imports and 100 device-query handle cycles. Hardware CLI smoke test detected 12 logical processors, approximately 32 GB RAM and NVMe/SSD/HDD/USB storage on this machine.

Installed live Auto fixture passed after adding a fresh-profile migration regression check: both drives on E: were detected as mechanical, profile mode Auto (0), effective 4 threads, buffered I/O, transfer Completed and destination payload correct. Installer hardware.json was generated during installation.

Version 1.1 final UI checks: all installer pages fit without clipped footer text; six storage dropdowns are visible in Settings. Selected HDD 2 threads in an isolated installed-app session; a subsequent Explorer request completed with hddThreads=2 and actual threadCount=2. All 37 checks and installer preference/upgrade/uninstall checks passed.

Deletion revision: all 29 engine/coordinator plus 11 native checks passed without failures/skips, including worker cancellation, 20 repeated deletion cycles, recycle, permanent delete, junction target preservation and native flag preservation/fallback. Installed permanent-delete confirmation was inspected and cancelled; its fixture remained intact. Protocol 5 enabled event and installed marker were present in the live Explorer process. Installer validation passed.
