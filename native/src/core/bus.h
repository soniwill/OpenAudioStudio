#pragma once

#include <oae_audio.h>
#include <miniaudio.h>

#include "bus_dsp.h"
#include "strip.h"

#include <string>

namespace oae {

class System;

class Bus {
public:
    static constexpr unsigned max_depth = 32;

    Bus(System& owner, Bus* parent, std::string path);
    ~Bus();
    Bus(const Bus&) = delete;
    Bus& operator=(const Bus&) = delete;

    oae_result init() noexcept;
    System* owner() const noexcept { return owner_; }
    unsigned depth() const noexcept { return depth_; }
    const std::string& path() const noexcept { return path_; }
    ma_sound_group* group() noexcept { return &group_; }
    bool within(const Bus& ancestor) const noexcept;
    bool is_return() const noexcept { return path_.rfind("return:/", 0) == 0; }
    bool can_reach(const Bus& target) const noexcept;
    oae_result add_send(Bus& target, float level, int position) noexcept;
    oae_result remove_send(const Bus& target) noexcept;
    void detach_all_sends() noexcept;
    bool paused_in_tree() const noexcept;
    double effective_gain() const noexcept;
    void set_volume(float value) noexcept { strip_.set_volume(value); }
    float volume() const noexcept { return strip_.volume(); }
    void set_vca_gain(float gain) noexcept { strip_.set_vca_gain(gain); }
    float vca_gain() const noexcept { return strip_.vca_gain(); }
    void set_snapshot_gain(float gain) noexcept { strip_.set_snapshot_gain(gain); }
    float snapshot_gain() const noexcept { return strip_.snapshot_gain(); }
    void set_muted(bool muted) noexcept { strip_.set_muted(muted); }
    bool muted() const noexcept { return strip_.muted(); }
    oae_result set_paused(bool paused) noexcept;
    bool paused() const noexcept { return paused_; }
    BusDsp& dsp() noexcept { return dsp_; }
    const BusDsp& dsp() const noexcept { return dsp_; }
    class Effect* effect_at(int position, std::size_t index) noexcept { return dsp_.effect_at(position, index); }
    class Effect* effect_at(std::size_t index) noexcept {
        auto* eff = dsp_.effect_at(OAE_EFFECT_POST_FADER, index);
        if (!eff) {
            eff = dsp_.effect_at(OAE_EFFECT_PRE_FADER, index);
        }
        return eff;
    }
    StripNode& strip() noexcept { return strip_; }
    const StripNode& strip() const noexcept { return strip_; }

private:
    System* owner_;
    Bus* parent_;
    std::string path_;
    unsigned depth_;
    ma_sound_group group_{};
    BusDsp dsp_;
    StripNode strip_;
    bool groupReady_ = false;
    bool paused_ = false;
};

}
