#include "PrivilegeEscalator.hpp"
#include "UniqueHandle.hpp"
namespace strata {
QString PrivilegeEscalator::enableBackupRestore() {
    HANDLE raw = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &raw))
        return "Cannot open process token.";
    m_token.reset(raw);
    Privileges requested;
    requested.count = 2;
    DWORD returned = 0;
    const wchar_t *names[]{SE_BACKUP_NAME, SE_RESTORE_NAME};
    for (int i = 0; i < 2; ++i) {
        if (!LookupPrivilegeValueW(nullptr, names[i], &requested.entries[i].Luid))
            return "Cannot resolve backup privileges.";
        requested.entries[i].Attributes = SE_PRIVILEGE_ENABLED;
    }
    SetLastError(ERROR_SUCCESS);
    m_adjusted = AdjustTokenPrivileges(m_token.get(), FALSE, reinterpret_cast<TOKEN_PRIVILEGES *>(&requested),
                                       sizeof(m_previous), reinterpret_cast<TOKEN_PRIVILEGES *>(&m_previous),
                                       &returned);
    if (!m_adjusted || GetLastError() != ERROR_SUCCESS)
        return "Backup / restore privileges are unavailable. Start StrataCopy as administrator or choose "
               "Standard I/O.";
    return {};
}
PrivilegeEscalator::~PrivilegeEscalator() {
    if (m_adjusted)
        AdjustTokenPrivileges(m_token.get(), FALSE, reinterpret_cast<TOKEN_PRIVILEGES *>(&m_previous), 0,
                              nullptr, nullptr);
}
} // namespace strata
