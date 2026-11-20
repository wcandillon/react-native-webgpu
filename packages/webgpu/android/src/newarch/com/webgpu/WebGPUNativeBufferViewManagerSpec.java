package com.webgpu;

import android.view.View;

import androidx.annotation.Nullable;

import com.facebook.react.uimanager.SimpleViewManager;
import com.facebook.react.uimanager.ViewManagerDelegate;
import com.facebook.react.viewmanagers.WebGPUNativeBufferViewManagerDelegate;
import com.facebook.react.viewmanagers.WebGPUNativeBufferViewManagerInterface;

public abstract class WebGPUNativeBufferViewManagerSpec<T extends View> extends SimpleViewManager<T> implements WebGPUNativeBufferViewManagerInterface<T> {
  private final ViewManagerDelegate<T> mDelegate;

  public WebGPUNativeBufferViewManagerSpec() {
    mDelegate = new WebGPUNativeBufferViewManagerDelegate(this);
  }

  @Nullable
  @Override
  protected ViewManagerDelegate<T> getDelegate() {
    return mDelegate;
  }
}
