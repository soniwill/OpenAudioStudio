#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <deque>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace oae::studio {

/** Local interactive hit target exposed by a custom-painted JUCE widget for UI testing. */
struct TestableTarget {
    std::string id;
    std::string role;
    juce::Rectangle<int> localBounds;
    bool enabled{ true };
};

/** Read-only testing interface exposing custom component hit zones in local coordinates. */
class ITestableCustomControl {
public:
    virtual ~ITestableCustomControl() = default;

    /** Returns local-space clickable or draggable target regions currently active on this widget. */
    virtual std::vector<TestableTarget> get_testable_targets() const = 0;
};

/** Recorded invocation of an intercepted native dialog, popup menu, or external process launch. */
struct DialogInvocation {
    std::string kind;
    std::string tag;
    std::string title;
    std::vector<std::string> options;
};

/** Deterministic non-blocking test seam intercepting modal dialogs, popup menus, and external launches. */
class UiTestDialogSeam {
public:
    /** RAII guard enabling the test seam for its lifetime and clearing queues on exit. */
    class ScopedActivation {
    public:
        ScopedActivation()
        {
            auto& s = state();
            s.active = true;
            s.fileQueue.clear();
            s.alertQueue.clear();
            s.popupQueue.clear();
            s.invocations.clear();
        }

        ~ScopedActivation()
        {
            auto& s = state();
            s.active = false;
            s.fileQueue.clear();
            s.alertQueue.clear();
            s.popupQueue.clear();
            s.invocations.clear();
        }

        ScopedActivation(const ScopedActivation&) = delete;
        ScopedActivation& operator=(const ScopedActivation&) = delete;
    };

    /** Returns true when test dialog interception is active. */
    static bool is_active() noexcept
    {
        return state().active;
    }

    /** Queues a path to be returned by the next intercepted FileChooser invocation. */
    static void enqueue_file_choice(std::filesystem::path path)
    {
        state().fileQueue.push_back(std::move(path));
    }

    /** Queues a result code and text input for the next intercepted AlertWindow invocation. */
    static void enqueue_alert_result(int result, std::string text = {})
    {
        state().alertQueue.push_back({ result, std::move(text) });
    }

    /** Queues a menu item ID to be returned by the next intercepted PopupMenu invocation. */
    static void enqueue_popup_result(int itemId)
    {
        state().popupQueue.push_back(itemId);
    }

    /** Clears all queued responses and recorded dialog invocations. */
    static void clear_all()
    {
        auto& s = state();
        s.fileQueue.clear();
        s.alertQueue.clear();
        s.popupQueue.clear();
        s.invocations.clear();
    }

    /** Clears only the recorded dialog invocations log. */
    static void clear_invocations()
    {
        state().invocations.clear();
    }

    /** Returns all dialog/menu invocations captured while active. */
    static const std::vector<DialogInvocation>& invocations() noexcept
    {
        return state().invocations;
    }

    /** Intercepts a FileChooser when active, writing the queued path (or empty path if cancelled). */
    static bool intercept_file_chooser(const std::string& tag, const std::string& title, std::filesystem::path& outPath)
    {
        auto& s = state();
        if (!s.active) return false;
        s.invocations.push_back({ "file_chooser", tag, title, {} });
        if (!s.fileQueue.empty()) {
            outPath = std::move(s.fileQueue.front());
            s.fileQueue.pop_front();
        } else {
            outPath.clear();
        }
        return true;
    }

    /** Intercepts an AlertWindow when active, writing the queued result code and optional input text. */
    static bool intercept_alert(const std::string& tag, const std::string& title, int& outResult, std::string& outText)
    {
        auto& s = state();
        if (!s.active) return false;
        s.invocations.push_back({ "alert", tag, title, {} });
        if (!s.alertQueue.empty()) {
            outResult = s.alertQueue.front().result;
            outText = std::move(s.alertQueue.front().text);
            s.alertQueue.pop_front();
        } else {
            outResult = 0;
            outText.clear();
        }
        return true;
    }

    /** Intercepts a PopupMenu when active, recording available options and returning the queued item ID. */
    static bool intercept_popup_menu(const std::string& tag, const juce::PopupMenu& menu, int& outItemId)
    {
        auto& s = state();
        if (!s.active) return false;
        DialogInvocation inv{ "popup_menu", tag, {}, {} };
        juce::PopupMenu::MenuItemIterator it(menu, true);
        while (it.next()) {
            const auto& item = it.getItem();
            if (!item.isSeparator && item.itemID != 0) {
                inv.options.push_back(std::to_string(item.itemID) + ":" + item.text.toStdString());
            }
        }
        s.invocations.push_back(std::move(inv));
        if (!s.popupQueue.empty()) {
            outItemId = s.popupQueue.front();
            s.popupQueue.pop_front();
        } else {
            outItemId = 0;
        }
        return true;
    }

    /** Intercepts launching an external OS file or URL when active. */
    static bool intercept_external_launch(const std::string& tag, const std::string& target)
    {
        auto& s = state();
        if (!s.active) return false;
        s.invocations.push_back({ "external_launch", tag, target, {} });
        return true;
    }

    /** Executes a synchronous PopupMenu through the seam when active, falling back to menu.show(). */
    static int show_modal_menu(const std::string& tag, juce::PopupMenu& menu)
    {
        int itemId = 0;
        if (intercept_popup_menu(tag, menu, itemId)) {
            return itemId;
        }
        return menu.show();
    }

    /** Executes an async PopupMenu synchronously through the seam when active, or shows it asynchronously. */
    template <typename Callback>
    static void show_popup_menu(const std::string& tag,
                                juce::PopupMenu& menu,
                                const juce::PopupMenu::Options& options,
                                Callback&& callback)
    {
        int itemId = 0;
        if (intercept_popup_menu(tag, menu, itemId)) {
            callback(itemId);
            return;
        }
        menu.showMenuAsync(options, std::forward<Callback>(callback));
    }

private:
    struct AlertResponse {
        int result{ 0 };
        std::string text;
    };

    struct State {
        bool active{ false };
        std::deque<std::filesystem::path> fileQueue;
        std::deque<AlertResponse> alertQueue;
        std::deque<int> popupQueue;
        std::vector<DialogInvocation> invocations;
    };

    static State& state()
    {
        static State instance;
        return instance;
    }
};

} // namespace oae::studio
