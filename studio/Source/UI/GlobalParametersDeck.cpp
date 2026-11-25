#include "GlobalParametersDeck.h"
#include "Testing/UiTestHooks.h"
#include <algorithm>
#include <cctype>
#include <cmath>

namespace oae::studio {

namespace {
constexpr int cardPadding = 12;
constexpr int cardHeight = 131;

std::string sanitize_global_param_id(const std::string& raw)
{
    std::string out;
    out.reserve(raw.size());
    for (unsigned char ch : raw) {
        out.push_back((std::isalnum(ch) || ch == '_') ? static_cast<char>(ch) : '_');
    }
    return out.empty() ? "unnamed" : out;
}
} // namespace

GlobalParamCard::GlobalParamCard(const nlohmann::json& spec,
                                 AuditionTransport& transport,
                                 const juce::String& subtitle,
                                 const juce::String& footerText,
                                 juce::Colour accentColour)
    : transport_(transport), accentColour_(accentColour)
{
    name_ = spec.value("name", "");
    const std::string safeName = sanitize_global_param_id(name_);
    setComponentID("deck.param." + safeName + ".card");
    kind_ = spec.value("kind", "continuous");
    velocity_ = spec.value("velocity", 0.0);
    seekSpeed_ = spec.value("seek_speed", 0.0);
    seekSpeedDown_ = spec.value("seek_speed_down", 0.0);
    holdDuringPlayback_ = spec.value("hold_during_playback", false);

    if (spec.contains("labels") && spec["labels"].is_array()) {
        for (const auto& l : spec["labels"]) {
            if (l.is_string()) labels_.push_back(l.get<std::string>());
        }
    }

    const bool labeled = kind_ == "labeled";
    minValue_ = labeled ? 0.0 : spec.value("min", 0.0);
    maxValue_ = labeled && !labels_.empty() ? static_cast<double>(labels_.size() - 1) : spec.value("max", 1.0);
    defaultValue_ = spec.value("default_value", minValue_);
    currentValue_ = transport_.get_global_parameter(name_);
    if (std::abs(currentValue_) < 1e-4 && std::abs(defaultValue_) > 1e-4) {
        currentValue_ = defaultValue_;
        transport_.set_global_parameter(name_, static_cast<float>(defaultValue_));
    }

    nameLabel.setText(juce::String::fromUTF8(name_.c_str()), juce::dontSendNotification);
    nameLabel.setFont(juce::FontOptions(OaeTheme::Typography::paramTitle, juce::Font::bold));
    nameLabel.setColour(juce::Label::textColourId, juce::Colour(OaeTheme::Colors::textPrimary));
    nameLabel.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(nameLabel);

    juce::String sub = subtitle;
    if (sub.isEmpty()) {
        if (labeled && !labels_.empty()) {
            sub = "Enumerado (" + juce::String(labels_.size()) + " estados)";
        } else if (kind_ == "discrete") {
            sub = juce::String::formatted("Discreto (%d .. %d)", static_cast<int>(minValue_), static_cast<int>(maxValue_));
        } else {
            sub = juce::String::formatted("Continuo (%.1f .. %.1f)", minValue_, maxValue_);
        }
    }
    subtitleLabel.setText(sub, juce::dontSendNotification);
    subtitleLabel.setFont(juce::FontOptions(OaeTheme::Typography::caption));
    subtitleLabel.setColour(juce::Label::textColourId, juce::Colour(OaeTheme::Colors::textSecondary));
    subtitleLabel.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(subtitleLabel);

    addAndMakeVisible(statusBadge);

    valueLabel.setFont(juce::FontOptions(OaeTheme::Typography::paramTitle, juce::Font::bold));
    valueLabel.setColour(juce::Label::textColourId, accentColour_);
    valueLabel.setJustificationType(juce::Justification::centredRight);
    valueLabel.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(valueLabel);

    juce::String foot = footerText;
    if (foot.isEmpty()) {
        juce::StringArray parts;
        if (has_velocity()) {
            parts.add((velocity_ > 0.0 ? "Velocity: +" : "Velocity: ") + juce::String(velocity_, 1) + "/s");
        }
        if (has_seek_speed()) {
            const double sv = seekSpeed_ > 0.0 ? seekSpeed_ : seekSpeedDown_;
            parts.add("Seek: " + juce::String(sv, 1) + "/s");
        }
        if (has_hold()) {
            parts.add("Hold ativo");
        }
        foot = parts.isEmpty() ? "Sem modulacao no evento ativo" : parts.joinIntoString("  |  ");
    }
    footerLabel.setText(foot, juce::dontSendNotification);
    footerLabel.setFont(juce::FontOptions(OaeTheme::Typography::caption));
    footerLabel.setColour(juce::Label::textColourId, juce::Colour(OaeTheme::Colors::textSecondary));
    footerLabel.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(footerLabel);

    // Hidden compatibility badges/reset for existing smoke queries
    kindBadge.setText(kind_ == "continuous" ? "Cont" : (kind_ == "discrete" ? "Disc" : "Label"), juce::dontSendNotification);
    addChildComponent(kindBadge);
    addChildComponent(velBadge);
    addChildComponent(seekBadge);
    addChildComponent(holdBadge);
    resetBtn.setComponentID("deck.param." + safeName + ".reset");
    resetBtn.setTooltip("Reset to default (" + juce::String(defaultValue_, 2) + ")");
    resetBtn.onClick = [this] { reset_to_default(); };
    addChildComponent(resetBtn);

    if (kind_ == "labeled" && !labels_.empty()) {
        const int activeIdx = std::clamp(static_cast<int>(std::round(currentValue_)), 0, static_cast<int>(labels_.size()) - 1);
        segmented_ = std::make_unique<OaeSegmentedControl>();
        segmented_->setComponentID("deck.param." + safeName + ".segmented");
        for (size_t i = 0; i < labels_.size(); ++i) {
            segmented_->addOption(juce::String::fromUTF8(labels_[i].c_str()),
                                  "deck.param." + safeName + ".opt." + std::to_string(i),
                                  accentColour_, accentColour_.withAlpha(0.18f));
        }
        for (const auto& btnPtr : segmented_->getOptionButtons()) {
            optionButtonPtrs_.push_back(btnPtr.get());
        }
        segmented_->setSelectedIndex(activeIdx, juce::dontSendNotification);
        segmented_->onSelectionChanged = [this](int idx) {
            currentValue_ = static_cast<double>(idx);
            transport_.set_global_parameter(name_, static_cast<float>(idx));
            refresh_status_and_value();
            repaint();
        };
        addAndMakeVisible(*segmented_);
        valueLabel.setVisible(false);
    } else {
        const double step = kind_ == "discrete" ? 1.0 : 0.0;
        slider_ = std::make_unique<OaeSlider>(juce::Slider::LinearHorizontal, juce::Slider::NoTextBox);
        slider_->setComponentID("deck.param." + safeName + ".slider");
        slider_->getProperties().set("oaeLargeSlider", true);
        slider_->setRange(minValue_, maxValue_, step);
        slider_->setValue(currentValue_, juce::dontSendNotification);
        slider_->setDoubleClickReturnValue(true, defaultValue_, {});
        slider_->setColour(juce::Slider::backgroundColourId, juce::Colour(OaeTheme::Colors::bgDark));
        slider_->setColour(juce::Slider::trackColourId, accentColour_);
        slider_->setColour(juce::Slider::thumbColourId, accentColour_);

        slider_->onValueChange = [this] {
            currentValue_ = slider_->getValue();
            transport_.set_global_parameter(name_, static_cast<float>(currentValue_));
            refresh_status_and_value();
            repaint();
        };
        addAndMakeVisible(*slider_);
    }

    refresh_status_and_value();
}

void GlobalParamCard::refresh_status_and_value()
{
    if (kind_ == "labeled" && labels_.size() == 2) {
        const int idx = std::clamp(static_cast<int>(std::round(currentValue_)), 0, 1);
        const auto lbl = juce::String::fromUTF8(labels_[static_cast<size_t>(idx)].c_str()).toUpperCase();
        statusBadge.setButtonText(lbl);
        statusBadge.setBadgeStyle(idx == 1 ? OaeStatusBadge::Variant::Green : OaeStatusBadge::Variant::Neutral, idx == 1);
    } else if (has_seek_speed()) {
        statusBadge.setButtonText("LIVE SEEK");
        statusBadge.setBadgeStyle(OaeStatusBadge::Variant::Cyan, true);
    } else if (is_modified()) {
        statusBadge.setButtonText("MODIFIED");
        statusBadge.setBadgeStyle(OaeStatusBadge::Variant::Cyan, false);
    } else {
        statusBadge.setButtonText("DEFAULT");
        statusBadge.setBadgeStyle(OaeStatusBadge::Variant::Amber, false);
    }

    if (kind_ == "discrete") {
        valueLabel.setText(juce::String(static_cast<int>(std::round(currentValue_))), juce::dontSendNotification);
    } else {
        valueLabel.setText(juce::String(currentValue_, 2), juce::dontSendNotification);
    }
}

void GlobalParamCard::set_selected(bool selected)
{
    if (selected_ == selected) return;
    selected_ = selected;
    repaint();
}

bool GlobalParamCard::is_modified() const noexcept
{
    return std::abs(currentValue_ - defaultValue_) > 1e-4;
}

void GlobalParamCard::reset_to_default()
{
    currentValue_ = defaultValue_;
    transport_.set_global_parameter(name_, static_cast<float>(defaultValue_));
    if (slider_) {
        slider_->setValue(defaultValue_, juce::dontSendNotification);
    }
    if (segmented_ && !labels_.empty()) {
        const int activeIdx = std::clamp(static_cast<int>(std::round(defaultValue_)), 0, static_cast<int>(labels_.size()) - 1);
        segmented_->setSelectedIndex(activeIdx, juce::dontSendNotification);
    }
    refresh_status_and_value();
    repaint();
}

void GlobalParamCard::update_telemetry(float currentVal, bool /*isPlaying*/)
{
    currentValue_ = currentVal;
    if (slider_ && !slider_->isMouseButtonDown()) {
        slider_->setValue(currentVal, juce::dontSendNotification);
    }
    if (segmented_ && !labels_.empty()) {
        const int activeIdx = std::clamp(static_cast<int>(std::round(currentVal)), 0, static_cast<int>(labels_.size()) - 1);
        segmented_->setSelectedIndex(activeIdx, juce::dontSendNotification);
    }
    refresh_status_and_value();
    repaint();
}

void GlobalParamCard::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour(juce::Colour(OaeTheme::Colors::cardBodyBg));
    g.fillRoundedRectangle(bounds, OaeTheme::Edges::cardCornerRadius);

