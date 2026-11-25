#include "TimelineSnap.h"

namespace oae::studio {

std::vector<TempoSegment> TimelineSnap::extract_tempo_segments(const nlohmann::json* logic_items)
{
    std::vector<TempoSegment> segments;
    if (logic_items && logic_items->is_array()) {
        for (const auto& item : *logic_items) {
            if (item.is_object() && item.value("kind", "") == "tempo") {
                TempoSegment seg;
                seg.position_ms = item.value("position_ms", 0.0);
                seg.bpm = item.value("bpm", 120.0);
                seg.ts_upper = item.value("ts_upper", 4);
                seg.ts_lower = item.value("ts_lower", 4);
                segments.push_back(seg);
            }
        }
    }

    std::sort(segments.begin(), segments.end(), [](const TempoSegment& a, const TempoSegment& b) {
        return a.position_ms < b.position_ms;
    });

    if (segments.empty() || segments.front().position_ms > 0.0) {
        segments.insert(segments.begin(), TempoSegment{ 0.0, 120.0, 4, 4 });
    }

    return segments;
}

TempoSegment TimelineSnap::get_active_tempo(double time_ms, const std::vector<TempoSegment>& segments)
{
    if (segments.empty()) {
        return TempoSegment{ 0.0, 120.0, 4, 4 };
    }

    TempoSegment active = segments.front();
    for (const auto& seg : segments) {
        if (seg.position_ms <= time_ms) {
            active = seg;
        } else {
            break;
        }
    }
    return active;
}

double TimelineSnap::snap(double time_ms, const nlohmann::json* logic_items) const
{
    if (time_ms <= 0.0) return 0.0;
    if (mode_ == SnapMode::None) return time_ms;

    // Time-based snap modes
    double time_interval_ms = 0.0;
    switch (mode_) {
        case SnapMode::Seconds_1s:    time_interval_ms = 1000.0; break;
        case SnapMode::Seconds_500ms: time_interval_ms = 500.0;  break;
        case SnapMode::Seconds_100ms: time_interval_ms = 100.0;  break;
        case SnapMode::Seconds_50ms:  time_interval_ms = 50.0;   break;
        case SnapMode::Seconds_10ms:  time_interval_ms = 10.0;   break;
        default: break;
    }

    if (time_interval_ms > 0.0) {
        return std::round(time_ms / time_interval_ms) * time_interval_ms;
    }

    // Musical snap modes
    const auto segments = extract_tempo_segments(logic_items);
    const auto active = get_active_tempo(time_ms, segments);

    double subdivision_ms = active.beat_duration_ms();
    switch (mode_) {
        case SnapMode::Bar:            subdivision_ms = active.bar_duration_ms(); break;
        case SnapMode::Beat:           subdivision_ms = active.beat_duration_ms(); break;
        case SnapMode::Beat_Half:      subdivision_ms = active.beat_duration_ms() / 2.0; break;
        case SnapMode::Beat_Quarter:   subdivision_ms = active.beat_duration_ms() / 4.0; break;
        case SnapMode::Beat_Eighth:    subdivision_ms = active.beat_duration_ms() / 8.0; break;
        case SnapMode::Beat_Sixteenth: subdivision_ms = active.beat_duration_ms() / 16.0; break;
        default: break;
    }

    if (subdivision_ms <= 0.0) return time_ms;

    const double delta = time_ms - active.position_ms;
    const double rounded = std::round(delta / subdivision_ms) * subdivision_ms;
    return std::max(0.0, active.position_ms + rounded);
}

const char* TimelineSnap::get_mode_name(SnapMode mode) noexcept
{
    switch (mode) {
        case SnapMode::None: return "No Snap";
        case SnapMode::Seconds_1s: return "1 Second";
        case SnapMode::Seconds_500ms: return "500 ms";
        case SnapMode::Seconds_100ms: return "100 ms";
        case SnapMode::Seconds_50ms: return "50 ms";
        case SnapMode::Seconds_10ms: return "10 ms";
        case SnapMode::Bar: return "1 Bar";
        case SnapMode::Beat: return "1 Beat (1/4)";
        case SnapMode::Beat_Half: return "1/2 Beat (1/8)";
        case SnapMode::Beat_Quarter: return "1/4 Beat (1/16)";
        case SnapMode::Beat_Eighth: return "1/8 Beat (1/32)";
        case SnapMode::Beat_Sixteenth: return "1/16 Beat (1/64)";
        default: return "Custom";
    }
}

SnapMode TimelineSnap::mode_from_index(int index) noexcept
{
    switch (index) {
        case 0: return SnapMode::None;
        case 1: return SnapMode::Seconds_1s;
        case 2: return SnapMode::Seconds_500ms;
        case 3: return SnapMode::Seconds_100ms;
        case 4: return SnapMode::Seconds_50ms;
        case 5: return SnapMode::Seconds_10ms;
        case 6: return SnapMode::Bar;
        case 7: return SnapMode::Beat;
        case 8: return SnapMode::Beat_Half;
        case 9: return SnapMode::Beat_Quarter;
        case 10: return SnapMode::Beat_Eighth;
        case 11: return SnapMode::Beat_Sixteenth;
        default: return SnapMode::Beat;
    }
}

} // namespace oae::studio
