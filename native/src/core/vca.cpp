#include "vca.h"
#include "system.h"

namespace oae {

Vca::Vca(System& owner, std::string path, std::vector<std::string> bus_paths, std::vector<std::string> event_paths, float default_volume)
    : owner_(&owner), path_(std::move(path)), bus_paths_(std::move(bus_paths)), event_paths_(std::move(event_paths)), volume_(std::clamp(default_volume, 0.0f, 1.0f))
{
}

oae_result Vca::set_volume(float volume) noexcept
{
    if (!std::isfinite(volume) || volume < 0.0f || volume > 1.0f) {
        return OAE_ERR_INVALID_PARAM;
    }
    volume_ = volume;
    if (owner_) {
        owner_->update_vca_multipliers();
    }
    return OAE_OK;
}

}
