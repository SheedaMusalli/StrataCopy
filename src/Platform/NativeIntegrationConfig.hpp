#pragma once
#include <windows.h>
namespace strata {
inline constexpr wchar_t nativeConfigurationName[] = L"Local\\StrataCopy.NativeCopy.Configuration.5";
struct NativeIntegrationConfig {
    DWORD version = 1;
    wchar_t executable[32768]{};
    wchar_t dataDirectory[32768]{};
    DWORD diagnostics = 0;
};
} // namespace strata
