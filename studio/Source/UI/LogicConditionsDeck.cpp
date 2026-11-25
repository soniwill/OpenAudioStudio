#include "LogicConditionsDeck.h"
#include "Theme.h"
#include <array>

namespace oae::studio {

namespace {

constexpr double timeline_limit_ms = 86400000.0;

template <typename Json>
Json* find_event(Json& root, const std::string& path)
{
    const auto events = root.find("events");
    if (events == root.end() || !events->is_array()) return nullptr;
    const auto event = std::find_if(events->begin(), events->end(), [&](const auto& value) {
        return value.value("path", "") == path && value.value("kind", "event") == "event";
    });
    return event == events->end() ? nullptr : &*event;
}

juce::String item_title(const nlohmann::json& item)
{
    const auto kind = item.value("kind", "");
    const auto position = item.value("position_ms", item.value("start_ms", 0.0));
    juce::String title;
    if (kind == "marker") title = "Marker: " + juce::String::fromUTF8(item.value("name", "").c_str());
    else if (kind == "tempo") title = "Tempo: " + juce::String(item.value("bpm", 120.0), 1) + " BPM";
    else if (kind == "sustain_point") title = "Sustain";
    else if (kind == "loop_region") title = "Loop";
    else title = "Transition to " + juce::String::fromUTF8(item.value("target_marker", "").c_str());
    return title + " @ " + juce::String(position, 1) + " ms";
}

}

LogicConditionsDeck::LogicConditionsDeck(AuthoringModel& model, AuditionTransport& transport)
    : model_(model), transport_(transport)
{
    setOpaque(true);
    setComponentID("deck_logic");
    addAndMakeVisible(viewport_);
    viewport_.setViewedComponent(&form_, false);

    const auto add = [this](juce::Component& control, const char* id) {
        control.setComponentID(id);
        form_.addAndMakeVisible(control);
    };
    const auto add_slider = [&](juce::Slider& slider, const char* id, double low, double high, double step) {
        add(slider, id);
        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 90, 20);
        slider.setRange(low, high, step);
        slider.onDragStart = [this] { model_.begin_gesture(); };
        slider.onDragEnd = [this] { model_.end_gesture(); };
    };
    add(header_, "logic_header");
    add(logic_items_, "logic_items");
    add(add_marker_, "logic_add_marker");
    add(add_loop_, "logic_add_loop");
    add(add_transition_, "logic_add_transition");
    add(add_transition_region_, "logic_add_transition_region");
    add(add_sustain_, "logic_add_sustain");
    add(remove_item_, "logic_remove_item");
    add(position_label_, "logic_position_label");
    add(length_label_, "logic_length_label");
    add(repeats_label_, "logic_repeat_label");
    add_slider(position_, "logic_position", 0, timeline_limit_ms, 0.01);
    add_slider(length_, "logic_length", 0.01, timeline_limit_ms, 0.01);
    add_slider(repeats_, "logic_repeats", -1, 2147483647, 1);
    add(until_key_off_, "logic_until_key_off");

    add(condition_label_, "logic_condition_label");
    add(terms_, "logic_terms");
    add(add_term_, "logic_add_term");
    add(remove_term_, "logic_remove_term");
    add(parameter_, "logic_parameter");
    add(minimum_label_, "logic_minimum_label");
    add(maximum_label_, "logic_maximum_label");
    add_slider(minimum_, "logic_minimum", 0, 1, 0);
    add_slider(maximum_, "logic_maximum", 0, 1, 0);
    add(probability_label_, "logic_probability_label");
    add_slider(probability_, "logic_probability", 0, 100, 0.1);

    add(target_label_, "logic_target_label");
    add(target_, "logic_target");
    add(quantization_label_, "logic_quantization_label");
    add(quantization_, "logic_quantization");
    quantization_.addItemList({ "Immediate", "Beat", "Bar" }, 1);
    add(crossfade_label_, "logic_crossfade_label");
    add_slider(crossfade_, "logic_crossfade", 0.0, 60000.0, 1.0);
    add(marker_name_label_, "logic_marker_name_label");
    add(marker_name_, "logic_marker_name");
    add(bpm_label_, "logic_bpm_label");
    add_slider(bpm_, "logic_bpm", 20.0, 300.0, 0.1);
    add(timesig_label_, "logic_timesig_label");
    add(ts_upper_, "logic_ts_upper");
    for (int i = 1; i <= 16; ++i) ts_upper_.addItem(juce::String(i), i);
    add(ts_lower_, "logic_ts_lower");
    for (int d : { 2, 4, 8, 16 }) ts_lower_.addItem(juce::String(d), d);