    g.setColour(selected_ ? juce::Colour(OaeTheme::Colors::accentCyan)
                          : juce::Colour(OaeTheme::Colors::cardBorder));
    g.drawRoundedRectangle(bounds.reduced(0.5f),
                           OaeTheme::Edges::cardCornerRadius,
                           selected_ ? OaeTheme::Edges::selectedBorderStroke : OaeTheme::Edges::borderStroke);
}

void GlobalParamCard::resized()
{
    auto area = getLocalBounds().reduced(cardPadding, 10);
    auto header = area.removeFromTop(32);
    const int badgeW = statusBadge.preferredWidth(64);
    statusBadge.setBounds(header.removeFromRight(badgeW).withSizeKeepingCentre(badgeW, 20));
    header.removeFromRight(6);
    nameLabel.setBounds(header.removeFromTop(17));
    subtitleLabel.setBounds(header);

    area.removeFromTop(10);
    auto mid = area.removeFromTop(28);
    if (segmented_) {
        segmented_->setBounds(mid);
    } else if (slider_) {
        valueLabel.setBounds(mid.removeFromRight(46));
        mid.removeFromRight(6);
        slider_->setBounds(mid);
    }

    area.removeFromTop(10);
    footerLabel.setBounds(area.removeFromBottom(16));
}

