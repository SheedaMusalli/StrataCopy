# StrataCopy v1.0.0 validation

Release environment: Windows x64, Qt 6.8.3, MinGW 13.1, Release configuration. Timed UI test hooks are disabled in the distributable.

## Required checks

The Explorer helper was stopped before the release tests. All 24 engine/coordinator checks and all 10 native integration checks passed: **34 passed, zero failed, zero skipped**. The build script rejects skipped tests.

Checks cover command generation, Unicode argument round trips, batching, overlap rejection, copy/dry-run output, excluded siblings, errors, cancellation, pause/resume, paused shutdown, hashing/mismatches, SQLite recovery, bounded models/graphs, destructive confirmation and durable Explorer requests.

Native checks exercise real IFileOperation objects in the test process: single items, arrays, IDataObject, uncloneable enumerators, IPersistIDList, same-name targets, failed handoff fallback, mixed-operation replay, final IUnknown release, object reuse and concurrent apartments. The end-to-end check launches an isolated offscreen app, hands off a request, verifies the extra launcher exits, verifies actual Robocopy output and confirms request consumption.

## Memory and resources

Resource measurements are recorded in build/test-results.txt and build/native-test-results.txt, and are packaged in the installer's validation folder. The engine assertion allows bounded handle variation; the native soak performs 1,200 lifecycle iterations and a separate apartment check performs 800 operations. Both must leave zero pending native operations. Additional checks exercise pending conflict cancellation and network-request destruction. These are resource-growth regressions, not a proof of zero leaks.

Registry objects and stop-request processes are scoped. Window filters unregister on destruction. Retained COM arguments have RAII owners; fallback and abandonment release them. The update check owns its network manager/reply, limits the response to 256 KiB, and applies an absolute 15-second deadline. Duplicate publication never replaces an occupied numbered name. File manifests stream to scoped temporary files. See MEMORY_REVIEW.md for ownership and budgets.

## Installer

The isolated regression installs all optional tasks, checks executable hashes, shortcuts and registrations, runs the deployed helper self-test, upgrades without respecifying tasks, and uninstalls. It verifies cleanup and asserts that the real startup registration is unchanged. The QA installer uses a separate identity and registry namespace.

The production app, helper, Windows version resource, manifest, deployment folder and installer identify the product as 1.0.0. Native protocol generations are internal compatibility identifiers, not product release versions.

## Practical limits

Explorer interception covers normal filesystem copy/move. Unsupported/virtual operations use Windows; intercepted transfers do not create Explorer Undo records. The default conflict dialog offers Replace Existing File, Skip This File and Make Duplicate. Progress and ETA are estimates. Cancellation does not undo completed files.
