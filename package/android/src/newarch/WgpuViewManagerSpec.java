package com.webgpu;

import android.view.View;

import androidx.annotation.Nullable;

import com.facebook.react.uimanager.SimpleViewManager;
import com.facebook.react.uimanager.ViewManagerDelegate;
import com.facebook.react.viewmanagers.WgpuViewManagerDelegate;
import com.facebook.react.viewmanagers.WgpuViewManagerInterface;

public abstract class WgpuViewManagerSpec<T extends View> extends SimpleViewManager<T> implements WgpuViewManagerInterface<T> {
  private final ViewManagerDelegate<T> mDelegate;

  public WgpuViewManagerSpec() {
    mDelegate = new WgpuViewManagerDelegate(this);
  }

  @Nullable
  @Override
  protected ViewManagerDelegate<T> getDelegate() {
    return mDelegate;
  }
}
