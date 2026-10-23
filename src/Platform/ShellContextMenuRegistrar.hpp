#pragma once
#include <QString>
namespace strata {
class ShellContextMenuRegistrar {
  public:
    static QString setEnabled(bool);
    static QString setKeyboardPasteEnabled(bool);
    static bool keyboardPasteEnabled();
};
} // namespace strata
