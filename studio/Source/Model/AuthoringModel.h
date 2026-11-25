#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <algorithm>
#include "../Project/ProjectDocument.h"

namespace oae::studio {

/** Enumerates the high-level authoring asset categories in an OpenAudio project. */
enum class ItemType {
    Event,
    Snapshot,
    Bus,
    Vca,
    Sample,
    GlobalParameter,
    Unknown
};

/** Identifies the currently selected authoring item or sub-item in the DAW. */
struct ItemSelection {
    ItemType type{ ItemType::Unknown };
    std::string id_or_path;
    int track_index{ -1 };
    int instrument_index{ -1 };
    int logic_index{ -1 };

    bool is_valid() const noexcept { return type != ItemType::Unknown && !id_or_path.empty(); }
    bool operator==(const ItemSelection& other) const noexcept {
        return type == other.type && id_or_path == other.id_or_path
            && track_index == other.track_index && instrument_index == other.instrument_index
            && logic_index == other.logic_index;
    }
    bool operator!=(const ItemSelection& other) const noexcept { return !(*this == other); }
};

/** Represents the parameter-driven activation mask of a track layer in LayerStackView. */
struct LayerMaskSpec {
    enum class State {
        AlwaysActive,
        MaskActive,
        CustomCurve
    };

    State state{ State::AlwaysActive };
    std::string param_scope{ "local" };
    std::string param_name;
    double min_norm{ 0.0 };
    double max_norm{ 1.0 };
    double fade_norm{ 0.1 };
    int auto_index{ -1 };
};

/** Manages structured authoring mutations, gesture-based undo/redo, and cross-reference integrity. */
class AuthoringModel {
public:
    /** Listener interface for observing authoring model mutations and selection changes. */
    class Listener {
    public:
        virtual ~Listener() = default;
        virtual void on_project_changed() {}
        virtual void on_selection_changed(const ItemSelection& selection) { (void)selection; }
        virtual void on_item_renamed(ItemType type, const std::string& old_id, const std::string& new_id) {
            (void)type; (void)old_id; (void)new_id;
        }
        virtual void on_item_deleted(ItemType type, const std::string& id) {
            (void)type; (void)id;
        }
    };

    explicit AuthoringModel(ProjectDocument& doc);
    ~AuthoringModel() = default;

    void add_listener(Listener* l);
    void remove_listener(Listener* l);

    // Document and root JSON access
    ProjectDocument& document() noexcept { return doc_; }
    const ProjectDocument& document() const noexcept { return doc_; }
    nlohmann::json& json() noexcept { return doc_.get_root_json(); }
    const nlohmann::json& json() const noexcept { return doc_.get_root_json(); }
    nlohmann::json create_immutable_snapshot() const { return doc_.create_immutable_snapshot(); }

    // Selection
    void select(const ItemSelection& sel);
    const ItemSelection& get_selection() const noexcept { return current_selection_; }
    void clear_selection();

    // Gesture-based Undo/Redo
    void begin_gesture();
    void end_gesture();
    void cancel_gesture();
    bool is_in_gesture() const noexcept { return in_gesture_; }

    bool can_undo() const noexcept { return !undo_stack_.empty(); }
    bool can_redo() const noexcept { return !redo_stack_.empty(); }
    bool undo();
    bool redo();
    void clear_history();

    // Content revision tracking for preview rebuilding
    uint64_t get_content_revision() const noexcept { return content_revision_; }

    // Model mutations (with automatic undo snapshot if not currently in gesture)
    void modify(const std::function<void(nlohmann::json&)>& mutator);
    /** Mutates audition-only state (with Undo/Redo and listener notification) without incrementing content revision. */
    void modify_audition(const std::function<void(nlohmann::json&)>& mutator);

    // CRUD with referential integrity
    bool create_event(const std::string& path, const std::string& bus_path = "bus:/");
    bool create_snapshot(const std::string& path);
    bool create_bus(const std::string& path, const std::string& parent = "bus:/");
    bool create_vca(const std::string& path);
    bool create_global_parameter(const std::string& name, const std::string& kind = "continuous");
    bool import_sample(const std::string& id, const std::string& relative_file);
    bool import_audio_asset(const fs::path& source_path, std::string* out_sample_id = nullptr, double* out_duration_ms = nullptr);
    bool add_single_instrument_from_audio_file(const std::string& event_path, size_t track_index,
                                               const fs::path& source_path, double cursor_time_ms,
                                               std::string* out_sample_id = nullptr, double* out_duration_ms = nullptr);

    bool rename_item(ItemType type, const std::string& old_id, const std::string& new_id);
    bool delete_item(ItemType type, const std::string& id);
    bool duplicate_item(ItemType type, const std::string& id, std::string* out_new_id = nullptr);

