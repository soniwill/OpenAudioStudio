#include "runtime.h"
#include "arguments.h"

#include <cmath>
#include <string_view>

namespace {

constexpr std::string_view vcaPrefix = "vca:/";

bool valid_vca_path(std::string_view path) noexcept
{
    return path.size() > vcaPrefix.size() && path.substr(0, vcaPrefix.size()) == vcaPrefix;
}

}

extern "C" {

oae_result OAE_CALL oae_studio_get_vca(oae_system system, const char* path, oae_vca* out_vca)
{
    return oae::api_call([&] {
        if (!out_vca || !oae::valid_utf8_argument(path) || !valid_vca_path(path)) {
            return OAE_ERR_INVALID_PARAM;
        }
        auto* sys = oae::runtime.systems.get(system);
        if (!sys) {
            return OAE_ERR_INVALID_HANDLE;
        }
        return sys->find_vca(path, out_vca);
    });
}

oae_result OAE_CALL oae_vca_set_volume(oae_vca vca, float volume)
{
    return oae::api_call([&] {
        if (!std::isfinite(volume) || volume < 0.0f || volume > 1.0f) {
            return OAE_ERR_INVALID_PARAM;
        }
        auto* obj = oae::runtime.vcas.get(vca);
        if (!obj) {
            return OAE_ERR_INVALID_HANDLE;
        }
        return obj->set_volume(volume);
    });
}

oae_result OAE_CALL oae_vca_get_volume(oae_vca vca, float* out_volume)
{
    return oae::api_call([&] {
        if (!out_volume) {
            return OAE_ERR_INVALID_PARAM;
        }
        auto* obj = oae::runtime.vcas.get(vca);
        if (!obj) {
            return OAE_ERR_INVALID_HANDLE;
        }
        *out_volume = obj->volume();
        return OAE_OK;
    });
}

}
