#pragma once

#include "OaeSlider.h"
#include "Theme.h"

#include <functional>
#include <memory>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

namespace oae::studio {

/** Standard Studio toolbar button with semantic outline/tint variants. */
class OaeToolbarButton : public juce::TextButton {
public:
    enum class Variant {
        Neutral,
        AccentCyan,
        TintCyan,
        AccentPurple
    };

    explicit OaeToolbarButton(const juce::String& text = {}, Variant variant = Variant::Neutral);

    void setVariant(Variant variant);
    Variant getVariant() const noexcept { return variant_; }

    int preferredWidth(int minWidth = 44, int horizontalPadding = 16) const;

    void paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown) override;

private:
    Variant variant_{ Variant::Neutral };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OaeToolbarButton)
};

/** Pill-shaped status badge backed by juce::TextButton for MCP/smoke compatibility. */
class OaeStatusBadge : public juce::TextButton {
public:
    enum class Variant {
        Cyan,
        Green,
        Amber,
        Purple,
        Neutral
    };

    explicit OaeStatusBadge(const juce::String& text = {},
                            Variant variant = Variant::Neutral,
                            bool showDot = false);

    void setBadgeStyle(Variant variant, bool showDot = false);
    void setInteractive(bool interactive);

    int preferredWidth(int minWidth = 44) const;

    void paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown) override;

private:
    Variant variant_{ Variant::Neutral };
    bool showDot_{ false };
    bool interactive_{ false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OaeStatusBadge)
};

/** Segmented pill switcher hosting child TextButtons for MCP and keyboard support. */
class OaeSegmentedControl : public juce::Component {
public:
    OaeSegmentedControl();

    void addOption(const juce::String& text,
                   const juce::String& componentId = {},
                   juce::Colour activeAccent = juce::Colour(OaeTheme::Colors::accentCyan),
                   juce::Colour activeTint = juce::Colour(OaeTheme::Colors::tintCyan));
    void clearOptions();

    void setSelectedIndex(int index, juce::NotificationType notification = juce::dontSendNotification);
    int getSelectedIndex() const noexcept { return selectedIndex_; }

    const std::vector<std::unique_ptr<juce::TextButton>>& getOptionButtons() const noexcept { return options_; }

    std::function<void(int index)> onSelectionChanged;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    struct OptionEntry {
        juce::Colour activeAccent;
        juce::Colour activeTint;
    };

    std::vector<std::unique_ptr<juce::TextButton>> options_;
    std::vector<OptionEntry> optionStyles_;
    int selectedIndex_{ 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OaeSegmentedControl)
};

/** Compact ValueBox-styled numeric slider preserving OaeSlider 1:1 drag, Shift fine drag and double-click reset. */
class OaeValueBoxSlider : public OaeSlider {
public:
    explicit OaeValueBoxSlider(const juce::String& componentName = {});

    void setDisplayPrefix(const juce::String& prefix);

    void paint(juce::Graphics& g) override;

private:
    juce::String displayPrefix_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OaeValueBoxSlider)
};

} // namespace oae::studio