    // Helper queries
    std::vector<std::string> get_event_paths() const;
    std::vector<std::string> get_instrument_targets(const std::string& event_path, const std::string& command_type = {}) const;
    std::vector<std::string> get_snapshot_paths() const;
    std::vector<std::string> get_bus_paths() const;
    std::vector<std::string> get_vca_paths() const;
    std::vector<std::string> get_sample_ids() const;
    std::vector<std::pair<std::string, std::string>> get_audio_table_entries() const;
    std::vector<std::string> get_global_parameter_names() const;
    std::vector<std::string> get_local_parameter_names(const std::string& event_path) const;

    // Sub-item manipulation
    bool add_track(const std::string& event_path);
    bool remove_track(const std::string& event_path, size_t track_index);
    bool duplicate_track(const std::string& event_path, size_t track_index);
    bool reorder_track(const std::string& event_path, size_t src_index, size_t dest_index);
    bool add_instrument(const std::string& event_path, size_t track_index, const std::string& kind = "single",
                        const std::string& target = {}, double offset_ms = 0.0);
    bool set_instrument_target(const std::string& event_path, size_t track_index, size_t instrument_index,
                               const std::string& target);
    bool set_command_type(const std::string& event_path, size_t track_index, size_t instrument_index,
                          const std::string& command_type);
    bool set_programmer_key(const std::string& event_path, size_t track_index, size_t instrument_index,
                            const std::string& key);
    bool set_audio_table_entry(const std::string& key, const std::string& sample_id);
    bool remove_audio_table_entry(const std::string& key);
    bool set_scatterer_properties(const std::string& event_path, size_t track_index, size_t instrument_index,
                                  double spawn_interval_min_ms, double spawn_interval_max_ms,
                                  uint32_t polyphony, uint32_t total_sounds,
                                  float min_scatter_distance, float max_scatter_distance);
    bool duplicate_instrument(const std::string& event_path, size_t track_index, size_t instrument_index);
    bool remove_instrument(const std::string& event_path, size_t track_index, size_t instrument_index);
    bool move_instrument(const std::string& event_path, size_t src_track, size_t inst_index, size_t dest_track, double new_offset_ms);
    bool resize_instrument(const std::string& event_path, size_t track_index, size_t inst_index, double new_offset_ms, double new_length_ms, double new_start_offset_ms);
    bool set_instrument_fades(const std::string& event_path, size_t track_index, size_t inst_index, double fade_in_ms, double fade_out_ms);

    bool add_local_parameter(const std::string& event_path, const std::string& name, const std::string& kind = "continuous");
    bool update_local_parameter(const std::string& event_path, const std::string& name, const nlohmann::json& spec);
    bool update_global_parameter(const std::string& name, const nlohmann::json& spec);
    bool rename_local_parameter(const std::string& event_path, const std::string& old_name, const std::string& new_name);
    bool remove_local_parameter(const std::string& event_path, const std::string& name);
    bool set_instrument_sheet_range(const std::string& event_path, size_t track_index, size_t instrument_index,
                                    const std::string& sheet_parameter, double param_min, double param_max);
    bool set_event_user_property(const std::string& event_path, const std::string& key, const std::string& value);
    bool remove_event_user_property(const std::string& event_path, const std::string& key);

    /** Returns the number of tracks in the specified event, or 0 if not found. */
    size_t get_track_count(const std::string& event_path) const;
    /** Returns the track volume in dB (defaults to 0.0). */
    double get_track_volume(const std::string& event_path, size_t track_index) const;
    /** Returns the track stereo pan in [-1.0, 1.0] (defaults to 0.0). */
    double get_track_pan(const std::string& event_path, size_t track_index) const;
    /** Returns the target output track index for track-to-track routing, or -1 for event master. */
    int get_track_output_track(const std::string& event_path, size_t track_index) const;

    bool set_track_volume(const std::string& event_path, size_t track_index, double volume_db);
    bool set_track_pan(const std::string& event_path, size_t track_index, double pan);
    bool set_track_muted(const std::string& event_path, size_t track_index, bool muted);
    bool set_track_solo(const std::string& event_path, size_t track_index, bool solo);
    bool is_track_muted(const std::string& event_path, size_t track_index) const;
    bool is_track_solo(const std::string& event_path, size_t track_index) const;
    std::string get_track_color(const std::string& event_path, size_t track_index) const;
    bool set_track_color(const std::string& event_path, size_t track_index, const std::string& hex_color);
    std::string get_track_name(const std::string& event_path, size_t track_index) const;
    bool set_track_name(const std::string& event_path, size_t track_index, const std::string& name);

    bool add_marker(const std::string& event_path, const std::string& name, double position_ms);
    bool add_tempo_marker(const std::string& event_path, double bpm, int ts_upper, int ts_lower, double position_ms);
    bool add_loop_region(const std::string& event_path, double start_ms, double length_ms, int iterations = -1);
    bool add_transition_marker(const std::string& event_path, double position_ms, const std::string& target_marker,
                               const std::string& quantization = "immediate", double crossfade_ms = 0.0);
    bool add_transition_region(const std::string& event_path, double start_ms, double length_ms, const std::string& target_marker,
                               const std::string& quantization = "immediate", double crossfade_ms = 0.0);
    bool add_sustain_point(const std::string& event_path, double position_ms);
    bool rename_marker(const std::string& event_path, size_t item_index, const std::string& new_name);
    bool update_tempo_marker(const std::string& event_path, size_t item_index, double bpm, int ts_upper, int ts_lower);
    bool remove_logic_item(const std::string& event_path, size_t item_index);

