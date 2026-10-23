#pragma once
#include "UniqueHandle.hpp"
#include <QString>
namespace strata {
class PrivilegeEscalator {
  public:
    PrivilegeEscalator() = default;
    PrivilegeEscalator(const PrivilegeEscalator &) = delete;
    PrivilegeEscalator &operator=(const PrivilegeEscalator &) = delete;
    ~PrivilegeEscalator();
    QString enableBackupRestore();

  private:
    UniqueHandle m_token;
    struct Privileges {
        DWORD count = 0;
        LUID_AND_ATTRIBUTES entries[2]{};
    } m_previous;
    bool m_adjusted = false;
};
} // namespace strata