void GlobalParamCard::mouseDown(const juce::MouseEvent& event)
{
    if (onCardSelected) {
        onCardSelected(*this);
    }
    if (event.mods.isPopupMenu()) {
        juce::PopupMenu menu;
        menu.addItem(1, "Reset to Default (" + juce::String(defaultValue_, 2) + ")");
        juce::Component::SafePointer<GlobalParamCard> safe(this);
        UiTestDialogSeam::show_popup_menu("global_param_context_menu", menu,
            juce::PopupMenu::Options().withTargetComponent(this),
            [safe](int res) {
                if (safe && res == 1) {
                    safe->reset_to_default();
                }
            });
    }
}

// ---------------------------------------------------------------------------
// GlobalParametersDeck
// ---------------------------------------------------------------------------

GlobalParametersDeck::GlobalParametersDeck(AuthoringModel& model, AuditionTransport& transport)
    : model_(model), transport_(transport)
{
    setComponentID("deck_parameters");
    model_.add_listener(this);
    transport_.add_listener(this);

    deckTitleLabel.setFont(juce::FontOptions(OaeTheme::Typography::caption, juce::Font::bold));
    deckTitleLabel.setColour(juce::Label::textColourId, juce::Colour(OaeTheme::Colors::textMuted));
    addAndMakeVisible(deckTitleLabel);

    searchEditor.setComponentID("ed_params_search");
    searchEditor.setTextToShowWhenEmpty("Search parameter...", juce::Colour(OaeTheme::Colors::textMuted));
    searchEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(OaeTheme::Colors::bgDark));
    searchEditor.onTextChange = [this] { apply_filters(); };
    addAndMakeVisible(searchEditor);

    filterAllBtn.setComponentID("btn_filter_all");
    filterAllBtn.setVariant(OaeToolbarButton::Variant::TintCyan);
    filterAllBtn.onClick = [this] {
        filterMode_ = FilterMode::All;
        filterAllBtn.setVariant(OaeToolbarButton::Variant::TintCyan);
        filterUsedBtn.setVariant(OaeToolbarButton::Variant::Neutral);
        filterModifiedBtn.setVariant(OaeToolbarButton::Variant::Neutral);
        apply_filters();
    };
    addAndMakeVisible(filterAllBtn);

    filterUsedBtn.setComponentID("btn_filter_used");
    filterUsedBtn.onClick = [this] {
        filterMode_ = FilterMode::UsedInEvent;
        filterAllBtn.setVariant(OaeToolbarButton::Variant::Neutral);
        filterUsedBtn.setVariant(OaeToolbarButton::Variant::TintCyan);
        filterModifiedBtn.setVariant(OaeToolbarButton::Variant::Neutral);
        apply_filters();
    };
    addAndMakeVisible(filterUsedBtn);

    filterModifiedBtn.setComponentID("btn_filter_modified");
    filterModifiedBtn.onClick = [this] {
        filterMode_ = FilterMode::Modified;
        filterAllBtn.setVariant(OaeToolbarButton::Variant::Neutral);
        filterUsedBtn.setVariant(OaeToolbarButton::Variant::Neutral);
        filterModifiedBtn.setVariant(OaeToolbarButton::Variant::TintCyan);
        apply_filters();
    };
    addAndMakeVisible(filterModifiedBtn);

    resetAllBtn.setComponentID("btn_reset_all_params");
    resetAllBtn.setTooltip("Reset all global parameters to default");
    resetAllBtn.onClick = [this] { reset_all_to_default(); };
    addAndMakeVisible(resetAllBtn);

    popOutBtn.setComponentID("btn_params_pop_out");
    popOutBtn.setTooltip("Dock Back to Bottom Deck");
    popOutBtn.onClick = [this] {
        if (onPopOutRequested) onPopOutRequested();
    };
    addChildComponent(popOutBtn);

    viewport_.setViewedComponent(&cardsContainer_, false);
    viewport_.setScrollBarsShown(true, false);
    viewport_.setScrollBarThickness(OaeTheme::Spacing::unit);
    addAndMakeVisible(viewport_);

    refresh();
}