    logic_items_.onChange = [this] { select_item(logic_items_.getSelectedItemIndex()); };
    add_marker_.onClick = [this] { add_item("marker"); };
    add_loop_.onClick = [this] { add_item("loop_region"); };
    add_transition_.onClick = [this] { add_item("transition_marker"); };
    add_transition_region_.onClick = [this] { add_item("transition_region"); };
    add_sustain_.onClick = [this] { add_item("sustain_point"); };
    remove_item_.onClick = [this] {
        const auto sel = model_.get_selection();
        if (sel.type == ItemType::Event && sel.logic_index >= 0) {
            if (model_.remove_logic_item(sel.id_or_path, static_cast<size_t>(sel.logic_index))) {
                model_.select({ ItemType::Event, sel.id_or_path, -1, -1, -1 });
            }
        }
    };
    until_key_off_.onClick = [this] {
        edit_item([&](auto& item) { item["until_key_off"] = until_key_off_.getToggleState(); });
    };
    crossfade_.onValueChange = [this] {
        edit_item([&](auto& item) { item["crossfade_ms"] = crossfade_.getValue(); });
    };
    auto commit_marker_name = [this] {
        const auto sel = model_.get_selection();
        if (sel.type == ItemType::Event && sel.logic_index >= 0) {
            const auto name = marker_name_.getText().trim().toStdString();
            if (!name.empty()) {
                model_.rename_marker(sel.id_or_path, static_cast<size_t>(sel.logic_index), name);
            }
        }
    };
    marker_name_.onReturnKey = commit_marker_name;
    marker_name_.onFocusLost = commit_marker_name;
    auto commit_tempo = [this] {
        const auto sel = model_.get_selection();
        if (sel.type == ItemType::Event && sel.logic_index >= 0) {
            const int upper = std::max(1, ts_upper_.getSelectedId());
            const int lower = ts_lower_.getSelectedId() > 0 ? ts_lower_.getSelectedId() : 4;
            model_.update_tempo_marker(sel.id_or_path, static_cast<size_t>(sel.logic_index), bpm_.getValue(), upper, lower);
        }
    };
    bpm_.onValueChange = commit_tempo;
    ts_upper_.onChange = commit_tempo;
    ts_lower_.onChange = commit_tempo;
    position_.onValueChange = [this] {
        edit_item([&](auto& item) {
            const auto key = item.contains("start_ms") ? "start_ms" : "position_ms";
            item[key] = position_.getValue();
        });
    };
    length_.onValueChange = [this] { edit_item([&](auto& item) { item["length_ms"] = length_.getValue(); }); };
    repeats_.onValueChange = [this] { edit_item([&](auto& item) { item["iterations"] = static_cast<int>(repeats_.getValue()); }); };
    probability_.onValueChange = [this] {
        edit_item([&](auto& item) { item["condition"]["probability_pct"] = probability_.getValue(); });
    };
    target_.onChange = [this] {
        const auto index = static_cast<std::size_t>(target_.getSelectedItemIndex());
        if (index < markers_.size()) {
            edit_item([&](auto& item) { item["target_marker"] = markers_[index]; });
        }
    };
    quantization_.onChange = [this] {
        static const char* options[] = { "immediate", "beat", "bar" };
        const auto index = quantization_.getSelectedItemIndex();
        if (index >= 0 && index < 3) edit_item([&](auto& item) { item["quantization"] = options[index]; });
    };
    terms_.onChange = [this] {
        term_index_ = terms_.getSelectedItemIndex();
        refresh();
    };
    add_term_.onClick = [this] {
        if (parameters_.empty()) return;
        const auto& option = parameters_.front();
        edit_item([&](auto& item) {
            auto& terms = item["condition"]["terms"];
            if (terms.is_null()) terms = nlohmann::json::array();
            if (terms.size() >= 4) return;
            term_index_ = static_cast<int>(terms.size());
            terms.push_back({ { "param", option.ref }, { "min", option.minimum }, { "max", option.maximum } });
        });
    };
    remove_term_.onClick = [this] {
        edit_item([&](auto& item) {
            auto& terms = item["condition"]["terms"];
            if (term_index_ >= 0 && static_cast<std::size_t>(term_index_) < terms.size()) {
                terms.erase(terms.begin() + term_index_);
                term_index_ = std::max(0, term_index_ - 1);
            }
        });
    };
    parameter_.onChange = [this] {
        const auto index = static_cast<std::size_t>(parameter_.getSelectedItemIndex());
        if (index >= parameters_.size()) return;
        const auto& option = parameters_[index];
        edit_term([&](auto& term) { term = { { "param", option.ref }, { "min", option.minimum }, { "max", option.maximum } }; });
    };
    minimum_.onValueChange = [this] { edit_term([&](auto& term) { term["min"] = minimum_.getValue(); }); };
    maximum_.onValueChange = [this] { edit_term([&](auto& term) { term["max"] = maximum_.getValue(); }); };

