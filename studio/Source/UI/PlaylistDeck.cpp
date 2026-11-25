#include "PlaylistDeck.h"
#include "Theme.h"
#include "Testing/UiTestHooks.h"
#include <miniaudio.h>
#include <array>

namespace oae::studio {
namespace {

constexpr const char* modes[] = { "random", "random_no_repeat", "shuffle", "sequential" };
constexpr const char* scopes[] = { "global", "instance" };

double total_weight(const nlohmann::json& entries)
{
    double total = 0.0;
    for (const auto& entry : entries) total += entry.value("weight", 1.0);
    return total;
}

} // namespace

PlaylistDeck::PlaylistDeck(AuthoringModel& model) : model_(model)
{
    formats_.registerBasicFormats();
    for (juce::Component* control : std::initializer_list<juce::Component*>{
             &status_, &mode_, &scope_, &entriesList_, &add_, &remove_, &up_, &down_,
             &weightLabel_, &pitchLabel_, &volumeLabel_, &delayLabel_, &weight_, &pitch_, &volume_, &delay_ }) {
        addAndMakeVisible(*control);
    }
    mode_.setComponentID("playlist_mode");
    mode_.setTooltip("Playback mode");
    mode_.addItemList({ "Random (weighted)", "Random (no repeat)", "Shuffle", "Sequential" }, 1);
    scope_.setComponentID("playlist_scope");
    scope_.setTooltip("Playlist history scope");
    scope_.addItemList({ "Shared history", "Per-instance history" }, 1);
    entriesList_.setComponentID("playlist_entries");
    entriesList_.setRowHeight(36);
    waveform_cache_.addChangeListener(this);
    add_.setComponentID("playlist_add");
    remove_.setComponentID("playlist_remove");
    up_.setComponentID("playlist_move_up");
    down_.setComponentID("playlist_move_down");
    weight_.setComponentID("playlist_weight");
    pitch_.setComponentID("playlist_pitch_range");
    volume_.setComponentID("playlist_volume_range");
    delay_.setComponentID("playlist_delay_range");

    mode_.onChange = [this] {
        const int i = mode_.getSelectedItemIndex();
        if (i >= 0) edit([i](auto& item) { item["playlist_mode"] = modes[i]; });
    };
    scope_.onChange = [this] {
        const int i = scope_.getSelectedItemIndex();
        if (i >= 0) edit([i](auto& item) { item["playlist_scope"] = scopes[i]; });
    };
    add_.onClick = [this] { show_add_sample_menu(); };
    remove_.onClick = [this] {
        const int row = entriesList_.getSelectedRow();
        if (row >= 0 && entries_.size() > 1) {
            edit([row](auto& item) {
                auto& entries = item["playlist"];
                entries.erase(entries.begin() + row);
            });
        }
    };
    up_.onClick = [this] { move_entry(-1); };
    down_.onClick = [this] { move_entry(1); };

    configure_slider(weight_, 100.0, " %");
    weight_.setRange(0.01, 100.0, 0.01);
    configure_slider(pitch_, 48.0, " st");
    configure_slider(volume_, 80.0, " dB");
    configure_slider(delay_, 5000.0, " ms");
    delay_.setRange(0.0, 5000.0, 1.0);
    weight_.onValueChange = [this] { change_weight(weight_.getValue()); };
    pitch_.onValueChange = [this] {
        const double value = pitch_.getValue();
        edit([value](auto& item) { item["pitch_semitones_random_range"] = value; });
    };
    volume_.onValueChange = [this] {
        const double value = volume_.getValue();
        edit([value](auto& item) { item["volume_db_random_range"] = value; });
    };
    delay_.onValueChange = [this] {
        const int value = juce::roundToInt(delay_.getValue());
        edit([value](auto& item) { item["delay_ms_random_range"] = value; });
    };
    model_.add_listener(this);
    on_selection_changed(model_.get_selection());
}

PlaylistDeck::~PlaylistDeck()
{
    waveform_cache_.removeChangeListener(this);
    entriesList_.setModel(nullptr);
    model_.remove_listener(this);
}

void PlaylistDeck::configure_slider(juce::Slider& slider, double max, const char* suffix)
{
    slider.setRange(0.0, max, 0.01);
    slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 72, 20);
    slider.setTextValueSuffix(suffix);
}