GlobalParametersDeck::~GlobalParametersDeck()
{
    stopTimer();
    transport_.remove_listener(this);
    model_.remove_listener(this);
}

void GlobalParametersDeck::set_timers_enabled(bool enabled)
{
    timersEnabled_ = enabled;
    if (timersEnabled_ && isVisible()) {
        startTimerHz(30);
    } else {
        stopTimer();
    }
}

void GlobalParametersDeck::parentHierarchyChanged()
{
    auto* dw = findParentComponentOfClass<juce::DocumentWindow>();
    const bool floating = (dw != nullptr && dw->getName() != "OpenAudio Studio");
    if (isFloating_ != floating) {
        set_floating(floating);
    }
}

void GlobalParametersDeck::visibilityChanged()
{
    if (timersEnabled_ && isVisible()) {
        startTimerHz(30);
    } else {
        stopTimer();
    }
}

void GlobalParametersDeck::timerCallback()
{
    auto* dw = findParentComponentOfClass<juce::DocumentWindow>();
    const bool floating = (dw != nullptr && dw->getName() != "OpenAudio Studio");
    if (isFloating_ != floating) {
        set_floating(floating);
    }
    const bool isPlaying = transport_.get_state() == TransportState::Playing;
    for (auto& card : cards_) {
        const float val = transport_.get_global_parameter(card->get_name());
        card->update_telemetry(val, isPlaying);
    }
}

void GlobalParametersDeck::set_floating(bool floating)
{
    if (isFloating_ == floating) return;
    isFloating_ = floating;
    popOutBtn.setVisible(isFloating_);
    popOutBtn.setButtonText("Dock Back");
    resized();
}

