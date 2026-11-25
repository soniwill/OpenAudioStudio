#include "UiInteractionDriver.h"
#include <algorithm>
#include <cmath>

namespace oae::studio::testing {

namespace {

juce::MouseEvent make_mouse_event(juce::Component& target,
                                  juce::Point<float> localPos,
                                  juce::ModifierKeys mods,
                                  juce::Point<float> downPos,
                                  int numClicks,
                                  bool wasDragged)
{
    auto mouseSource = juce::Desktop::getInstance().getMainMouseSource();
    const auto now = juce::Time::getCurrentTime();
    return juce::MouseEvent(mouseSource,
                            localPos,
                            mods,
                            0.0f,
                            0.0f,
                            0.0f,
                            0.0f,
                            0.0f,
                            &target,
                            &target,
                            now,
                            downPos,
                            now,
                            numClicks,
                            wasDragged);
}

std::optional<TestableTarget> find_custom_target(juce::Component& comp, const std::string& targetId)
{
    const auto* testable = dynamic_cast<const ITestableCustomControl*>(&comp);
    if (testable == nullptr) {
        return std::nullopt;
    }
    for (const auto& t : testable->get_testable_targets()) {
        if (t.id == targetId) {
            return t;
        }
    }
    return std::nullopt;
}

} // namespace

juce::Component* UiInteractionDriver::find_root_for(juce::Component& comp) noexcept
{
    juce::Component* cur = &comp;
    while (cur->getParentComponent() != nullptr) {
        auto* parent = cur->getParentComponent();
        if (auto* dw = dynamic_cast<juce::DocumentWindow*>(parent)) {
            if (dw->getContentComponent() == cur) {
                return cur;
            }
        }
        cur = parent;
    }
    return cur;
}

UiInteractionDriver::ViewportState UiInteractionDriver::scroll_into_view(
    juce::Component& target,
    std::optional<juce::Rectangle<int>> localSubRect)
{
    ViewportState saved;
    const auto targetRect = localSubRect.value_or(target.getLocalBounds());
    for (juce::Component* cur = target.getParentComponent(); cur != nullptr; cur = cur->getParentComponent()) {
        if (auto* vp = dynamic_cast<juce::Viewport*>(cur)) {
            auto* viewed = vp->getViewedComponent();
            if (viewed != nullptr && (viewed == &target || viewed->isParentOf(&target))) {
                saved.emplace_back(vp, vp->getViewPosition());
                const auto rectInViewed = viewed->getLocalArea(&target, targetRect);
                const auto viewArea = vp->getViewArea();
                if (!viewArea.contains(rectInViewed)) {
                    int nx = vp->getViewPositionX();
                    int ny = vp->getViewPositionY();
                    if (viewed->getWidth() > vp->getViewWidth()) {
                        nx = std::clamp(rectInViewed.getCentreX() - vp->getViewWidth() / 2,
                                        0,
                                        std::max(0, viewed->getWidth() - vp->getViewWidth()));
                    }
                    if (viewed->getHeight() > vp->getViewHeight()) {
                        ny = std::clamp(rectInViewed.getCentreY() - vp->getViewHeight() / 2,
                                        0,
                                        std::max(0, viewed->getHeight() - vp->getViewHeight()));
                    }
                    vp->setViewPosition(nx, ny);
                }
            }
        } else if (auto* lb = dynamic_cast<juce::ListBox*>(cur)) {
            if (lb->getSelectedRow() >= 0) {
                lb->scrollToEnsureRowIsOnscreen(lb->getSelectedRow());
            }
        }
    }
    return saved;
}

void UiInteractionDriver::restore_viewports(const ViewportState& saved)
{
    for (auto it = saved.rbegin(); it != saved.rend(); ++it) {
        if (it->first != nullptr) {
            it->first->setViewPosition(it->second);
        }
    }
}

PreflightResult UiInteractionDriver::preflight(juce::Component& target,
                                               std::optional<juce::Point<int>> localPoint,
                                               bool autoScrollViewport,
                                               std::optional<juce::Rectangle<int>> localSubRect)
{
    juce::Component* root = find_root_for(target);
    if (root == nullptr) {
        return { false, "NOT_VISIBLE", "No root component found for target", {}, {} };
    }

    for (juce::Component* cur = &target; cur != nullptr; cur = cur->getParentComponent()) {
        if (!cur->isVisible()) {
            return { false, "NOT_VISIBLE",
                     "Component or ancestor is not visible (" + cur->getComponentID().toStdString() + ")",
                     {}, {} };
        }
        if (cur == root) {
            break;
        }
    }

    if (!target.isEnabled()) {
        return { false, "DISABLED",
                 "Target component is disabled (" + target.getComponentID().toStdString() + ")",
                 {}, {} };
    }

    const auto subRect = localSubRect.value_or(target.getLocalBounds());
    if (target.getWidth() <= 0 || target.getHeight() <= 0 || subRect.getWidth() <= 0 || subRect.getHeight() <= 0) {
        return { false, "ZERO_BOUNDS",
                 "Target component or sub-target has non-positive bounds (" + target.getComponentID().toStdString() + ")",
                 {}, {} };
    }

    if (autoScrollViewport) {
        scroll_into_view(target, subRect);
    }

    juce::Rectangle<int> clippedInRoot = root->getLocalArea(&target, subRect);
    for (juce::Component* anc = target.getParentComponent(); anc != nullptr; anc = anc->getParentComponent()) {
        juce::Rectangle<int> ancBounds = anc->getLocalBounds();
        if (auto* vp = dynamic_cast<juce::Viewport*>(anc)) {
            ancBounds = juce::Rectangle<int>(0, 0, vp->getViewWidth(), vp->getViewHeight());
        }
        clippedInRoot = clippedInRoot.getIntersection(root->getLocalArea(anc, ancBounds));
        if (anc == root) {
            break;
        }
    }

    if (clippedInRoot.isEmpty()) {
        return { false, "CLIPPED",
                 "Target bounds are completely clipped outside visible ancestor bounds (" + target.getComponentID().toStdString() + ")",
                 {}, {} };
    }

    juce::Point<int> ptLocal = localPoint.value_or(subRect.getCentre());
    juce::Point<int> ptInRoot = root->getLocalPoint(&target, ptLocal);
    if (!clippedInRoot.contains(ptInRoot)) {
        if (!localPoint.has_value()) {
            ptInRoot = clippedInRoot.getCentre();
            ptLocal = target.getLocalPoint(root, ptInRoot);
        } else {
            return { false, "CLIPPED",
                     "Interaction point lies outside clipped visible bounds (" + target.getComponentID().toStdString() + ")",
                     ptInRoot, ptLocal };
        }
    }

    juce::Component* hit = root->getComponentAt(ptInRoot);
    if (hit == nullptr) {
        return { false, "OCCLUDED_HITTEST",
                 "hitTest returned nullptr at interaction point (" + target.getComponentID().toStdString() + ")",
                 ptInRoot, ptLocal };
    }

    const bool validHit = (hit == &target) || target.isParentOf(hit);
    if (!validHit) {
        return { false, "OCCLUDED_HITTEST",
                 "Target '" + target.getComponentID().toStdString() + "' occluded by '" + hit->getComponentID().toStdString() + "'",
                 ptInRoot, ptLocal };
    }

    return { true, "OK", {}, ptInRoot, ptLocal };
}

void UiInteractionDriver::drain_messages(int durationMs)
{
    if (auto* mm = juce::MessageManager::getInstanceWithoutCreating()) {
        mm->runDispatchLoopUntil(std::max(1, durationMs));
    }
}

PreflightResult UiInteractionDriver::click(juce::Component& target,
                                           bool rightClick,
                                           std::optional<juce::Point<int>> localPt)
{
    const auto pf = preflight(target, localPt, true);
    if (!pf.ok) {
        return pf;
    }

    const auto pos = pf.localPoint.toFloat();
    const juce::ModifierKeys downMods(rightClick ? juce::ModifierKeys::rightButtonModifier
                                                 : juce::ModifierKeys::leftButtonModifier);
    juce::Component::SafePointer<juce::Component> safe(&target);

    if (safe) safe->mouseEnter(make_mouse_event(*safe, pos, juce::ModifierKeys(), pos, 0, false));
    if (safe) safe->mouseMove(make_mouse_event(*safe, pos, juce::ModifierKeys(), pos, 0, false));
    if (safe) safe->mouseDown(make_mouse_event(*safe, pos, downMods, pos, 1, false));
    if (safe) safe->mouseUp(make_mouse_event(*safe, pos, downMods, pos, 1, false));
    drain_messages(2);
    return pf;
}

PreflightResult UiInteractionDriver::double_click(juce::Component& target,
                                                  std::optional<juce::Point<int>> localPt)
{
    const auto pf = preflight(target, localPt, true);
    if (!pf.ok) {
        return pf;
    }

    const auto pos = pf.localPoint.toFloat();
    const juce::ModifierKeys downMods(juce::ModifierKeys::leftButtonModifier);
    juce::Component::SafePointer<juce::Component> safe(&target);

    if (safe) safe->mouseEnter(make_mouse_event(*safe, pos, juce::ModifierKeys(), pos, 0, false));
    if (safe) safe->mouseMove(make_mouse_event(*safe, pos, juce::ModifierKeys(), pos, 0, false));
    if (safe) safe->mouseDown(make_mouse_event(*safe, pos, downMods, pos, 1, false));
    if (safe) safe->mouseUp(make_mouse_event(*safe, pos, downMods, pos, 1, false));
    if (safe) safe->mouseDown(make_mouse_event(*safe, pos, downMods, pos, 2, false));
    if (safe) safe->mouseDoubleClick(make_mouse_event(*safe, pos, downMods, pos, 2, false));
    if (safe) safe->mouseUp(make_mouse_event(*safe, pos, downMods, pos, 2, false));
    drain_messages(2);
    return pf;
}

PreflightResult UiInteractionDriver::drag(juce::Component& target,
                                          juce::Point<int> localStart,
                                          juce::Point<int> localEnd,
                                          bool shiftModifier,
                                          int steps,
                                          bool ctrlModifier)
{
    const auto pf = preflight(target, localStart, true);
    if (!pf.ok) {
        return pf;
    }

    int flags = juce::ModifierKeys::leftButtonModifier;
    if (shiftModifier) flags |= juce::ModifierKeys::shiftModifier;
    if (ctrlModifier) flags |= juce::ModifierKeys::ctrlModifier;
    const juce::ModifierKeys dragMods(flags);

    const auto startF = localStart.toFloat();
    const auto endF = localEnd.toFloat();
    const int numSteps = std::max(2, steps);
    juce::Component::SafePointer<juce::Component> safe(&target);

    if (safe) safe->mouseEnter(make_mouse_event(*safe, startF, juce::ModifierKeys(), startF, 0, false));
    if (safe) safe->mouseMove(make_mouse_event(*safe, startF, juce::ModifierKeys(), startF, 0, false));
    if (safe) safe->mouseDown(make_mouse_event(*safe, startF, dragMods, startF, 1, false));
    for (int i = 1; i <= numSteps && safe != nullptr; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(numSteps);
        const juce::Point<float> cur = startF + (endF - startF) * t;
        safe->mouseDrag(make_mouse_event(*safe, cur, dragMods, startF, 1, true));
    }
    if (safe) safe->mouseUp(make_mouse_event(*safe, endF, dragMods, startF, 1, true));
    drain_messages(2);
    return pf;
}

PreflightResult UiInteractionDriver::drag_slider(juce::Slider& slider,
                                                 double normalizedDelta,
                                                 bool shiftModifier)
{
    auto sliderArea = slider.getLookAndFeel().getSliderLayout(slider).sliderBounds;
    if (sliderArea.isEmpty()) {
        sliderArea = slider.getLocalBounds();
    }
    const auto startPt = sliderArea.getCentre();
    const auto style = slider.getSliderStyle();
    const bool vertical = (style == juce::Slider::LinearVertical ||
                           style == juce::Slider::LinearBarVertical ||
                           style == juce::Slider::RotaryVerticalDrag);

    const int span = std::max(40, vertical ? sliderArea.getHeight() : sliderArea.getWidth());
    const int pixelDelta = static_cast<int>(std::round(normalizedDelta * static_cast<double>(span)));
    const juce::Point<int> endPt = vertical
        ? juce::Point<int>(startPt.x, startPt.y - pixelDelta)
        : juce::Point<int>(startPt.x + pixelDelta, startPt.y);

    return drag(slider, startPt, endPt, shiftModifier, 8, false);
}

PreflightResult UiInteractionDriver::wheel(juce::Component& target,
                                           float deltaY,
                                           bool shiftModifier,
                                           bool ctrlModifier,
                                           std::optional<juce::Point<int>> localPt)
{
    const auto pf = preflight(target, localPt, true);
    if (!pf.ok) {
        return pf;
    }

    int flags = 0;
    if (shiftModifier) flags |= juce::ModifierKeys::shiftModifier;
    if (ctrlModifier) flags |= juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::commandModifier;
    const auto pos = pf.localPoint.toFloat();
    const auto ev = make_mouse_event(target, pos, juce::ModifierKeys(flags), pos, 0, false);

    juce::MouseWheelDetails details{};
    details.deltaX = 0.0f;
    details.deltaY = deltaY;
    details.isReversed = false;
    details.isSmooth = false;
    details.isInertial = false;

    target.mouseWheelMove(ev, details);
    drain_messages(2);
    return pf;
}

PreflightResult UiInteractionDriver::key_press(juce::Component& target, const juce::KeyPress& key)
{
    const auto pf = preflight(target, std::nullopt, true);
    if (!pf.ok) {
        return pf;
    }

    target.grabKeyboardFocus();
    target.keyPressed(key);
    drain_messages(2);
    return pf;
}

PreflightResult UiInteractionDriver::select_combo_item(juce::ComboBox& combo, int itemId)
{
    const auto pf = preflight(combo, std::nullopt, true);
    if (!pf.ok) {
        return pf;
    }
    if (combo.getNumItems() <= 0) {
        return { false, "DISABLED", "ComboBox has zero items (" + combo.getComponentID().toStdString() + ")", pf.rootPoint, pf.localPoint };
    }
    if (!combo.isItemEnabled(itemId)) {
        return { false, "DISABLED", "Requested ComboBox itemId is disabled (" + std::to_string(itemId) + ")", pf.rootPoint, pf.localPoint };
    }

    juce::Component::SafePointer<juce::ComboBox> safe(&combo);
    const auto pos = pf.localPoint.toFloat();
    if (safe) safe->mouseEnter(make_mouse_event(*safe, pos, juce::ModifierKeys(), pos, 0, false));
    if (safe) safe->mouseMove(make_mouse_event(*safe, pos, juce::ModifierKeys(), pos, 0, false));
    if (safe) safe->setSelectedId(itemId, juce::sendNotificationSync);
    juce::PopupMenu::dismissAllActiveMenus();
    drain_messages(2);
    return pf;
}

PreflightResult UiInteractionDriver::select_combo_index(juce::ComboBox& combo, int index)
{
    if (index < 0 || index >= combo.getNumItems()) {
        return { false, "DISABLED", "ComboBox item index out of range", {}, {} };
    }
    return select_combo_item(combo, combo.getItemId(index));
}

PreflightResult UiInteractionDriver::type_text(juce::TextEditor& editor,
                                               const std::string& text,
                                               bool pressReturn,
                                               bool unfocus)
{
    const auto pf = click(editor, false, std::nullopt);
    if (!pf.ok) {
        return pf;
    }
    if (editor.isReadOnly()) {
        return { false, "DISABLED", "TextEditor is read-only (" + editor.getComponentID().toStdString() + ")", pf.rootPoint, pf.localPoint };
    }

    juce::Component::SafePointer<juce::TextEditor> safe(&editor);
    if (safe) safe->grabKeyboardFocus();
    if (safe) safe->selectAll();
    if (safe) safe->keyPressed(juce::KeyPress(juce::KeyPress::backspaceKey));
    for (unsigned char ch : text) {
        if (!safe) break;
        const auto wchar = static_cast<juce::juce_wchar>(ch);
        safe->keyPressed(juce::KeyPress(static_cast<int>(wchar), juce::ModifierKeys(), wchar));
    }
    if (pressReturn && safe) {
        safe->keyPressed(juce::KeyPress(juce::KeyPress::returnKey));
    }
    if (unfocus && safe) {
        if (auto* root = find_root_for(*safe)) {
            root->grabKeyboardFocus();
        }
        if (safe) {
            safe->focusLost(juce::Component::focusChangedDirectly);
        }
    }
    drain_messages(2);
    return pf;
}

PreflightResult UiInteractionDriver::click_target(juce::Component& customComp,
                                                  const std::string& targetId,
                                                  bool rightClick)
{
    const auto target = find_custom_target(customComp, targetId);
    if (!target.has_value()) {
        return { false, "MISSING_TARGET", "Custom target '" + targetId + "' not found on '" + customComp.getComponentID().toStdString() + "'", {}, {} };
    }
    if (!target->enabled || target->localBounds.isEmpty()) {
        return { false, "DISABLED", "Custom target '" + targetId + "' is disabled or empty", {}, {} };
    }
    const auto pf = preflight(customComp, target->localBounds.getCentre(), true, target->localBounds);
    if (!pf.ok) {
        return pf;
    }
    return click(customComp, rightClick, pf.localPoint);
}

PreflightResult UiInteractionDriver::double_click_target(juce::Component& customComp,
                                                         const std::string& targetId)
{
    const auto target = find_custom_target(customComp, targetId);
    if (!target.has_value()) {
        return { false, "MISSING_TARGET", "Custom target '" + targetId + "' not found on '" + customComp.getComponentID().toStdString() + "'", {}, {} };
    }
    if (!target->enabled || target->localBounds.isEmpty()) {
        return { false, "DISABLED", "Custom target '" + targetId + "' is disabled or empty", {}, {} };
    }
    const auto pf = preflight(customComp, target->localBounds.getCentre(), true, target->localBounds);
    if (!pf.ok) {
        return pf;
    }
    return double_click(customComp, pf.localPoint);
}

PreflightResult UiInteractionDriver::drag_target(juce::Component& customComp,
                                                 const std::string& targetId,
                                                 juce::Point<int> deltaOffset,
                                                 bool shiftModifier,
                                                 int steps,
                                                 bool ctrlModifier)
{
    const auto target = find_custom_target(customComp, targetId);
    if (!target.has_value()) {
        return { false, "MISSING_TARGET", "Custom target '" + targetId + "' not found on '" + customComp.getComponentID().toStdString() + "'", {}, {} };
    }
    if (!target->enabled || target->localBounds.isEmpty()) {
        return { false, "DISABLED", "Custom target '" + targetId + "' is disabled or empty", {}, {} };
    }
    const auto pf = preflight(customComp, target->localBounds.getCentre(), true, target->localBounds);
    if (!pf.ok) {
        return pf;
    }
    return drag(customComp, pf.localPoint, pf.localPoint + deltaOffset, shiftModifier, steps, ctrlModifier);
}

PreflightResult UiInteractionDriver::wheel_target(juce::Component& customComp,
                                                  const std::string& targetId,
                                                  float deltaY,
                                                  bool shiftModifier,
                                                  bool ctrlModifier)
{
    const auto target = find_custom_target(customComp, targetId);
    if (!target.has_value()) {
        return { false, "MISSING_TARGET", "Custom target '" + targetId + "' not found on '" + customComp.getComponentID().toStdString() + "'", {}, {} };
    }
    if (!target->enabled || target->localBounds.isEmpty()) {
        return { false, "DISABLED", "Custom target '" + targetId + "' is disabled or empty", {}, {} };
    }
    const auto pf = preflight(customComp, target->localBounds.getCentre(), true, target->localBounds);
    if (!pf.ok) {
        return pf;
    }
    return wheel(customComp, deltaY, shiftModifier, ctrlModifier, pf.localPoint);
}

bool UiInteractionDriver::run_preflight_negative_self_test(juce::Component& host, std::string& outError)
{
    struct TempSandbox {
        juce::Component& host;
        juce::TextButton hiddenBtn{ "Hidden" };
        juce::TextButton disabledBtn{ "Disabled" };
        juce::TextButton zeroBtn{ "Zero" };
        juce::TextButton occludedBtn{ "Occluded" };
        juce::Component blocker;
        bool clickedAny{ false };

        explicit TempSandbox(juce::Component& h) : host(h)
        {
            hiddenBtn.setComponentID("selftest.hidden_btn");
            disabledBtn.setComponentID("selftest.disabled_btn");
            zeroBtn.setComponentID("selftest.zero_btn");
            occludedBtn.setComponentID("selftest.occluded_btn");
            blocker.setComponentID("selftest.opaque_blocker");

            auto markClicked = [this]() { clickedAny = true; };
            hiddenBtn.onClick = markClicked;
            disabledBtn.onClick = markClicked;
            zeroBtn.onClick = markClicked;
            occludedBtn.onClick = markClicked;

            host.addChildComponent(hiddenBtn);
            hiddenBtn.setBounds(20, 20, 80, 24);

            host.addAndMakeVisible(disabledBtn);
            disabledBtn.setBounds(20, 50, 80, 24);
            disabledBtn.setEnabled(false);

            host.addAndMakeVisible(zeroBtn);
            zeroBtn.setBounds(20, 80, 0, 24);

            host.addAndMakeVisible(occludedBtn);
            occludedBtn.setBounds(20, 110, 80, 24);

            host.addAndMakeVisible(blocker);
            blocker.setBounds(15, 105, 90, 34);
            blocker.setInterceptsMouseClicks(true, true);
            blocker.toFront(false);
        }

        ~TempSandbox()
        {
            host.removeChildComponent(&blocker);
            host.removeChildComponent(&occludedBtn);
            host.removeChildComponent(&zeroBtn);
            host.removeChildComponent(&disabledBtn);
            host.removeChildComponent(&hiddenBtn);
        }
    } box(host);

    const auto rHidden = click(box.hiddenBtn);
    if (rHidden.ok || rHidden.code != "NOT_VISIBLE") {
        outError = "Negative self-test failed: invisible button did not fail with NOT_VISIBLE";
        return false;
    }

    const auto rDisabled = click(box.disabledBtn);
    if (rDisabled.ok || rDisabled.code != "DISABLED") {
        outError = "Negative self-test failed: disabled button did not fail with DISABLED";
        return false;
    }

    const auto rZero = click(box.zeroBtn);
    if (rZero.ok || rZero.code != "ZERO_BOUNDS") {
        outError = "Negative self-test failed: zero-bounds button did not fail with ZERO_BOUNDS";
        return false;
    }

    const auto rOccluded = click(box.occludedBtn);
    if (rOccluded.ok || rOccluded.code != "OCCLUDED_HITTEST") {
        outError = "Negative self-test failed: occluded button did not fail with OCCLUDED_HITTEST (got " + rOccluded.code + ")";
        return false;
    }

    if (box.clickedAny) {
        outError = "Negative self-test failed: an onClick callback was invoked despite pre-flight rejection";
        return false;
    }

    return true;
}

} // namespace oae::studio::testing
