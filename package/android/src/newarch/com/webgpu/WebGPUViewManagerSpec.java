package com.webgpu;

import android.view.View;

import androidx.annotation.Nullable;

import com.facebook.react.uimanager.SimpleViewManager;
import com.facebook.react.uimanager.ViewManagerDelegate;
import com.facebook.react.viewmanagers.WebGPUViewManagerDelegate;
import com.facebook.react.viewmanagers.WebGPUViewManagerInterface;

public abstract class WebGPUViewManagerSpec<T extends View> extends SimpleViewManager<T> implements WebGPUViewManagerInterface<T> {
  private final ViewManagerDelegate<T> mDelegate;

  public WebGPUViewManagerSpec() {
    mDelegate = new WebGPUViewManagerDelegate(this);
  }

  @Nullable
  @Override
  protected ViewManagerDelegate<T> getDelegate() {
    return mDelegate;
  }
}