GlobalParamCard* GlobalParametersDeck::find_card(const std::string& name) const
{
    for (const auto& card : cards_) {
        if (card->get_name() == name) return card.get();
    }
    return nullptr;
}

bool GlobalParametersDeck::is_card_visible(const std::string& name) const
{
    for (const auto* card : visibleCards_) {
        if (card && card->get_name() == name) return true;
    }
    return false;
}

void GlobalParametersDeck::on_parameter_changed(const std::string& name, float val, bool is_global)
{
    if (!is_global) return;
    if (auto* card = find_card(name)) {
        card->update_telemetry(val, transport_.get_state() == TransportState::Playing);
    }
}

void GlobalParametersDeck::on_transport_state_changed(TransportState state)
{
    const bool isPlaying = state == TransportState::Playing;
    for (auto& card : cards_) {
        card->update_telemetry(transport_.get_global_parameter(card->get_name()), isPlaying);
    }
}

void GlobalParametersDeck::on_project_changed()
{
    refresh();
}

void GlobalParametersDeck::on_selection_changed(const ItemSelection& selection)
{
    if (selection.type == ItemType::Event && filterMode_ == FilterMode::UsedInEvent) {
        apply_filters();
    }
}

void GlobalParametersDeck::reset_all_to_default()
{
    for (auto& card : cards_) {
        card->reset_to_default();
    }
    apply_filters();
}

std::unordered_set<std::string> GlobalParametersDeck::get_event_used_global_parameters() const
{
    std::unordered_set<std::string> result;
    const std::string evPath = transport_.get_event_path();
    if (evPath.empty()) return result;

    const auto& root = model_.document().get_root_json();
    if (!root.contains("events") || !root["events"].is_array()) return result;

    const nlohmann::json* eventJson = nullptr;
    for (const auto& ev : root["events"]) {
        if (ev.value("path", "") == evPath) {
            eventJson = &ev;
            break;
        }
    }
    if (!eventJson) return result;

    const auto scan_condition = [&](const nlohmann::json& cond) {
        if (cond.contains("terms") && cond["terms"].is_array()) {
            for (const auto& term : cond["terms"]) {
                if (term.contains("param") && term["param"].value("scope", "") == "global") {
                    const std::string gpName = term["param"].value("name", "");
                    if (!gpName.empty()) result.insert(gpName);
                }
            }
        }
    };
    const auto scan_automations = [&](const nlohmann::json& autos) {
        if (autos.is_array()) {
            for (const auto& a : autos) {
                if (a.contains("param") && a["param"].value("scope", "") == "global") {
                    const std::string gpName = a["param"].value("name", "");
                    if (!gpName.empty()) result.insert(gpName);
                }
            }
        }
    };

    if (eventJson->contains("automations")) scan_automations((*eventJson)["automations"]);
    if (eventJson->contains("tracks") && (*eventJson)["tracks"].is_array()) {
        for (const auto& trk : (*eventJson)["tracks"]) {
            if (trk.contains("automations")) scan_automations(trk["automations"]);
            if (trk.contains("instruments") && trk["instruments"].is_array()) {
                for (const auto& inst : trk["instruments"]) {
                    if (inst.contains("condition")) scan_condition(inst["condition"]);
                    const std::string sheetParam = inst.value("sheet_parameter", "");
                    if (!sheetParam.empty()) result.insert(sheetParam);
                }
            }
        }
    }
    if (eventJson->contains("logic_items") && (*eventJson)["logic_items"].is_array()) {
        for (const auto& li : (*eventJson)["logic_items"]) {
            if (li.contains("condition")) scan_condition(li["condition"]);
        }
    }

    return result;
}

void GlobalParametersDeck::refresh()
{
    cards_.clear();
    cardsContainer_.removeAllChildren();

    const auto& root = model_.document().get_root_json();
    if (root.contains("global_parameters") && root["global_parameters"].is_array()) {
        const auto usedSet = get_event_used_global_parameters();
        const auto evPath = transport_.get_event_path();
        size_t idx = 0;
        for (const auto& gp : root["global_parameters"]) {
            const std::string gpName = gp.value("name", "");
            const std::array<std::uint32_t, 4> accents = {
                OaeTheme::Colors::accentCyan,
                OaeTheme::Colors::accentGreen,
                OaeTheme::Colors::accentAmber,
                OaeTheme::Colors::accentPurple
            };
            const auto accent = juce::Colour(accents[idx % accents.size()]);
            juce::String footer;
            if (!evPath.empty() && usedSet.count(gpName) > 0) {
                footer = "Usado em: " + juce::String::fromUTF8(evPath.c_str());
            }
            auto card = std::make_unique<GlobalParamCard>(gp, transport_, juce::String(), footer, accent);
            card->onCardSelected = [this](GlobalParamCard& selectedCard) {
                for (auto& c : cards_) {
                    c->set_selected(c.get() == &selectedCard);
                }
            };
            cardsContainer_.addAndMakeVisible(*card);
            cards_.push_back(std::move(card));
            ++idx;
        }
    }

    apply_filters();
}