    model_.add_listener(this);
    refresh();
}

LogicConditionsDeck::~LogicConditionsDeck()
{
    model_.remove_listener(this);
    viewport_.setViewedComponent(nullptr);
}

void LogicConditionsDeck::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(OaeTheme::Colors::bgDark));
}

void LogicConditionsDeck::resized()
{
    viewport_.setBounds(getLocalBounds());
    form_.setSize(std::max(760, getWidth() - viewport_.getScrollBarThickness()), 214);

    juce::FlexBox columnsBox;
    columnsBox.flexDirection = juce::FlexBox::Direction::row;
    columnsBox.items.add(juce::FlexItem().withFlex(0.35f).withMargin({ 0.0f, 12.0f, 0.0f, 0.0f }));
    columnsBox.items.add(juce::FlexItem().withFlex(0.36f).withMargin({ 0.0f, 12.0f, 0.0f, 0.0f }));
    columnsBox.items.add(juce::FlexItem().withFlex(0.29f));
    columnsBox.performLayout(form_.getLocalBounds().reduced(OaeTheme::Spacing::small, OaeTheme::Spacing::unit).toFloat());

    const auto make_pair_row = [](juce::FlexBox& row, juce::Component& label, juce::Component& control, float labelWidth) {
        row.flexDirection = juce::FlexBox::Direction::row;
        row.alignItems = juce::FlexBox::AlignItems::center;
        row.items.add(juce::FlexItem(label).withFlex(0.0f, 1.0f, labelWidth).withMaxWidth(labelWidth).withHeight(22.0f).withMargin({ 0.0f, 4.0f, 0.0f, 0.0f }));
        row.items.add(juce::FlexItem(control).withFlex(1.0f).withHeight(22.0f));
    };

    juce::FlexBox hdrRow;
    hdrRow.flexDirection = juce::FlexBox::Direction::row;
    hdrRow.alignItems = juce::FlexBox::AlignItems::center;
    hdrRow.items.add(juce::FlexItem(header_).withFlex(1.0f).withHeight(22.0f).withMargin({ 0.0f, 4.0f, 0.0f, 0.0f }));
    hdrRow.items.add(juce::FlexItem(remove_item_).withFlex(0.0f, 0.5f, 60.0f).withMaxWidth(60.0f).withHeight(22.0f));

    juce::FlexBox toolsRow;
    toolsRow.flexDirection = juce::FlexBox::Direction::row;
    toolsRow.alignItems = juce::FlexBox::AlignItems::center;
    for (auto* button : { &add_marker_, &add_loop_, &add_transition_, &add_transition_region_, &add_sustain_ }) {
        toolsRow.items.add(juce::FlexItem(*button).withFlex(1.0f).withHeight(22.0f).withMargin({ 0.0f, 2.0f, 0.0f, 0.0f }));
    }

    std::array<juce::FlexBox, 6> pairRows;
    make_pair_row(pairRows[0], position_label_, position_, 116.0f);
    make_pair_row(pairRows[1], length_label_, length_, 116.0f);
    make_pair_row(pairRows[2], repeats_label_, repeats_, 116.0f);
    make_pair_row(pairRows[3], minimum_label_, minimum_, 124.0f);
    make_pair_row(pairRows[4], maximum_label_, maximum_, 124.0f);
    make_pair_row(pairRows[5], probability_label_, probability_, 124.0f);

    juce::FlexBox col1;
    col1.flexDirection = juce::FlexBox::Direction::column;
    col1.items.add(juce::FlexItem(hdrRow).withFlex(0.0f, 0.0f, 24.0f));
    col1.items.add(juce::FlexItem(logic_items_).withFlex(0.0f, 0.0f, 24.0f).withMargin({ 1.0f, 2.0f, 1.0f, 2.0f }));
    col1.items.add(juce::FlexItem(toolsRow).withFlex(0.0f, 0.0f, 24.0f));
    col1.items.add(juce::FlexItem(pairRows[0]).withFlex(0.0f, 0.0f, 24.0f));
    col1.items.add(juce::FlexItem(pairRows[1]).withFlex(0.0f, 0.0f, 24.0f));
    col1.items.add(juce::FlexItem(pairRows[2]).withFlex(0.0f, 0.0f, 24.0f));
    col1.items.add(juce::FlexItem(until_key_off_).withFlex(0.0f, 0.0f, 24.0f).withMargin({ 1.0f, 2.0f, 1.0f, 2.0f }));
    col1.performLayout(columnsBox.items[0].currentBounds);

    juce::FlexBox termRow;
    termRow.flexDirection = juce::FlexBox::Direction::row;
    termRow.alignItems = juce::FlexBox::AlignItems::center;
    termRow.items.add(juce::FlexItem(terms_).withFlex(1.0f).withHeight(22.0f).withMargin({ 0.0f, 4.0f, 0.0f, 0.0f }));
    termRow.items.add(juce::FlexItem(add_term_).withFlex(0.45f, 1.0f, 95.0f).withMaxWidth(95.0f).withHeight(22.0f).withMargin({ 0.0f, 4.0f, 0.0f, 0.0f }));
    termRow.items.add(juce::FlexItem(remove_term_).withFlex(0.35f, 1.0f, 80.0f).withHeight(22.0f));

    juce::FlexBox col2;
    col2.flexDirection = juce::FlexBox::Direction::column;
    col2.items.add(juce::FlexItem(condition_label_).withFlex(0.0f, 0.0f, 24.0f));
    col2.items.add(juce::FlexItem(termRow).withFlex(0.0f, 0.0f, 24.0f));
    col2.items.add(juce::FlexItem(parameter_).withFlex(0.0f, 0.0f, 24.0f).withMargin({ 1.0f, 2.0f, 1.0f, 2.0f }));
    col2.items.add(juce::FlexItem(pairRows[3]).withFlex(0.0f, 0.0f, 24.0f));
    col2.items.add(juce::FlexItem(pairRows[4]).withFlex(0.0f, 0.0f, 24.0f));
    col2.items.add(juce::FlexItem(pairRows[5]).withFlex(0.0f, 0.0f, 24.0f));
    col2.performLayout(columnsBox.items[1].currentBounds);

    const auto col3Bounds = columnsBox.items[2].currentBounds;
    juce::FlexBox col3Trans;
    col3Trans.flexDirection = juce::FlexBox::Direction::column;
    for (auto* c : std::initializer_list<juce::Component*>{
             &target_label_, &target_, &quantization_label_, &quantization_, &crossfade_label_, &crossfade_ }) {
        col3Trans.items.add(juce::FlexItem(*c).withFlex(0.0f, 0.0f, 24.0f).withMargin({ 1.0f, 2.0f, 1.0f, 2.0f }));
    }
    col3Trans.performLayout(col3Bounds);

    juce::FlexBox col3Marker;
    col3Marker.flexDirection = juce::FlexBox::Direction::column;
    col3Marker.items.add(juce::FlexItem(marker_name_label_).withFlex(0.0f, 0.0f, 24.0f).withMargin({ 1.0f, 2.0f, 1.0f, 2.0f }));
    col3Marker.items.add(juce::FlexItem(marker_name_).withFlex(0.0f, 0.0f, 24.0f).withMargin({ 1.0f, 2.0f, 1.0f, 2.0f }));
    col3Marker.performLayout(col3Bounds);

    juce::FlexBox tsRow;
    tsRow.flexDirection = juce::FlexBox::Direction::row;
    tsRow.alignItems = juce::FlexBox::AlignItems::center;
    tsRow.items.add(juce::FlexItem(ts_upper_).withFlex(0.0f, 1.0f, 64.0f).withMaxWidth(64.0f).withHeight(22.0f).withMargin({ 0.0f, 4.0f, 0.0f, 0.0f }));
    tsRow.items.add(juce::FlexItem(ts_lower_).withFlex(0.0f, 1.0f, 64.0f).withMaxWidth(64.0f).withHeight(22.0f));

    juce::FlexBox col3Tempo;
    col3Tempo.flexDirection = juce::FlexBox::Direction::column;
    col3Tempo.items.add(juce::FlexItem(bpm_label_).withFlex(0.0f, 0.0f, 24.0f).withMargin({ 1.0f, 2.0f, 1.0f, 2.0f }));
    col3Tempo.items.add(juce::FlexItem(bpm_).withFlex(0.0f, 0.0f, 24.0f).withMargin({ 1.0f, 2.0f, 1.0f, 2.0f }));
    col3Tempo.items.add(juce::FlexItem(timesig_label_).withFlex(0.0f, 0.0f, 24.0f).withMargin({ 1.0f, 2.0f, 1.0f, 2.0f }));
    col3Tempo.items.add(juce::FlexItem(tsRow).withFlex(0.0f, 0.0f, 24.0f));
    col3Tempo.performLayout(col3Bounds);
}

