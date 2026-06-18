package com.webgpu;

import android.graphics.RenderEffect;
import android.graphics.RuntimeShader;
import android.os.Build;
import android.view.View;

import androidx.annotation.OptIn;
import androidx.annotation.RequiresApi;

import com.facebook.react.bridge.Promise;
import com.facebook.react.bridge.ReactApplicationContext;
import com.facebook.react.bridge.ReactContextBaseJavaModule;
import com.facebook.react.bridge.ReactMethod;
import com.facebook.react.bridge.UIManager;
import com.facebook.react.bridge.UiThreadUtil;
import com.facebook.react.common.annotations.FrameworkAPI;
import com.facebook.react.fabric.FabricUIManager;
import com.facebook.react.uimanager.UIManagerHelper;
import com.facebook.react.uimanager.common.UIManagerType;

// Path B proof-of-concept: attach a GPU compositor shader to a live React Native
// view via View.setRenderEffect (API 31+) + a RuntimeShader (AGSL, API 33+). The
// effect is re-applied by RenderThread on every redraw (e.g. while scrolling)
// with no readback and no per-frame work from JS, unlike the capture path
// (copyElementImageToTexture). This is a legacy (non-codegen) module so it can
// live alongside the existing WebGPUModule TurboModule without touching the spec.
public class RenderEffectModule extends ReactContextBaseJavaModule {
  public static final String NAME = "RenderEffectModule";

  // AGSL: a bold, obvious effect: a constant horizontal RGB split (chromatic
  // aberration visible everywhere, growing toward the edges) plus a strong
  // vignette. `content` is the input view layer; coordinates passed to main()
  // and eval() are in pixels (the view's pixel size, passed via iResolution).
  private static final String AGSL_VIGNETTE =
    "uniform shader content;\n" +
    "uniform float2 iResolution;\n" +
    "half4 main(float2 fragCoord) {\n" +
    "  float2 uv = fragCoord / iResolution;\n" +
    "  float2 centered = uv - 0.5;\n" +
    "  float dist = length(centered);\n" +
    "  float split = (0.012 + dist * 0.04) * iResolution.x;\n" +
    "  half4 c;\n" +
    "  c.r = content.eval(fragCoord + float2(split, 0.0)).r;\n" +
    "  c.g = content.eval(fragCoord).g;\n" +
    "  c.b = content.eval(fragCoord - float2(split, 0.0)).b;\n" +
    "  c.a = 1.0;\n" +
    "  float vignette = 1.0 - smoothstep(0.25, 0.95, dist);\n" +
    "  vignette *= vignette;\n" +
    "  c.rgb *= vignette;\n" +
    "  return c;\n" +
    "}\n";

  public RenderEffectModule(ReactApplicationContext reactContext) {
    super(reactContext);
  }

  @Override
  public String getName() {
    return NAME;
  }

  // effect is accepted for forward-compatibility; the PoC ships one shader.
  @ReactMethod
  public void applyRenderEffect(double tag, String effect, Promise promise) {
    final int reactTag = (int) tag;
    UiThreadUtil.runOnUiThread(() -> {
      try {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) {
          promise.reject(
            "E_UNSUPPORTED",
            "RuntimeShader RenderEffect requires Android 13 (API 33) or newer");
          return;
        }
        View view = resolveView(reactTag);
        if (view == null) {
          promise.reject("E_NO_VIEW", "No view found for tag " + reactTag);
          return;
        }
        applyEffect(view);
        promise.resolve(null);
      } catch (Throwable t) {
        promise.reject("E_RENDER_EFFECT", t);
      }
    });
  }

  @ReactMethod
  public void clearRenderEffect(double tag, Promise promise) {
    final int reactTag = (int) tag;
    UiThreadUtil.runOnUiThread(() -> {
      try {
        View view = resolveView(reactTag);
        if (view != null && Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
          view.setRenderEffect(null);
        }
        promise.resolve(null);
      } catch (Throwable t) {
        promise.reject("E_RENDER_EFFECT", t);
      }
    });
  }

  @RequiresApi(api = Build.VERSION_CODES.TIRAMISU)
  private void applyEffect(View view) {
    int width = Math.max(1, view.getWidth());
    int height = Math.max(1, view.getHeight());
    RuntimeShader shader = new RuntimeShader(AGSL_VIGNETTE);
    shader.setFloatUniform("iResolution", (float) width, (float) height);
    RenderEffect effect = RenderEffect.createRuntimeShaderEffect(shader, "content");
    view.setRenderEffect(effect);
  }

  @OptIn(markerClass = FrameworkAPI.class)
  private View resolveView(int reactTag) {
    UIManager uiManager =
      UIManagerHelper.getUIManager(getReactApplicationContext(), UIManagerType.FABRIC);
    if (!(uiManager instanceof FabricUIManager)) {
      return null;
    }
    return ((FabricUIManager) uiManager).resolveView(reactTag);
  }
}