const nlohmann::json* PlaylistDeck::instrument(const nlohmann::json& root, const ItemSelection& selection) const
{
    if (selection.type != ItemType::Event || selection.track_index < 0 || selection.instrument_index < 0) return nullptr;
    const auto events = root.find("events");
    if (events == root.end() || !events->is_array()) return nullptr;
    for (const auto& event : *events) {
        if (event.value("path", "") != selection.id_or_path) continue;
        const auto tracks = event.find("tracks");
        const auto track = static_cast<size_t>(selection.track_index);
        if (tracks == event.end() || !tracks->is_array() || track >= tracks->size()) return nullptr;
        const auto& selected_track = (*tracks)[track];
        const auto instruments = selected_track.find("instruments");
        const auto index = static_cast<size_t>(selection.instrument_index);
        if (instruments == selected_track.end() || !instruments->is_array() || index >= instruments->size()) return nullptr;
        const auto& item = (*instruments)[index];
        const auto kind = item.value("kind", "");
    return (kind == "multi" || kind == "scatterer") ? &item : nullptr;
    }
    return nullptr;
}

bool PlaylistDeck::has_selected_instrument(const ItemSelection& selection) const
{
    return instrument(model_.json(), selection) != nullptr;
}

void PlaylistDeck::edit(const std::function<void(nlohmann::json&)>& change)
{
    if (refreshing_ || !has_selected_instrument(selection_)) return;
    model_.modify([&](auto& root) {
        if (const auto* item = instrument(root, selection_)) change(*const_cast<nlohmann::json*>(item));
    });
}

void PlaylistDeck::on_selection_changed(const ItemSelection& selection)
{
    selection_ = selection;
    refresh(0);
}

void PlaylistDeck::on_project_changed()
{
    refresh(entriesList_.getSelectedRow());
}

void PlaylistDeck::refresh(int selected_row)
{
    const juce::ScopedValueSetter<bool> refreshing(refreshing_, true);
    const auto* item = instrument(model_.json(), selection_);
    entries_ = item ? item->value("playlist", nlohmann::json::array()) : nlohmann::json::array();
    const bool available = item != nullptr;
    status_.setText(available ? juce::String::fromUTF8(selection_.id_or_path.c_str()) :
                    juce::String("Select a multi-instrument in the timeline."), juce::dontSendNotification);
    mode_.setEnabled(available);
    scope_.setEnabled(available);
    add_.setEnabled(available && !model_.get_sample_ids().empty());
    pitch_.setEnabled(available);
    volume_.setEnabled(available);
    delay_.setEnabled(available);
    mode_.setSelectedItemIndex(-1, juce::dontSendNotification);
    scope_.setSelectedItemIndex(-1, juce::dontSendNotification);
    if (item) {
        const auto mode = item->value("playlist_mode", "random");
        for (int i = 0; i < 4; ++i) {
            if (mode == modes[i]) mode_.setSelectedItemIndex(i, juce::dontSendNotification);
        }
        scope_.setSelectedItemIndex(item->value("playlist_scope", "global") == "instance" ? 1 : 0,
                                    juce::dontSendNotification);
    }
    pitch_.setValue(item ? item->value("pitch_semitones_random_range", 0.0) : 0.0, juce::dontSendNotification);
    volume_.setValue(item ? item->value("volume_db_random_range", 0.0) : 0.0, juce::dontSendNotification);
    delay_.setValue(item ? item->value("delay_ms_random_range", 0) : 0, juce::dontSendNotification);
    entry_durations_.clear();
    for (const auto& entry : entries_) {
        const auto sid = entry.value("sample_id", "");
        entry_durations_.push_back(sid.empty() ? juce::String("Event") : duration(sid));
    }
    entriesList_.updateContent();
    if (entries_.empty()) entriesList_.deselectAllRows();
    else entriesList_.selectRow(juce::jlimit(0, getNumRows() - 1, selected_row));
    selectedRowsChanged(entriesList_.getSelectedRow());
    entriesList_.repaint();
}