    /** Reads points from the first timeline-driven volume curve of the track. */
    nlohmann::json get_track_timeline_volume_points(const std::string& event_path, int track_index) const;

    /** Updates the track's timeline volume curve, optionally creating it on insertion. */
    bool set_track_timeline_volume_points(const std::string& event_path, int track_index,
                                          const nlohmann::json& points, bool create_if_missing = false);

    /** Removes the track's timeline volume curve entry, preserving the other automations. */
    bool clear_track_timeline_volume_points(const std::string& event_path, int track_index);

    /** Converts track opacity percentage [0..100] to volume in dB [-80..0]. */
    static double opacity_pct_to_db(double opacity_pct) noexcept;

    /** Converts track volume in dB [-80..0] to opacity percentage [0..100]. */
    static double db_to_opacity_pct(double volume_db) noexcept;

    /** Evaluates the effective normalized gain [0..1] of a LayerMaskSpec at a normalized parameter value [0..1]. */
    static double evaluate_layer_mask_gain(const LayerMaskSpec& mask, double param_norm) noexcept;

    /** Inspects track parameter volume automations and extracts a LayerMaskSpec without modifying complex curves. */
    LayerMaskSpec get_track_layer_mask(const std::string& event_path, int track_index) const;

    /** Creates or updates the parameter-driven volume automation curve from a LayerMaskSpec in a single Undo/Redo step. Refuses to overwrite CustomCurve unless force_overwrite is true. */
    bool set_track_layer_mask(const std::string& event_path, int track_index,
                              const std::string& param_scope, const std::string& param_name,
                              double min_norm, double max_norm, double fade_norm,
                              bool force_overwrite = false);

    /** Removes the parameter-driven volume automation curve of a track, returning it to AlwaysActive. */
    bool clear_track_layer_mask(const std::string& event_path, int track_index);

    /** Adds a new track with a looped Single Instrument for stem workflows in a single Undo/Redo step. */
    bool add_stem_layer(const std::string& event_path, const std::string& sample_id = {});

    /** Imports an audio file and creates a new track with a looped Single Instrument in a single Undo/Redo step. */
    bool import_audio_stem_layer(const std::string& event_path, const fs::path& source_path,
                                 std::string* out_sample_id = nullptr, double* out_duration_ms = nullptr);

    // Modulators (Event / Instrument) and Parameter Automation Curves (T10b)
    bool add_modulator(const ItemSelection& owner, const nlohmann::json& modulator);
    bool update_modulator(const ItemSelection& owner, size_t mod_index, const nlohmann::json& modulator);
    bool remove_modulator(const ItemSelection& owner, size_t mod_index);
    std::pair<double, double> get_parameter_domain(const std::string& event_path, const std::string& scope, const std::string& name) const;
    bool add_parameter_automation(const std::string& event_path, int track_index, const std::string& target,
                                  const std::string& param_scope, const std::string& param_name, const nlohmann::json& points);
    bool update_automation_curve(const std::string& event_path, int track_index, size_t auto_index, const nlohmann::json& points);
    bool remove_automation(const std::string& event_path, int track_index, size_t auto_index);
    bool add_effect_automation(const std::string& bus_path, int position, size_t effect_index,
                               const std::string& effect_type, int param_id, const std::string& global_param_name,
                               const nlohmann::json& points);
    bool update_effect_automation_curve(size_t ea_index, const nlohmann::json& points);
    bool remove_effect_automation(size_t ea_index);

    bool add_effect_to_bus(const std::string& bus_path, const std::string& effect_type, int position = 0);
    bool remove_effect_from_bus(const std::string& bus_path, size_t effect_index);
    const nlohmann::json* get_effects(const ItemSelection& target) const;
    /** Returns the parameter values used to initialize an effect of this type. */
    static nlohmann::json default_effect_parameters(const std::string& effect_type);
    bool add_effect(const ItemSelection& target, const std::string& effect_type, int position);
    bool remove_effect(const ItemSelection& target, size_t effect_index);
    bool modify_effect(const ItemSelection& target, size_t effect_index,
                       const std::function<void(nlohmann::json&)>& mutator);

    // Notifications
    void notify_project_changed();

private:
    void push_undo_state();
    void apply_snapshot(const nlohmann::json& snapshot);

    ProjectDocument& doc_;
    ItemSelection current_selection_;
    std::vector<Listener*> listeners_;

    bool in_gesture_{ false };
    nlohmann::json gesture_start_state_;
    std::vector<nlohmann::json> undo_stack_;
    std::vector<nlohmann::json> redo_stack_;
    static constexpr size_t max_history_size_{ 100 };
    uint64_t content_revision_{ 0 };
};

} // namespace oae::studio
