#include "ProjectToolbar.h"

namespace oae::studio {

namespace {
constexpr int kButtonHeight = 24;
constexpr int kBrandWidth = 74;
constexpr int kGap = OaeTheme::Spacing::unit;
} // namespace

ProjectToolbar::ProjectToolbar()
{
    setComponentID("project_toolbar");

    gripHandle_.onDoubleClick = [this] { set_minimized(!minimized_); };
    gripHandle_.onToggleMinimized = [this] { set_minimized(!minimized_); };
    addAndMakeVisible(gripHandle_);

    brandLabel_.setFont(juce::FontOptions(OaeTheme::Typography::compact, juce::Font::bold));
    brandLabel_.setColour(juce::Label::textColourId, juce::Colour(OaeTheme::Colors::accentCyan));
    brandLabel_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(brandLabel_);

    newProjectButton.setComponentID("btn_new_project");
    newProjectButton.setTooltip("New Project (Ctrl+N)");

    openProjectButton.setComponentID("btn_open_project");
    openProjectButton.setTooltip("Open Project (Ctrl+O)");

    saveProjectButton.setComponentID("btn_save_project");
    saveProjectButton.setTooltip("Save Project (Ctrl+S)");

    buildBankButton.setComponentID("btn_build_bank");
    buildBankButton.setTooltip("Build Bank (F7)");

    helpButton.setComponentID("btn_help");
    helpButton.setTooltip("Help Manual (F1)");

    addAndMakeVisible(newProjectButton);
    addAndMakeVisible(openProjectButton);
    addAndMakeVisible(saveProjectButton);
    addAndMakeVisible(buildBankButton);
    addAndMakeVisible(helpButton);
}

int ProjectToolbar::preferred_width() const noexcept
{
    int width = GripHandle::kDefaultWidth + OaeTheme::Spacing::small * 2 + kBrandWidth + kGap;
    if (!minimized_) {
        width += newProjectButton.preferredWidth(42, 14) + kGap;
        width += openProjectButton.preferredWidth(46, 14) + kGap;
    }
    width += saveProjectButton.preferredWidth(44, 14) + kGap;
    width += buildBankButton.preferredWidth(104, 16);
    if (!minimized_) {
        width += kGap + 24;
    }
    return width;
}

void ProjectToolbar::set_minimized(bool minimized)
{
    if (minimized_ == minimized) {
        return;
    }
    minimized_ = minimized;
    gripHandle_.set_minimized(minimized_);
    newProjectButton.setVisible(!minimized_);
    openProjectButton.setVisible(!minimized_);
    helpButton.setVisible(!minimized_);
    resized();
    if (onLayoutChanged) {
        onLayoutChanged();
    }
}

void ProjectToolbar::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(OaeTheme::Colors::bgSurface));
    g.setColour(juce::Colour(OaeTheme::Colors::borderSubtle));
    g.drawLine(0.0f,
               static_cast<float>(getHeight()) - 0.5f,
               static_cast<float>(getWidth()),
               static_cast<float>(getHeight()) - 0.5f,
               1.0f);
}

void ProjectToolbar::resized()
{
    auto area = getLocalBounds();
    gripHandle_.setBounds(area.removeFromLeft(GripHandle::kDefaultWidth));
    area.reduce(OaeTheme::Spacing::small, (area.getHeight() - kButtonHeight) / 2);

    brandLabel_.setBounds(area.removeFromLeft(kBrandWidth).withHeight(kButtonHeight));
    area.removeFromLeft(kGap);

    auto placeButton = [&](juce::Button& btn, int width) {
        if (!btn.isVisible()) {
            return;
        }
        btn.setBounds(area.removeFromLeft(width).withHeight(kButtonHeight));
        area.removeFromLeft(kGap);
    };

    placeButton(newProjectButton, newProjectButton.preferredWidth(42, 14));
    placeButton(openProjectButton, openProjectButton.preferredWidth(46, 14));
    placeButton(saveProjectButton, saveProjectButton.preferredWidth(44, 14));
    placeButton(buildBankButton, buildBankButton.preferredWidth(104, 16));
    placeButton(helpButton, 24);
}

} // namespace oae::studio
