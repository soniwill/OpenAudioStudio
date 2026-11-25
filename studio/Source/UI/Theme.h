#pragma once

#include <array>
#include <cctype>
#include <cstdint>
#include <string>

#include <juce_graphics/juce_graphics.h>

#include "Audio/AcousticMeterProcessor.h"

namespace oae::studio {

/** Semantic colors, layout metrics, and meter display settings for the Studio UI. */
struct OaeTheme {
    struct Colors {
        static constexpr std::uint32_t bgDark          = 0xff161719;
        static constexpr std::uint32_t bgSurface       = 0xff202225;
        static constexpr std::uint32_t bgSurfaceRaised = 0xff282b30;
        static constexpr std::uint32_t borderSubtle    = 0xff2f3237;
        static constexpr std::uint32_t borderActive    = 0xff40444b;
        static constexpr std::uint32_t accentCyan      = 0xff00f5d4;
        static constexpr std::uint32_t accentAmber     = 0xffffb703;
        static constexpr std::uint32_t accentCoral     = 0xffff5964;
        static constexpr std::uint32_t accentGreen     = 0xff38b000;
        static constexpr std::uint32_t accentPurple    = 0xffb5179e;
        static constexpr std::uint32_t tintCyan        = 0x2900f5d4;
        static constexpr std::uint32_t tintGreen       = 0x3838b000;
        static constexpr std::uint32_t tintAmber       = 0x2effb703;
        static constexpr std::uint32_t tintPurple      = 0x3db5179e;
        static constexpr std::uint32_t textPrimary     = 0xfff0f2f5;
        static constexpr std::uint32_t textSecondary   = 0xffa0a4ab;
        static constexpr std::uint32_t textMuted       = 0xff6b7280;
        static constexpr std::uint32_t indicator       = 0xffffffff;
        static constexpr std::uint32_t clipSingle        = 0xff1d4e89;
        static constexpr std::uint32_t clipMulti         = 0xff5c2751;
        static constexpr std::uint32_t clipScatterer     = 0xff2a9d8f;
        static constexpr std::uint32_t clipProgrammer    = 0xff3b6e8c;
        static constexpr std::uint32_t clipEvent         = 0xffd06224;
        static constexpr std::uint32_t clipCommand       = 0xffa27b5c;
        static constexpr std::uint32_t clipFallback      = 0xff3f4248;
        static constexpr std::uint32_t dockHeaderBg      = 0xff25282c;
        static constexpr std::uint32_t dockTabInactiveBg = 0xff2d3035;
        static constexpr std::uint32_t dockTabActiveBg   = 0xff464b53;
        static constexpr std::uint32_t dockTabHoverBg    = 0xff383d44;
        static constexpr std::uint32_t dockTabBorder     = 0xff3a3e44;
        static constexpr std::uint32_t dockTabText       = 0xffb0b4ba;
        static constexpr std::uint32_t dockTabActiveText = 0xfff2f3f5;
        static constexpr std::uint32_t dockTabFocus      = 0xffffffff;
        static constexpr std::uint32_t waveformPrimary   = 0xff80ffdb;
        static constexpr std::uint32_t waveformEvent     = 0xffffd166;
        static constexpr std::uint32_t waveformEventPeak = 0xfffff3b0;
        static constexpr std::uint32_t trackFaderAccent  = 0xff3a86ff;
        static constexpr std::uint32_t trackPanAccent    = 0xff8338ec;
        static constexpr std::uint32_t cardHeaderBg      = 0xff232733;
        static constexpr std::uint32_t cardBodyBg        = 0xff1a1d26;
        static constexpr std::uint32_t cardBorder        = 0xff2c3140;
        static constexpr std::uint32_t clipTitleTextDark = 0xff0b0c10;
    };

    /** Curated cyclically-assigned color palette for timeline tracks. */
    struct TrackColors {
        static constexpr size_t paletteSize = 8;
        static constexpr std::array<std::uint32_t, paletteSize> palette = {
            0xFF00B4D8u,
            0xFFF77F00u,
            0xFF7209B7u,
            0xFF2EC4B6u,
            0xFFE71D36u,
            0xFF4361EEu,
            0xFFFFBE0Bu,
            0xFFF72585u
        };
        static constexpr std::array<const char*, paletteSize> hexPalette = {
            "#00B4D8",
            "#F77F00",
            "#7209B7",
            "#2EC4B6",
            "#E71D36",
            "#4361EE",
            "#FFBE0B",
            "#F72585"
        };
        static constexpr std::array<const char*, paletteSize> labelPalette = {
            "Cyan",
            "Orange",
            "Purple",
            "Teal",
            "Red",
            "Indigo",
            "Amber",
            "Magenta"
        };

        /** Returns the track colour corresponding to a cyclic track index. */
        static juce::Colour colorForIndex(size_t index) noexcept
        {
            return juce::Colour(palette[index % paletteSize]);
        }

