#pragma once

#include "Theme.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace oae::studio {

/** Identifiers for native vector glyphs rendered with juce::Path. */
enum class VectorIconType {
    Play,
    Pause,
    Stop,
    Loop,
    KeyOff,
    NewProject,
    OpenProject,
    SaveProject,
    BuildBank,
    Help,
    DockBack,
    LockClosed,
    LockOpen,
    MoreHorizontal,
    ChevronRight,
    ChevronDown
};

/** Helper for building and drawing crisp vector icons scaled to arbitrary bounds. */
class VectorIcons {
public:
    /** Draws the requested vector icon centered inside bounds using the given colour. */
    static void draw_icon(juce::Graphics& g,
                          VectorIconType type,
                          juce::Rectangle<float> bounds,
                          juce::Colour colour);

    /** Builds the filled path component for the requested icon in a 16x16 reference space. */
    static juce::Path create_fill_path(VectorIconType type);

    /** Builds the stroked path component for the requested icon in a 16x16 reference space. */
    static juce::Path create_stroke_path(VectorIconType type);
};

/** Compact button rendering a VectorIconType with optional semantic active background colour. */
class VectorIconButton : public juce::Button {
public:
    enum class Style {
        SolidActive,
        OutlineTint
    };

    /** Creates a vector icon button with an accessible name, icon type, and optional active colour. */
    VectorIconButton(const juce::String& name,
                     VectorIconType iconType,
                     juce::Colour activeBackground = juce::Colour(OaeTheme::Colors::accentCyan));

    /** Selects SolidActive or Figma-aligned OutlineTint rendering. */
    void set_button_style(Style style);

    /** Keeps a subtle semantic tint/icon colour even when inactive in OutlineTint mode. */
    void set_always_tinted(bool alwaysTinted);

    /** Updates the semantic background colour used when the button is in an active/toggled state. */
    void set_active_background(juce::Colour colour);

    /** Sets an explicit active state independent of toggle state (e.g. Play while auditioning). */
    void set_semantic_active(bool active);

    /** Returns whether the button is currently displaying its semantic active state. */
    bool is_semantic_active() const noexcept { return semanticActive_; }

    /** Returns the icon type rendered by this button. */
    VectorIconType icon_type() const noexcept { return iconType_; }

    /** Updates the icon type rendered by this button. */
    void set_icon_type(VectorIconType type);

    void paintButton(juce::Graphics& g,
                     bool shouldDrawButtonAsHighlighted,
                     bool shouldDrawButtonAsDown) override;

private:
    VectorIconType iconType_;
    juce::Colour activeBackground_;
    Style style_{ Style::SolidActive };
    bool alwaysTinted_{ false };
    bool semanticActive_ = false;
};

} // namespace oae::studio
