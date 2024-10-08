// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertNotNull;

import android.app.Activity;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.ui.base.TestActivity;

/** Tests for {@link TabGroupRowCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.DATA_SHARING)
public class TabGroupRowCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private Activity mActivity;
    @Mock private FaviconResolver mFaviconResolver;

    private SavedTabGroup mSavedTabGroup;
    private TabGroupRowCoordinator mCoordinator;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity((activity -> mActivity = activity));
        mSavedTabGroup = new SavedTabGroup();
        mCoordinator = new TabGroupRowCoordinator(mActivity, mSavedTabGroup, mFaviconResolver);
    }

    @Test
    @SmallTest
    public void testGetView() {
        assertNotNull(mCoordinator.getView());
    }
}