void PlaylistDeck::show_add_sample_menu()
{
    const auto ids = model_.get_sample_ids();
    const auto targets = model_.get_instrument_targets(selection_.id_or_path, "event");
    const auto selection = selection_;
    const auto revision = model_.get_content_revision();
    juce::PopupMenu menu, sample_menu, event_menu;
    for (size_t i = 0; i < ids.size(); ++i) {
        sample_menu.addItem(static_cast<int>(i) + 1, juce::String::fromUTF8(ids[i].c_str()));
    }
    const int event_offset = static_cast<int>(ids.size()) + 1;
    for (size_t i = 0; i < targets.size(); ++i) {
        event_menu.addItem(event_offset + static_cast<int>(i), juce::String::fromUTF8(targets[i].c_str()));
    }
    menu.addSubMenu("Add Sample", sample_menu, !ids.empty());
    menu.addSubMenu("Add Event", event_menu, !targets.empty());
    UiTestDialogSeam::show_popup_menu("playlist_add_sample", menu,
        juce::PopupMenu::Options().withTargetComponent(add_),
        [self = juce::Component::SafePointer<PlaylistDeck>(this), selection, revision, ids, targets](int result) {
            if (!self || result <= 0 || self->selection_ != selection ||
                self->model_.get_content_revision() != revision) return;
            const auto idx = static_cast<size_t>(result - 1);
            if (idx < ids.size()) {
                self->edit([&](auto& item) {
                    item["playlist"].push_back({ { "sample_id", ids[idx] }, { "weight", 1.0 } });
                });
                self->entriesList_.selectRow(self->getNumRows() - 1);
            } else if (idx - ids.size() < targets.size()) {
                const auto& target = targets[idx - ids.size()];
                self->edit([&](auto& item) {
                    item["playlist"].push_back({ { "target_event", target }, { "weight", 1.0 } });
                });
                self->entriesList_.selectRow(self->getNumRows() - 1);
            }
        });
}

void PlaylistDeck::move_entry(int delta)
{
    const int from = entriesList_.getSelectedRow();
    const int to = from + delta;
    if (from < 0 || to < 0 || to >= getNumRows()) return;
    edit([=](auto& item) { std::swap(item["playlist"][from], item["playlist"][to]); });
    entriesList_.selectRow(to);
}

void PlaylistDeck::change_weight(double percent)
{
    const int row = entriesList_.getSelectedRow();
    if (row < 0 || entries_.size() <= 1) return;
    percent = juce::jlimit(0.01, 99.99, percent);
    edit([=](auto& item) {
        auto& entries = item["playlist"];
        const double selected = entries[row].value("weight", 1.0);
        double other_weight = 0.0;
        for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
            if (i != row) other_weight += entries[i].value("weight", 1.0);
        }
        if (other_weight > 0.0 && selected > 0.0) {
            entries[row]["weight"] = other_weight * percent / (100.0 - percent);
        }
    });
}

fs::path PlaylistDeck::resolve_sample_path(const std::string& sample) const
{
    if (sample.empty()) return {};
    const auto& root = model_.json();
    const auto samples = root.find("samples");
    if (samples == root.end() || !samples->is_array()) return {};
    for (const auto& source : *samples) {
        if (source.value("id", "") == sample) {
            return model_.document().get_project_dir() / fs::u8path(source.value("file", ""));
        }
    }
    return {};
}

juce::String PlaylistDeck::duration(const std::string& sample)
{
    if (sample.empty()) return {};
    const auto path = resolve_sample_path(sample);
    if (path.empty()) return "Missing";
    const juce::File file(juce::String::fromUTF8(path.u8string().c_str()));
    if (!file.existsAsFile()) return "Missing";
    const auto modified = file.getLastModificationTime();
    auto cached = durations_.find(path);
    if (cached != durations_.end() && cached->second.first == modified) return cached->second.second;

    juce::String text = "Unreadable";

    ma_decoder decoder;
    ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 0, 0);
#if defined(_WIN32)
    ma_result res = ma_decoder_init_file_w(path.c_str(), &config, &decoder);
#else
    ma_result res = ma_decoder_init_file(path.u8string().c_str(), &config, &decoder);
