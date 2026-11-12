## StrataCopy v2.0.0

- Elevated retries enable backup privileges before scanning protected sources or taking the same-volume move path, and open a visible transfer window with the correct data directory.
- Added an Explorer prompt to choose StrataCopy or the Windows copy handler for copy and move actions.
- Fixed move action selection and added same-volume rename moves for fast transfers.
- Added an administrator retry path for operations that need elevated access.
- Multiple Explorer actions can run at once in separate StrataCopy transfer windows.
- Moved automatic drive detection off the UI thread and reduced file-list update pressure during large transfers.
- Installer and application version metadata updated to 2.0.0.
- The production installer requests administrator approval and installs under Program Files; StrataCopy itself stays unelevated by default.
- Delete and recycle are handled by Windows Explorer; StrataCopy integration now intercepts copy and move only.
- Reworked the full app into a compact Windows copy-dialog style, including the transfer view, settings, history, dialogs and completion notification.
- The installer no longer asks about Explorer integration or shows the preference/performance pages. It sets Auto on every install/upgrade and defaults tray-only mode, elapsed-time notifications, notification animation and close-after-success on; detailed mode and pinning stay off.
- Fresh installs now enable and start the Explorer copy/move integration by default; existing on/off choices are preserved during upgrades.
- Auto starts USB/removable transfers at four Robocopy workers and respects the slowest storage pair across multi-operation batches; source pre-scans use background I/O priority.
- Reduced per-file Robocopy log verbosity to lower formatting, disk-write and parsing overhead on large transfers while retaining byte counts and error output; child workers use background CPU/I/O priority to share the machine more gently with foreground apps.
- Same-volume moves now map a selected item into an existing destination folder using native rename when the target name is free, avoiding needless read/write copies while preserving collision safety.
- Corrected Windows-style same-volume move destinations: selected loose files now land directly in the chosen folder; existing same-name folder targets merge missing entries and remove emptied source folders, while conflicts remain for the configured conflict handling.
- Same-volume detection now resolves a not-yet-created destination to its nearest existing parent, so a new target path on the same drive takes the instant native rename path instead of falling back to a file copy.
- Added source-only checksum generation from the Test button using XXH3-64, BLAKE3 or SHA-256; optional sidecars use atomic writes and existing sidecars are preserved.

## StrataCopy v1.1.0

This update adds hardware-aware performance settings and a more configurable Windows copy experience.

- Auto checks CPU, RAM and both transfer drives; separate thread dropdowns cover NVMe, SATA/other SSDs, HDDs, USB/removable, network and unknown storage. Custom limits persist and Explorer transfers retain them.
- Steam-style completion notifications show elapsed time with slide-up and slide-down animations. Notifications and animation can be disabled in Settings or during installation.
- The expanded installer offers tray-only operation, compact/detailed defaults, pinning, completion behavior, notification choices and performance profiles. The task-page footer layout now fits above the navigation bar.
- Tray-only mode keeps the app available without an idle copy window. Successful Explorer transfers clear source and destination. Additional dropped files queue for the same destination.
- Existing-file prompts support Do this for all conflicts for the current transfer.

Download **StrataCopy-1.1.0-Setup-x64.exe**. All **40 engine/native checks passed**, with zero failures or skips. Installer checks cover preferences, upgrades, registrations and uninstall cleanup.

Auto uses conservative heuristics rather than throughput benchmarks. Unsupported Explorer operations use Windows. Copy cancellation does not roll back completed files.

Windows handles Delete, Shift+Delete and Recycle Bin actions. StrataCopy integration is limited to copy and move.
