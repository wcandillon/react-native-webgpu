package com.webgpu;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.facebook.react.module.annotations.ReactModule;
import com.facebook.react.uimanager.ThemedReactContext;
import com.facebook.react.uimanager.annotations.ReactProp;

@ReactModule(name = WebGPUViewManager.NAME)
public class WebGPUViewManager extends WebGPUViewManagerSpec<WebGPUView> {

  public static final String NAME = "WebGPUView";
  private static final double MAX_SAFE_JAVASCRIPT_INTEGER = 9007199254740991.0;

  @NonNull
  @Override
  public String getName() {
    return NAME;
  }

  @NonNull
  @Override
  public WebGPUView createViewInstance(@NonNull ThemedReactContext context) {
    return new WebGPUView(context);
  }

  @Override
  @ReactProp(name = "transparent")
  public void setTransparent(WebGPUView view, boolean value) {
    view.setTransparent(value);
  }

  @Override
  @ReactProp(name = "contextId")
  public void setContextId(WebGPUView view, int value) {
    view.setContextId(value);
  }

  @Override
  @ReactProp(name = "sessionId")
  public void setSessionId(WebGPUView view, double value) {
    if (Double.isNaN(value)
      || Double.isInfinite(value)
      || value < 1
      || value > MAX_SAFE_JAVASCRIPT_INTEGER
      || value != Math.rint(value)) {
      throw new IllegalArgumentException("sessionId must be a positive JavaScript-safe integer");
    }
    view.setSessionId((long) value);
  }

  @Override
  protected void onAfterUpdateTransaction(@NonNull WebGPUView view) {
    super.onAfterUpdateTransaction(view);
    // sessionId and contextId form one native identity. Publish only after all
    // React props have been applied so a recycled view cannot briefly attach
    // a new session to its previous context.
    view.commitProperties();
  }

  @Override
  public void onDropViewInstance(@NonNull WebGPUView view) {
    view.dispose();
    super.onDropViewInstance(view);
  }
}
