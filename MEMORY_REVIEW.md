# Ownership and resource review

Reviewed before the first build and execution.

* Process, thread, pipe, null-input, job, drive and token handles use a non-copyable RAII owner. Directory searches use `FindClose`, not `CloseHandle`. Windows process attribute lists have a separate scope guard.
* Robocopy starts suspended and is assigned to a kill-on-close job before its first instruction runs. Assignment failure explicitly terminates and waits for the suspended child. Only the two intended standard-I/O handles are inherited.
* The transfer thread owns all per-run resources. Cancellation requests termination from that thread. The supervisor destructor cancels and joins before the coordinator's remaining members are destroyed. No detached worker or borrowed process handle outlives its owner.
* Backup and restore privileges are restored at the end of the run, including early returns and exceptions.
* File events are limited to 16,384 entries and 8 MiB of estimated string payload. The virtualized UI retains at most 1,000,000 rows and 256 MiB of estimated row payload. Container overhead and Qt allocations add to those estimates. Full raw output is streamed to a disk log.
* At most one telemetry notification is outstanding. The receiving UI acknowledges it; a stalled UI cannot accumulate an unlimited telemetry event backlog.
* The UTF-16 parser limits an incomplete line to 1 MiB. The graph keeps 600 samples. Hash readers use reusable 1 MiB buffers and scoped hasher states. The pre-scanner walks directories without retaining a file manifest.
* SQLite connections are closed and removed after their queries have gone out of scope. IPC sockets have QObject ownership, disconnect cleanup, a five-second lifetime limit and a 1 MiB input limit. Qt signal handlers have receiver contexts.

Issues fixed during review: added event byte limits, added a model byte budget, bounded outstanding telemetry, fixed nonexistent-path ancestor resolution, restored privileges after runs, and stopped a transfer if Windows cannot resume it.

The test suite measures Windows handle count and process private bytes across 28 repeated Robocopy runs after a warm-up. This is a resource-growth regression check, not a substitute for a full heap profiler or proof that every driver/Qt path is leak-free. Final measured results are recorded in `build/test-results.txt` after testing.


## Native Explorer integration

The native DLL has no Qt dependency. COM arguments retained for deferred operations have reference-counted RAII owners. Both `PerformOperations` and final `Release`, including a final release through `IUnknown`, remove tracked state. Unsupported mixed batches replay their retained original calls in order. The process-wide lookup lock is never held while making original COM calls or releasing retained arguments; each operation has its own lock. Exceptions at interception boundaries fail the operation or revert subsequent interception to Windows rather than crossing the COM ABI.

Limits: 64 outstanding objects per Explorer process, 4,096 sources and 80,000 path characters per intercepted object, 512 KiB serialized requests and 64 pending request files. Reaching the object budget disables interception for that Explorer process. The host keeps at most 128 scoped thread hooks, detects exited threads and releases hooks/handles at shutdown. A named event gates every handoff. A versioned shared mapping supplies the correct executable and data directory, including isolated runs. A per-process marker prevents a second loaded copy of the DLL from layering the same hooks. Native code and target implementation modules are pinned for the process lifetime intentionally; disabling causes pass-through without unsafe concurrent unpatching.

Directory enumeration, file/mapping/event/process handles and mapped views have matching release paths. Committed requests survive busy/confirmation states and are removed only after journaling and worker startup, or explicit rejection. Startup and IPC identities include the data directory so a test request cannot enter another installation's journal. A second launcher exits after forwarding to that instance.

The native suite includes 1,200 create/queue/perform-or-abandon cycles and 800 operations across four COM apartments. It tests final release via `IUnknown`, object reuse, Unicode payloads, item arrays and IDataObject, native fallback, mixed-batch replay and actual app/Robocopy handoff. Measured results are in `build/native-test-results.txt`. These are bounded-growth checks, not proof of zero leaks.


## Enumeration and UI ownership

Uncloneable shell enumerators retain at most 4,097 COM item references before delegating their remaining stream to Windows. The replay object is reference counted, with scoped retained arguments and exception-safe fallback. Normalized path strings retain the existing 80,000-character / 4,096-source limits. The window chrome filter has QObject window ownership, a weak window reference, and explicit native-filter removal on destruction. The helper singleton retry is limited to five seconds. Diagnostic file and mapping handles are scoped on every call; logging is disabled in ordinary startup and bounded when explicitly enabled.

## Settings integration

The default copy handler checkbox uses the existing helper singleton. It adds no persistent process objects, timers, native hooks or raw allocations. Registry access uses scoped QSettings objects; the helper stop request uses a scoped QProcess, waits for completion, and kills/reaps the stop-request child on timeout. Stop exit status and registry write failures now produce feedback. Installer task metadata is updated only for the matching installed directory so an upgrade preserves the checkbox choice.

All required release checks must pass without skips. The helper is stopped before testing the actual application/Robocopy handoff. Handle and private-byte measurements are recorded in the release validation report. These checks do not establish that every allocation path is leak-free.

## File conflicts and update checks

The conflict manifest streams to QTemporaryFile rather than retaining a file list in RAM. A worker waits on a condition variable for one token-matched UI decision. Cancellation takes the same mutex before notifying, avoiding a lost-wakeup shutdown deadlock. Stale or duplicate replies are ignored. Temporary manifests and empty duplicate staging folders are removed by scoped owners. If Robocopy has produced staged payload and publication fails, the payload is deliberately retained with a logged recovery path; it is not blindly deleted after a move.

The updater owns a QNetworkAccessManager and its replies. It accepts only one check at a time, caps retained response data at 256 KiB, and has a 15-second absolute deadline. Replies are deleted after completion; destruction disconnects callbacks and aborts any outstanding reply before the manager releases it. Tests cover current/newer/malformed versions, HTTP errors, oversized payloads, timeouts, duplicate clicks and shutdown with a pending request.