void LogicConditionsDeck::on_project_changed()
{
    refresh();
}

void LogicConditionsDeck::on_selection_changed(const ItemSelection&)
{
    term_index_ = 0;
    refresh();
}

void LogicConditionsDeck::select_item(int index)
{
    auto selection = model_.get_selection();
    if (selection.type != ItemType::Event) return;
    selection.track_index = -1;
    selection.instrument_index = -1;
    selection.logic_index = index;
    model_.select(selection);
}

double LogicConditionsDeck::insertion_time() const
{
    return std::clamp(transport_.get_current_position_ms(), 0.0, timeline_limit_ms);
}

void LogicConditionsDeck::add_item(const std::string& kind)
{
    const auto selection = model_.get_selection();
    const auto* event = find_event(std::as_const(model_).json(), selection.id_or_path);
    if (selection.type != ItemType::Event || !event) return;
    const auto items = event->find("logic_items");
    const int new_index = items == event->end() ? 0 : static_cast<int>(items->size());
    const double position = insertion_time();

    if (kind == "marker") {
        if (!model_.add_marker(selection.id_or_path, "Marker", position)) return;
    } else if (kind == "loop_region") {
        if (!model_.add_loop_region(selection.id_or_path, position, 2000.0)) return;
    } else if (kind == "transition_marker") {
        const auto target_index = static_cast<std::size_t>(target_.getSelectedItemIndex());
        if (markers_.empty()) return;
        const auto& dest = target_index < markers_.size() ? markers_[target_index] : markers_.front();
        if (!model_.add_transition_marker(selection.id_or_path, position, dest)) return;
    } else if (kind == "transition_region") {
        const auto target_index = static_cast<std::size_t>(target_.getSelectedItemIndex());
        if (markers_.empty()) return;
        const auto& dest = target_index < markers_.size() ? markers_[target_index] : markers_.front();
        if (!model_.add_transition_region(selection.id_or_path, position, 2000.0, dest)) return;
    } else if (kind == "sustain_point") {
        if (!model_.add_sustain_point(selection.id_or_path, position)) return;
    } else {
        return;
    }
    select_item(new_index);
}