#endif
    if (res == MA_SUCCESS) {
        ma_uint64 totalFrames = 0;
        if (ma_decoder_get_length_in_pcm_frames(&decoder, &totalFrames) != MA_SUCCESS || totalFrames == 0) {
            ma_uint64 framesCount = 0;
            ma_uint64 read = 0;
            std::vector<float> probeBuf(4096 * (decoder.outputChannels > 0 ? decoder.outputChannels : 2));
            while (ma_decoder_read_pcm_frames(&decoder, probeBuf.data(), 4096, &read) == MA_SUCCESS && read > 0) {
                framesCount += read;
            }
            totalFrames = framesCount;
        }

        if (decoder.outputSampleRate > 0 && totalFrames > 0) {
            const double duration_ms = (static_cast<double>(totalFrames) / decoder.outputSampleRate) * 1000.0;
            text = juce::String::formatted("%.0f ms", duration_ms);
        }
        ma_decoder_uninit(&decoder);
    } else {
        std::unique_ptr<juce::AudioFormatReader> reader(formats_.createReaderFor(file));
        if (reader && reader->sampleRate > 0.0) {
            text = juce::String::formatted("%.0f ms", 1000.0 * reader->lengthInSamples / reader->sampleRate);
        }
    }

    durations_[path] = { modified, text };
    return text;
}

int PlaylistDeck::getNumRows()
{
    return static_cast<int>(entries_.size());
}

void PlaylistDeck::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == &waveform_cache_) {
        entriesList_.repaint();
    }
}

void PlaylistDeck::paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected)
{
    if (row < 0 || row >= getNumRows()) return;
    if (selected) g.fillAll(juce::Colour(OaeTheme::Colors::accentCyan).withAlpha(0.15f));
    const auto& entry = entries_[row];
    const auto sample_id = entry.value("sample_id", "");
    const auto name = sample_id.empty() ? ("[EVT] " + entry.value("target_event", "")) : sample_id;
    const double total = total_weight(entries_);
    const double fraction = total > 0.0 ? entry.value("weight", 1.0) / total : 0.0;
    auto bounds = juce::Rectangle<int>(0, 0, width, height).reduced(8, 3);
    auto percent = bounds.removeFromRight(62);
    auto bar = bounds.removeFromRight(juce::jmin(120, width / 6)).reduced(3, 8);
    auto duration_bounds = bounds.removeFromRight(68);
    auto wave_bounds = bounds.removeFromRight(juce::jmin(160, juce::jmax(60, width / 4))).reduced(4, 1);

    const auto wave_rect = wave_bounds.toFloat();
    g.setColour(juce::Colour(OaeTheme::Colors::bgSurface));
    g.fillRoundedRectangle(wave_rect, 3.0f);
    g.setColour(juce::Colour(OaeTheme::Colors::borderSubtle));
    g.drawRoundedRectangle(wave_rect, 3.0f, 1.0f);
    if (!sample_id.empty()) {
        const auto sample_path = resolve_sample_path(sample_id);
        if (!sample_path.empty()) {
            auto env = waveform_cache_.get_or_load(sample_path);
            auto inner_wave = wave_rect.reduced(2.0f, 2.0f);
            if (env && !env->is_empty()) {
                env->render_clip(g, inner_wave, 0.0, env->duration_ms, 0.0, 0.0, false, 0.0, 0.0,
                                 juce::Colour(OaeTheme::Colors::accentCyan),
                                 juce::Colour(OaeTheme::Colors::waveformPrimary));
            } else {
                g.setColour(juce::Colour(OaeTheme::Colors::textPrimary).withAlpha(OaeTheme::States::curveGridOpacity));
                g.drawHorizontalLine(static_cast<int>(inner_wave.getCentreY()), inner_wave.getX(), inner_wave.getRight());
            }
        }
    }

    g.setColour(juce::Colour(OaeTheme::Colors::textPrimary));
    g.drawFittedText(juce::String::fromUTF8(name.c_str()), bounds, juce::Justification::centredLeft, 1);
    g.drawText(entry_durations_[row], duration_bounds, juce::Justification::centredRight);
    g.drawText(juce::String::formatted("%.2f%%", fraction * 100.0), percent, juce::Justification::centredRight);
    g.setColour(juce::Colour(OaeTheme::Colors::accentCyan));
    g.fillRect(bar.withWidth(juce::roundToInt(bar.getWidth() * fraction)));
}

