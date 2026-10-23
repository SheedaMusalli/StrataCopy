# Third-party notices

StrataCopy dynamically links the following Qt 6.8.3 modules and their runtime dependencies: Core, Gui, Network, Sql, Qml, Quick, Quick Controls, Quick Dialogs, Quick Effects and SVG. The installed Qt distribution includes third-party components with their own notices. License texts are included in `third_party/qt-licenses`, and the Qt SBOM is included when available.

Qt project: https://www.qt.io/licensing/open-source-lgpl-obligations

Matching source archives:

* https://download.qt.io/archive/qt/6.8/6.8.3/single/
* https://code.qt.io/cgit/qt/qtbase.git/tree/?h=v6.8.3
* https://code.qt.io/cgit/qt/qtdeclarative.git/tree/?h=v6.8.3
* https://code.qt.io/cgit/qt/qtsvg.git/tree/?h=v6.8.3

The application uses shared Qt libraries; the supplied libraries can be replaced with compatible builds. The StrataCopy source package and CMake scripts allow rebuilding against another Qt installation. Shader compilation uses Qt Shader Tools at build time.

xxHash 0.8.3: https://github.com/Cyan4973/xxHash/tree/v0.8.3 — BSD-2-Clause. Its complete vendored source and LICENSE are in `third_party/xxhash`.

BLAKE3 1.8.2: https://github.com/BLAKE3-team/BLAKE3/tree/1.8.2 — Apache-2.0 or CC0. Portable C sources and both licenses are in `third_party/blake3`.

MinGW-w64 GCC 13.1 runtime DLLs are redistributed alongside Qt. GCC libraries use GPLv3 with the GCC Runtime Library Exception. The relevant texts are in `third_party/toolchain-licenses`; MinGW-w64 runtime notices are included there when supplied by the toolchain.

Robocopy is part of Windows and is not redistributed. StrataCopy invokes the installed executable in the Windows system directory.

The “Forward Strata” SVG/logo and icon were drawn specifically for this application. Their editable source is included in `resources/icons` in the source repository.


MinHook 1.3.4: https://github.com/TsudaKageyu/minhook/tree/v1.3.4 — BSD-2-Clause. Complete source and LICENSE.txt are in `third_party/minhook`. The native Explorer DLL statically links MinHook and the GCC support runtimes; those runtimes remain covered by the GCC Runtime Library Exception described above.