void GlobalParametersDeck::apply_filters()
{
    visibleCards_.clear();
    const juce::String query = searchEditor.getText().trim().toLowerCase();
    const auto usedSet = get_event_used_global_parameters();

    int usedCount = 0;
    int modifiedCount = 0;
    for (const auto& card : cards_) {
        if (usedSet.count(card->get_name()) > 0) ++usedCount;
        if (card->is_modified()) ++modifiedCount;
    }
    filterAllBtn.setButtonText("All (" + juce::String(cards_.size()) + ")");
    filterUsedBtn.setButtonText(juce::String::fromUTF8("\xe2\x80\xa2 Used in Event (") + juce::String(usedCount) + ")");
    filterModifiedBtn.setButtonText("Modified (" + juce::String(modifiedCount) + ")");

    for (auto& card : cards_) {
        bool pass = true;

        if (filterMode_ == FilterMode::UsedInEvent && usedSet.count(card->get_name()) == 0) {
            pass = false;
        } else if (filterMode_ == FilterMode::Modified && !card->is_modified()) {
            pass = false;
        }

        if (pass && query.isNotEmpty()) {
            const juce::String cardName = juce::String::fromUTF8(card->get_name().c_str()).toLowerCase();
            if (!cardName.contains(query)) {
                pass = false;
            }
        }

        card->setVisible(pass);
        if (pass) {
            visibleCards_.push_back(card.get());
        }
    }

    resized();
}

void GlobalParametersDeck::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(OaeTheme::Colors::bgDark));
    g.setColour(juce::Colour(OaeTheme::Colors::bgSurface));
    g.fillRect(0, 0, getWidth(), 34);
    g.setColour(juce::Colour(OaeTheme::Colors::borderSubtle));
    g.drawHorizontalLine(33, 0.0f, static_cast<float>(getWidth()));
}

void GlobalParametersDeck::resized()
{
    auto area = getLocalBounds();
    auto topBar = area.removeFromTop(34).reduced(12, 5);

    if (isFloating_) {
        popOutBtn.setBounds(topBar.removeFromRight(80));
        topBar.removeFromRight(6);
    }
    resetAllBtn.setBounds(topBar.removeFromRight(76));
    topBar.removeFromRight(6);
    searchEditor.setBounds(topBar.removeFromRight(std::min(168, std::max(100, topBar.getWidth() / 3))));
    topBar.removeFromRight(8);

    deckTitleLabel.setBounds(topBar.removeFromLeft(152));
    topBar.removeFromLeft(8);
    filterAllBtn.setBounds(topBar.removeFromLeft(filterAllBtn.preferredWidth(54, 14)));
    topBar.removeFromLeft(6);
    filterUsedBtn.setBounds(topBar.removeFromLeft(filterUsedBtn.preferredWidth(116, 14)));
    topBar.removeFromLeft(6);
    filterModifiedBtn.setBounds(topBar.removeFromLeft(filterModifiedBtn.preferredWidth(84, 14)));

    viewport_.setBounds(area);

    const int availableW = std::max(200, area.getWidth() - 16);
    int cols = 1;
    if (availableW >= 1180) cols = 4;
    else if (availableW >= 900) cols = 3;
    else if (availableW >= 600) cols = 2;

    constexpr int gap = 8;
    const int colW = (availableW - gap * (cols - 1)) / cols;
    const int numVisible = static_cast<int>(visibleCards_.size());
    const int rows = (numVisible + cols - 1) / std::max(1, cols);
    const int totalH = std::max(area.getHeight(), rows * (cardHeight + gap) + gap);

    cardsContainer_.setSize(area.getWidth(), totalH);

    for (int i = 0; i < numVisible; ++i) {
        const int r = i / cols;
        const int c = i % cols;
        const int x = gap / 2 + c * (colW + gap);
        const int y = gap + r * (cardHeight + gap);
        visibleCards_[static_cast<size_t>(i)]->setBounds(x, y, colW, cardHeight);
    }
}

} // namespace oae::studio
