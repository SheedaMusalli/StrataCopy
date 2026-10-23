#pragma once
#include <utility>
#include <windows.h>
namespace strata {
class UniqueHandle {
  public:
    UniqueHandle() = default;
    explicit UniqueHandle(HANDLE handle) : m_handle(handle) {}
    ~UniqueHandle() {
        reset();
    }
    UniqueHandle(const UniqueHandle &) = delete;
    UniqueHandle &operator=(const UniqueHandle &) = delete;
    UniqueHandle(UniqueHandle &&other) noexcept : m_handle(std::exchange(other.m_handle, nullptr)) {}
    UniqueHandle &operator=(UniqueHandle &&other) noexcept {
        if (this != &other)
            reset(std::exchange(other.m_handle, nullptr));
        return *this;
    }
    HANDLE get() const {
        return m_handle;
    }
    explicit operator bool() const {
        return m_handle && m_handle != INVALID_HANDLE_VALUE;
    }
    void reset(HANDLE handle = nullptr) {
        if (*this)
            CloseHandle(m_handle);
        m_handle = handle;
    }

  private:
    HANDLE m_handle = nullptr;
};
} // namespace strata