void LogicConditionsDeck::edit_item(const std::function<void(nlohmann::json&)>& edit)
{
    const auto selection = model_.get_selection();
    const auto* event = find_event(std::as_const(model_).json(), selection.id_or_path);
    if (selection.type != ItemType::Event || !event || selection.logic_index < 0) return;
    const auto items = event->find("logic_items");
    const auto index = static_cast<std::size_t>(selection.logic_index);
    if (items == event->end() || !items->is_array() || index >= items->size()) return;

    auto updated = (*items)[index];
    edit(updated);
    if (updated != (*items)[index]) {
        model_.modify([&](auto& root) {
            (*find_event(root, selection.id_or_path))["logic_items"][index] = std::move(updated);
        });
    }
}

void LogicConditionsDeck::edit_term(const std::function<void(nlohmann::json&)>& edit)
{
    edit_item([&](auto& item) {
        auto& terms = item["condition"]["terms"];
        if (term_index_ >= 0 && static_cast<std::size_t>(term_index_) < terms.size()) {
            edit(terms[static_cast<std::size_t>(term_index_)]);
        }
    });
}

void LogicConditionsDeck::refresh_parameters(const nlohmann::json& event)
{
    const auto add_options = [&](const nlohmann::json& owner, const char* key, const char* scope) {
        const auto definitions = owner.find(key);
        if (definitions == owner.end() || !definitions->is_array()) return;
        for (const auto& definition : *definitions) {
            const auto name = definition.value("name", "");
            const auto kind = definition.value("kind", "");
            const bool labeled = kind == "labeled";
            const bool discrete = kind == "discrete" || labeled;
            const double low = labeled ? 0.0 : definition.value("min", 0.0);
            const double high = labeled ? static_cast<double>(definition["labels"].size()) - 1.0
                                        : definition.value("max", 1.0);
            parameters_.push_back({ { { "scope", scope }, { "name", name } }, low, high, discrete ? 1.0 : 0.0 });
            parameter_.addItem(juce::String::fromUTF8(name.c_str()) + " (" + scope + ")", static_cast<int>(parameters_.size()));
        }
    };
    add_options(event, "parameters", "local");
    add_options(std::as_const(model_).json(), "global_parameters", "global");
}

