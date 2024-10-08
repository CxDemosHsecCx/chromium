// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.test.util.modaldialog;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.mockito.Mockito;

import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/** A fake ModalDialogManager for use in tests involving modals. */
@JNINamespace("ui")
public class FakeModalDialogManager extends ModalDialogManager {
    private PropertyModel mShownDialogModel;

    @CalledByNative
    private static FakeModalDialogManager createForTab() {
        return new FakeModalDialogManager(ModalDialogType.TAB);
    }

    public FakeModalDialogManager(int modalDialogType) {
        super(Mockito.mock(Presenter.class), modalDialogType);
    }

    @Override
    public void showDialog(PropertyModel model, int dialogType) {
        mShownDialogModel = model;
    }

    @Override
    public void dismissDialog(PropertyModel model, int dismissalCause) {
        model.get(ModalDialogProperties.CONTROLLER).onDismiss(model, dismissalCause);
        mShownDialogModel = null;
    }

    @CalledByNative
    public void clickPositiveButton() {
        mShownDialogModel
                .get(ModalDialogProperties.CONTROLLER)
                .onClick(mShownDialogModel, ModalDialogProperties.ButtonType.POSITIVE);
    }

    public void clickNegativeButton() {
        mShownDialogModel
                .get(ModalDialogProperties.CONTROLLER)
                .onClick(mShownDialogModel, ModalDialogProperties.ButtonType.NEGATIVE);
    }

    public PropertyModel getShownDialogModel() {
        return mShownDialogModel;
    }
}
