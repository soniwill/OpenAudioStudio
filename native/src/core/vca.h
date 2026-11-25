#pragma once

#include <oae_audio.h>
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace oae {

class System;

class Vca {
public:
    Vca(System& owner, std::string path, std::vector<std::string> bus_paths, std::vector<std::string> event_paths, float default_volume = 1.0f);
    ~Vca() = default;

    System* owner() const noexcept { return owner_; }
    const std::string& path() const noexcept { return path_; }
    const std::vector<std::string>& bus_paths() const noexcept { return bus_paths_; }
    const std::vector<std::string>& event_paths() const noexcept { return event_paths_; }

    float volume() const noexcept { return volume_; }
    oae_result set_volume(float volume) noexcept;

private:
    System* owner_;
    std::string path_;
    std::vector<std::string> bus_paths_;
    std::vector<std::string> event_paths_;
    float volume_ = 1.0f;
};

}
