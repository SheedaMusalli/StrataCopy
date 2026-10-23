#include "PresetListModel.hpp"
namespace strata {
QStringList PresetListModel::builtins() {
    return {"Auto (recommended) · hardware aware", "Network NAS · restartable / Auto",
            "Exact mirror backup · /MIR /ZB", "Admin full clone · /COPYALL /ZB", "Safe / Silent · 4 threads", "NVMe Extreme · 32 threads / no retries"};
}
} // namespace strata