void PlaylistDeck::selectedRowsChanged(int row)
{
    const bool selected = row >= 0 && row < getNumRows();
    const bool multiple = getNumRows() > 1;
    remove_.setEnabled(selected && multiple);
    up_.setEnabled(selected && row > 0);
    down_.setEnabled(selected && row + 1 < getNumRows());
    weight_.setEnabled(selected && multiple);
    const double total = total_weight(entries_);
    const double percent = selected && total > 0.0 ? entries_[row].value("weight", 1.0) / total * 100.0 : 0.01;
    weight_.setValue(percent, juce::dontSendNotification);
}

void PlaylistDeck::resized()
{
    const auto bounds = getLocalBounds().reduced(OaeTheme::Spacing::small).toFloat();

    juce::FlexBox headerBox;
    headerBox.flexDirection = juce::FlexBox::Direction::row;
    headerBox.alignItems = juce::FlexBox::AlignItems::center;
    headerBox.items.add(juce::FlexItem(status_).withFlex(1.0f).withHeight(22.0f).withMargin({ 0.0f, 4.0f, 0.0f, 0.0f }));
    headerBox.items.add(juce::FlexItem(scope_).withFlex(0.0f, 1.0f, 155.0f).withMaxWidth(155.0f).withHeight(22.0f).withMargin({ 0.0f, 4.0f, 0.0f, 0.0f }));
    headerBox.items.add(juce::FlexItem(mode_).withFlex(0.0f, 1.0f, 160.0f).withMaxWidth(160.0f).withHeight(22.0f));

    juce::FlexBox buttonsBox;
    buttonsBox.flexDirection = juce::FlexBox::Direction::row;
    buttonsBox.alignItems = juce::FlexBox::AlignItems::center;
    for (auto* button : { &add_, &remove_, &up_, &down_ }) {
        buttonsBox.items.add(juce::FlexItem(*button)
                                 .withFlex(1.0f, 1.0f, 72.0f)
                                 .withMaxWidth(96.0f)
                                 .withHeight(24.0f)
                                 .withMargin({ 0.0f, 4.0f, 0.0f, 0.0f }));
    }

    juce::FlexBox leftColumn;
    leftColumn.flexDirection = juce::FlexBox::Direction::column;
    leftColumn.items.add(juce::FlexItem(entriesList_).withFlex(1.0f));
    leftColumn.items.add(juce::FlexItem(buttonsBox).withFlex(0.0f, 0.0f, 28.0f).withMargin({ 4.0f, 0.0f, 0.0f, 0.0f }));

    std::array<juce::FlexBox, 4> settingRows;
    juce::FlexBox settingsColumn;
    settingsColumn.flexDirection = juce::FlexBox::Direction::column;
    const std::array<std::pair<juce::Label*, juce::Slider*>, 4> rows = {{
        { &weightLabel_, &weight_ },
        { &pitchLabel_, &pitch_ },
        { &volumeLabel_, &volume_ },
        { &delayLabel_, &delay_ }
    }};
    for (size_t i = 0; i < rows.size(); ++i) {
        settingRows[i].flexDirection = juce::FlexBox::Direction::row;
        settingRows[i].alignItems = juce::FlexBox::AlignItems::center;
        settingRows[i].items.add(juce::FlexItem(*rows[i].first).withFlex(0.0f, 1.0f, 112.0f).withMaxWidth(112.0f).withHeight(24.0f));
        settingRows[i].items.add(juce::FlexItem(*rows[i].second).withFlex(1.0f).withHeight(24.0f));
        settingsColumn.items.add(juce::FlexItem(settingRows[i]).withFlex(0.0f, 0.0f, 28.0f));
    }

    juce::FlexBox bodyBox;
    bodyBox.flexDirection = juce::FlexBox::Direction::row;
    bodyBox.items.add(juce::FlexItem(leftColumn).withFlex(1.0f).withMargin({ 0.0f, 8.0f, 0.0f, 0.0f }));
    bodyBox.items.add(juce::FlexItem(settingsColumn).withFlex(0.45f, 1.0f, 310.0f).withMaxWidth(310.0f));

    juce::FlexBox rootBox;
    rootBox.flexDirection = juce::FlexBox::Direction::column;
    rootBox.items.add(juce::FlexItem(headerBox).withFlex(0.0f, 0.0f, 26.0f).withMargin({ 0.0f, 0.0f, 4.0f, 0.0f }));
    rootBox.items.add(juce::FlexItem(bodyBox).withFlex(1.0f));
    rootBox.performLayout(bounds);
}

} // namespace oae::studio