        /** Returns the translucent fade-ramp colour for a track mask bar. */
        static juce::Colour fadeTint(juce::Colour c) noexcept
        {
            return c.withAlpha(0.30f);
        }

        /** Returns the canonical #RRGGBB hex string for a cyclic track index. */
        static std::string hexForIndex(size_t index)
        {
            return hexPalette[index % paletteSize];
        }

        /** Returns the human-readable palette label for a cyclic track index. */
        static const char* labelForIndex(size_t index) noexcept
        {
            return labelPalette[index % paletteSize];
        }

        /** Parses a #RRGGBB hex string or falls back deterministically to the palette index. */
        static juce::Colour parseHexColor(const std::string& hex, size_t fallbackIndex)
        {
            std::string clean = hex;
            if (!clean.empty() && clean[0] == '#') {
                clean.erase(clean.begin());
            }
            if (clean.size() == 6) {
                bool allHex = true;
                for (unsigned char c : clean) {
                    if (!std::isxdigit(c)) {
                        allHex = false;
                        break;
                    }
                }
                if (allHex) {
                    const auto rgb = static_cast<std::uint32_t>(std::stoul(clean, nullptr, 16));
                    return juce::Colour(0xFF000000u | rgb);
                }
            }
            return colorForIndex(fallbackIndex);
        }
    };

    struct Spacing {
        static constexpr int unit = 4;
        static constexpr int small = 2 * unit;
        static constexpr int medium = 3 * unit;
        static constexpr int large = 4 * unit;
        static constexpr int extraLarge = 6 * unit;
    };

    struct Typography {
        static constexpr float micro = 9.0f;
        static constexpr float caption = 10.0f;
        static constexpr float compact = 11.0f;
        static constexpr float control = 12.0f;
        static constexpr float paramTitle = 13.0f;
        static constexpr float body = 14.0f;
        static constexpr float heading = 16.0f;
    };

    struct Edges {
        static constexpr float borderStroke = 1.0f;
        static constexpr float selectedBorderStroke = 1.5f;
        static constexpr float iconStroke = 1.5f;
        static constexpr float cornerRadius = 3.0f;
        static constexpr float cardCornerRadius = 4.0f;
        static constexpr float popupCornerRadius = 4.0f;
        static constexpr float switcherCornerRadius = 5.0f;
        static constexpr float pillCornerRadius = 10.0f;
        static constexpr int buttonTextInsetX = Spacing::unit;
        static constexpr int buttonTextInsetY = 1;
    };

    struct States {
        static constexpr float selectedFillOpacity = 0.25f;
        static constexpr float menuHighlightOpacity = 0.22f;
        static constexpr float pressedDarkening = 0.2f;
        static constexpr float hoverBrightening = 0.15f;
        static constexpr float scrollbarHoverBrightening = 0.25f;
        static constexpr float curveRibbonTopOpacity = 0.24f;
        static constexpr float curveRibbonBottomOpacity = 0.02f;
        static constexpr float curveGridOpacity = 0.18f;
        static constexpr float nodeHoverHaloOpacity = 0.30f;
        static constexpr float selectedTrackTintOpacity = 0.14f;
        static constexpr float selectedClipBorderThickness = 2.5f;
        static constexpr float ghostClipOpacity = 0.45f;
    };

    struct Sliders {
        static constexpr float fineDragScale = 0.1f;
        static constexpr float trackThickness = 4.0f;
        static constexpr float thumbDiameter = 12.0f;
        static constexpr float largeTrackThickness = 6.0f;
        static constexpr float largeThumbDiameter = 14.0f;
        static constexpr float faderWidth = 20.0f;
        static constexpr float faderHeight = 10.0f;
        static constexpr float faderLineInset = 3.0f;
        static constexpr float rotaryInset = 4.0f;
        static constexpr float arcStroke = 3.5f;
        static constexpr float dialGap = 5.0f;
        static constexpr float needleStroke = 2.0f;
        static constexpr float needleFraction = 0.8f;
    };

    struct Scrollbars {
        static constexpr int thumbInset = 2;
        static constexpr float thumbRadius = 3.0f;
    };

    struct Icons {
        static constexpr float comboRightInset = 14.0f;
        static constexpr float menuRightInset = 12.0f;
        static constexpr float chevronSize = 8.0f;
        static constexpr float triangleWidth = 7.0f;
        static constexpr float triangleBack = 2.0f;
        static constexpr float triangleTip = 3.0f;
    };

    struct Menus {
        static constexpr int textInset = 10;
        static constexpr int tickWidth = Spacing::large;
    };

    struct Animation {
        static constexpr int meterRefreshHz = 30;
        static constexpr float meterFloorDbfs = BallisticMeter::kFloorDb;
        static constexpr float meterDecayDbPerSecond = BallisticMeter::kDecayRateDbPerSec;
        static constexpr float meterRmsIntegrationSeconds = BallisticMeter::kRmsIntegrationSec;
        static constexpr float meterPeakHoldSeconds = BallisticMeter::kPeakHoldDurationSec;
    };
};

} // namespace oae::studio
