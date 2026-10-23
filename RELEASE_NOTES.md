## StrataCopy v1.1.0

This update adds hardware-aware performance settings and a more configurable Windows copy experience.

- Auto checks CPU, RAM and both transfer drives; separate thread dropdowns cover NVMe, SATA/other SSDs, HDDs, USB/removable, network and unknown storage. Custom limits persist and Explorer transfers retain them.
- Steam-style completion notifications show elapsed time with slide-up and slide-down animations. Notifications and animation can be disabled in Settings or during installation.
- The expanded installer offers tray-only operation, compact/detailed defaults, pinning, completion behavior, notification choices and performance profiles. The task-page footer layout now fits above the navigation bar.
- Tray-only mode keeps the app available without an idle copy window. Successful Explorer transfers clear source and destination. Additional dropped files queue for the same destination.
- Existing-file prompts support Do this for all conflicts for the current transfer.

Download **StrataCopy-1.1.0-Setup-x64.exe**. All **40 engine/native checks passed**, with zero failures or skips. Installer checks cover preferences, upgrades, registrations and uninstall cleanup.

Auto uses conservative heuristics rather than throughput benchmarks. Unsupported Explorer operations use Windows. Copy cancellation does not roll back completed files.

Explorer Delete, context-menu Delete and Shift+Delete now route into StrataCopy with recycle/permanent intent preserved, explicit confirmation, progress, cancellation and completion timing. Recycling uses Windows Shell behavior; deletion speed is not guaranteed to exceed Explorer.
