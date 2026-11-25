#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>

namespace oae::studio {

/** Enumerates the magnetic grid snapping modes available in the timeline. */
enum class SnapMode {
    None,
    Seconds_1s,
    Seconds_500ms,
    Seconds_100ms,
    Seconds_50ms,
    Seconds_10ms,
    Bar,
    Beat,
    Beat_Half,
    Beat_Quarter,
    Beat_Eighth,
    Beat_Sixteenth
};

/** Structure defining an active musical tempo and meter segment. */
struct TempoSegment {
    double position_ms{ 0.0 };
    double bpm{ 120.0 };
    int ts_upper{ 4 };
    int ts_lower{ 4 };

    double beat_duration_ms() const noexcept {
        return 60000.0 / std::max(1.0, bpm);
    }

    double bar_duration_ms() const noexcept {
        return beat_duration_ms() * (static_cast<double>(ts_upper) * 4.0 / static_cast<double>(ts_lower));
    }
};

/** Provides magnetic time quantization and musical grid calculations. */
class TimelineSnap {
public:
    TimelineSnap() = default;

    void set_mode(SnapMode mode) noexcept { mode_ = mode; }
    SnapMode get_mode() const noexcept { return mode_; }

    /** Snaps a time position in milliseconds to the active magnetic grid. */
    double snap(double time_ms, const nlohmann::json* logic_items = nullptr) const;

    /** Extracts tempo segments from event logic_items, sorted by position. */
    static std::vector<TempoSegment> extract_tempo_segments(const nlohmann::json* logic_items);

    /** Returns the active tempo segment at the specified timeline position. */
    static TempoSegment get_active_tempo(double time_ms, const std::vector<TempoSegment>& segments);

    /** Converts mode to human-readable label for UI selectors. */
    static const char* get_mode_name(SnapMode mode) noexcept;

    /** Parses a mode enum from index. */
    static SnapMode mode_from_index(int index) noexcept;

private:
    SnapMode mode_{ SnapMode::Beat };
};

} // namespace oae::studio