void LogicConditionsDeck::refresh()
{
    const auto& selection = model_.get_selection();
    const auto* event = selection.type == ItemType::Event
        ? find_event(std::as_const(model_).json(), selection.id_or_path) : nullptr;
    const nlohmann::json* item = nullptr;

    logic_items_.clear(juce::dontSendNotification);
    logic_items_.setTextWhenNothingSelected(event ? "Select a logic item" : "Select an event");
    target_.clear(juce::dontSendNotification);
    target_.setTextWhenNothingSelected("No destination selected");
    markers_.clear();
    parameters_.clear();
    parameter_.clear(juce::dontSendNotification);
    if (event) {
        refresh_parameters(*event);
        const auto items = event->find("logic_items");
        if (items != event->end() && items->is_array()) {
            for (std::size_t i = 0; i < items->size(); ++i) {
                const auto& value = (*items)[i];
                logic_items_.addItem(item_title(value), static_cast<int>(i + 1));
                if (value.value("kind", "") == "marker") {
                    markers_.push_back(value.value("name", ""));
                    target_.addItem(juce::String::fromUTF8(markers_.back().c_str()), static_cast<int>(markers_.size()));
                }
            }
            if (selection.logic_index >= 0 && static_cast<std::size_t>(selection.logic_index) < items->size()) {
                item = &(*items)[static_cast<std::size_t>(selection.logic_index)];
                logic_items_.setSelectedId(selection.logic_index + 1, juce::dontSendNotification);
            }
        }
    }

    const auto kind = item ? item->value("kind", "") : "";
    const bool is_marker = kind == "marker";
    const bool is_tempo = kind == "tempo";
    const bool loop = kind == "loop_region";
    const bool transition = kind == "transition_marker" || kind == "transition_region";
    const bool region = loop || kind == "transition_region";
    logic_items_.setEnabled(logic_items_.getNumItems() > 0);
    remove_item_.setEnabled(item != nullptr);
    add_marker_.setEnabled(event != nullptr);
    add_loop_.setEnabled(event != nullptr);
    add_sustain_.setEnabled(event != nullptr);
    add_transition_.setEnabled(!markers_.empty());
    add_transition_region_.setEnabled(!markers_.empty());
    add_transition_.setTooltip("Create a transition marker at the current position. Add a destination marker first.");
    add_transition_region_.setTooltip("Create a transition region at the current position. Add a destination marker first.");

    const double position = item ? item->value("position_ms", item->value("start_ms", 0.0)) : 0.0;
    const double length = region ? item->value("length_ms", 1.0) : 0.0;
    position_.setRange(0.0, std::max(0.0, timeline_limit_ms - length), 0.01);
    position_.setValue(position, juce::dontSendNotification);
    position_.setEnabled(item != nullptr);
    length_.setRange(0.01, std::max(0.01, timeline_limit_ms - position), 0.01);
    length_.setValue(length, juce::dontSendNotification);
    length_.setEnabled(region);
    repeats_.setValue(loop ? item->value("iterations", -1) : 0, juce::dontSendNotification);
    const bool key_off = loop && item->value("until_key_off", false);
    repeats_.setEnabled(loop && !key_off);
    until_key_off_.setToggleState(key_off, juce::dontSendNotification);
    until_key_off_.setEnabled(loop);

    const bool show_trans = !is_marker && !is_tempo;
    target_label_.setVisible(show_trans);
    target_.setVisible(show_trans);
    quantization_label_.setVisible(show_trans);
    quantization_.setVisible(show_trans);
    crossfade_label_.setVisible(show_trans);
    crossfade_.setVisible(show_trans);

    target_.setEnabled(transition && !markers_.empty());
    quantization_.setEnabled(transition);
    crossfade_.setEnabled(transition);
    crossfade_.setValue(transition ? item->value("crossfade_ms", 0.0) : 0.0, juce::dontSendNotification);
    const auto destination = transition ? item->value("target_marker", "") : "";
    const auto marker = std::find(markers_.begin(), markers_.end(), destination);
    if (marker != markers_.end()) target_.setSelectedItemIndex(static_cast<int>(marker - markers_.begin()), juce::dontSendNotification);
    const auto quantization = transition ? item->value("quantization", "immediate") : "";
    quantization_.setSelectedId(quantization == "bar" ? 3 : quantization == "beat" ? 2 : transition ? 1 : 0, juce::dontSendNotification);

    marker_name_label_.setVisible(is_marker);
    marker_name_.setVisible(is_marker);
    if (is_marker) {
        marker_name_.setText(juce::String::fromUTF8(item->value("name", "").c_str()), juce::dontSendNotification);
    }

    bpm_label_.setVisible(is_tempo);
    bpm_.setVisible(is_tempo);
    timesig_label_.setVisible(is_tempo);
    ts_upper_.setVisible(is_tempo);
    ts_lower_.setVisible(is_tempo);
    if (is_tempo) {
        bpm_.setValue(item->value("bpm", 120.0), juce::dontSendNotification);
        ts_upper_.setSelectedId(std::clamp(item->value("ts_upper", 4), 1, 16), juce::dontSendNotification);
        const int lower = item->value("ts_lower", 4);
        ts_lower_.setSelectedId((lower == 2 || lower == 4 || lower == 8 || lower == 16) ? lower : 4, juce::dontSendNotification);
    }

    refresh_condition(loop || transition ? item : nullptr);
}

