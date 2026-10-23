#include "PresetListModel.hpp"
namespace strata {
QStringList PresetListModel::builtins() {
    return {"High-speed default · 128 threads", "Network NAS · restartable / 64 threads",
            "Exact mirror backup · /MIR /ZB", "Admin full clone · /COPYALL /ZB"};
}
} // namespace strata
