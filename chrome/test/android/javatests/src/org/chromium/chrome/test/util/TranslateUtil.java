// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util;

import android.app.Instrumentation;
import android.view.View;

import org.junit.Assert;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.infobar.TranslateCompactInfoBar;
import org.chromium.components.infobars.InfoBar;
import org.chromium.components.infobars.InfoBarCompactLayout;
import org.chromium.components.translate.TranslateMenu;
import org.chromium.components.translate.TranslateTabLayout;

import java.util.ArrayList;
import java.util.concurrent.TimeoutException;

/** Utility functions for dealing with Translate InfoBars. */
public class TranslateUtil {

    public static void assertCompactTranslateInfoBar(InfoBar infoBar) {
        Assert.assertTrue(infoBar.getView() instanceof InfoBarCompactLayout);

        View content = infoBar.getView().findViewById(R.id.translate_infobar_content);
        Assert.assertNotNull(content);

        View tabLayout = content.findViewById(R.id.translate_infobar_tabs);
        Assert.assertTrue(tabLayout instanceof TranslateTabLayout);
    }

    public static void assertHasAtLeastTwoLanguageTabs(TranslateCompactInfoBar infoBar) {
        View content = infoBar.getView().findViewById(R.id.translate_infobar_content);
        Assert.assertNotNull(content);

        TranslateTabLayout tabLayout =
                (TranslateTabLayout) content.findViewById(R.id.translate_infobar_tabs);
        Assert.assertTrue(tabLayout.getTabCount() >= 2);
    }

    /**
     * Checks if the menu button exists on the InfoBar.
     * @return True if the View was found.
     */
    public static boolean hasMenuButton(InfoBar infoBar) {
        return InfoBarUtil.findButton(infoBar, R.id.translate_infobar_menu_button, false);
    }

    /**
     * Simulates clicking the menu button in the specified infobar.
     * @return True if the View was found.
     */
    public static boolean clickMenuButton(InfoBar infoBar) {
        return InfoBarUtil.findButton(infoBar, R.id.translate_infobar_menu_button, true);
    }

    /** Simulates clicking the menu button and check if overflow menu is shown. */
    public static void clickMenuButtonAndAssertMenuShown(final TranslateCompactInfoBar infoBar) {
        clickMenuButton(infoBar);
        CriteriaHelper.pollInstrumentationThread(
                infoBar::isShowingOverflowMenuForTesting, "Overflow menu did not show");
    }

    /** Simulates clicking the 'More Language' menu item and check if language menu is shown. */
    public static void clickMoreLanguageButtonAndAssertLanguageMenuShown(
            Instrumentation instrumentation, final TranslateCompactInfoBar infoBar) {
        invokeOverflowMenuActionSync(
                instrumentation, infoBar, TranslateMenu.ID_OVERFLOW_MORE_LANGUAGE);
        CriteriaHelper.pollInstrumentationThread(
                infoBar::isShowingLanguageMenuForTesting, "Language menu did not show");
    }

    /**
     * Execute a particular menu item from the overflow menu.
     * The item is executed even if it is disabled or not visible.
     */
    public static void invokeOverflowMenuActionSync(
            Instrumentation instrumentation, final TranslateCompactInfoBar infoBar, final int id) {
        instrumentation.runOnMainSync(
                new Runnable() {
                    @Override
                    public void run() {
                        infoBar.onOverflowMenuItemClicked(id);
                    }
                });
    }

    /**
     * Simulate the target menu item with given language |code| being clicked (even if it is not
     * visible).
     */
    public static void clickTargetMenuItem(
            final TranslateCompactInfoBar infoBar, final String code) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    infoBar.onTargetMenuItemClicked(code);
                });
    }

    /**
     * Wait until the translate infobar is present in the given InfoBarContainer and is in the given
     * state. Throws a TimeoutException if the given state is never reached.
     */
    public static void waitForTranslateInfoBarState(
            InfoBarContainer container, boolean expectTranslated) throws TimeoutException {
        CriteriaHelper.pollUiThread(
                () -> {
                    ArrayList<InfoBar> infoBars = container.getInfoBarsForTesting();
                    if (infoBars.isEmpty()) {
                        return false;
                    }
                    assertCompactTranslateInfoBar(infoBars.get(0));
                    TranslateCompactInfoBar infoBar = (TranslateCompactInfoBar) infoBars.get(0);
                    if (expectTranslated) {
                        return !infoBar.isSourceTabSelectedForTesting()
                                && infoBar.isTargetTabSelectedForTesting();
                    } else {
                        return infoBar.isSourceTabSelectedForTesting()
                                && !infoBar.isTargetTabSelectedForTesting();
                    }
                });
    }
}
