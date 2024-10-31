package com.webgpu;

import android.view.View;

import androidx.annotation.Nullable;

import com.facebook.react.uimanager.SimpleViewManager;

public abstract class WebGPUViewManagerSpec<T extends View> extends SimpleViewManager<T> {
  public abstract void setContextId(T view, int contextId);
  public abstract void setTransparent(T view, boolean transparency);
}