void LogicConditionsDeck::refresh_condition(const nlohmann::json* item)
{
    const auto condition = item ? item->value("condition", nlohmann::json::object()) : nlohmann::json::object();
    const auto terms = condition.value("terms", nlohmann::json::array());
    probability_.setValue(condition.value("probability_pct", 100.0), juce::dontSendNotification);
    probability_.setEnabled(item != nullptr);
    terms_.clear(juce::dontSendNotification);
    terms_.setTextWhenNothingSelected("No parameter conditions");
    for (std::size_t i = 0; i < terms.size(); ++i) {
        const auto& ref = terms[i]["param"];
        terms_.addItem(juce::String::fromUTF8(ref.value("name", "").c_str()) + " (" +
                       juce::String::fromUTF8(ref.value("scope", "").c_str()) + ")", static_cast<int>(i + 1));
    }
    const auto term_count = static_cast<int>(terms.size());
    terms_.setEnabled(term_count > 0);
    add_term_.setEnabled(item && !parameters_.empty() && term_count < 4);
    remove_term_.setEnabled(term_count > 0);
    parameter_.setTextWhenNothingSelected("Select a condition");
    parameter_.setEnabled(term_count > 0 && !parameters_.empty());
    minimum_.setEnabled(false);
    maximum_.setEnabled(false);
    if (term_count == 0) {
        minimum_.setValue(0, juce::dontSendNotification);
        maximum_.setValue(0, juce::dontSendNotification);
        return;
    }

    term_index_ = std::clamp(term_index_, 0, term_count - 1);
    terms_.setSelectedId(term_index_ + 1, juce::dontSendNotification);
    const auto& term = terms[static_cast<std::size_t>(term_index_)];
    const auto option = std::find_if(parameters_.begin(), parameters_.end(), [&](const auto& p) {
        return p.ref == term["param"];
    });
    if (option == parameters_.end()) return;
    parameter_.setSelectedItemIndex(static_cast<int>(option - parameters_.begin()), juce::dontSendNotification);
    const double low = term.value("min", option->minimum);
    const double high = term.value("max", option->maximum);
    minimum_.setRange(option->minimum, high, option->interval);
    maximum_.setRange(low, option->maximum, option->interval);
    minimum_.setValue(low, juce::dontSendNotification);
    maximum_.setValue(high, juce::dontSendNotification);
    minimum_.setEnabled(option->minimum < high);
    maximum_.setEnabled(low < option->maximum);
}

} // namespace oae::studio
