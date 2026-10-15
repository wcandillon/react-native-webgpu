package com.webgpu;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

import android.os.Build;

import org.junit.Test;

public class BackingViewKindTest {
  private static final int[] BEFORE_ANDROID_10 = {
    Build.VERSION_CODES.O,
    Build.VERSION_CODES.O_MR1,
    Build.VERSION_CODES.P,
  };

  private static final int[] BEFORE_ANDROID_12L = {
    Build.VERSION_CODES.O,
    Build.VERSION_CODES.O_MR1,
    Build.VERSION_CODES.P,
    Build.VERSION_CODES.Q,
    Build.VERSION_CODES.R,
    Build.VERSION_CODES.S,
  };

  private static final int[] FROM_ANDROID_12L = {
    Build.VERSION_CODES.S_V2,
    Build.VERSION_CODES.TIRAMISU,
    Build.VERSION_CODES.UPSIDE_DOWN_CAKE,
  };

  @Test
  public void onlyAShownLaidOutSurfaceViewRetiresOnAndroid10To12() {
    for (int sdkInt = Build.VERSION_CODES.N; sdkInt <= Build.VERSION_CODES.UPSIDE_DOWN_CAKE; sdkInt++) {
      boolean retires = sdkInt >= Build.VERSION_CODES.Q && sdkInt <= Build.VERSION_CODES.S;
      assertEquals("API " + sdkInt, retires, BackingViewKind.SURFACE_VIEW.retiresWhenReplaced(sdkInt, true, 100, 50));
      assertEquals(false, BackingViewKind.SURFACE_VIEW.retiresWhenReplaced(sdkInt, false, 100, 50));
      assertEquals(false, BackingViewKind.SURFACE_VIEW.retiresWhenReplaced(sdkInt, true, 0, 50));
      assertEquals(false, BackingViewKind.TEXTURE_VIEW.retiresWhenReplaced(sdkInt, true, 100, 50));
      assertEquals(false, BackingViewKind.HARDWARE_BUFFER_VIEW.retiresWhenReplaced(sdkInt, true, 100, 50));
    }
  }

  @Test
  public void onlyAShownLaidOutTextureViewGetsItsSurfaceBeforeItsFirstDraw() {
    assertEquals(true, BackingViewKind.TEXTURE_VIEW.receivesSurfaceBeforeFirstDraw(true, 100, 50));
    assertEquals(false, BackingViewKind.TEXTURE_VIEW.receivesSurfaceBeforeFirstDraw(false, 100, 50));
    assertEquals(false, BackingViewKind.TEXTURE_VIEW.receivesSurfaceBeforeFirstDraw(true, 0, 50));
    assertEquals(false, BackingViewKind.TEXTURE_VIEW.receivesSurfaceBeforeFirstDraw(true, 100, 0));
    assertEquals(false, BackingViewKind.SURFACE_VIEW.receivesSurfaceBeforeFirstDraw(true, 100, 50));
    assertEquals(false, BackingViewKind.HARDWARE_BUFFER_VIEW.receivesSurfaceBeforeFirstDraw(true, 100, 50));
  }

  @Test
  public void onlyASurfaceViewReplacingAShownTextureViewKeepsItOnScreen() {
    for (BackingViewKind incoming : BackingViewKind.values()) {
      for (BackingViewKind outgoing : BackingViewKind.values()) {
        boolean keeps = incoming == BackingViewKind.SURFACE_VIEW && outgoing == BackingViewKind.TEXTURE_VIEW;
        assertEquals(incoming + " over " + outgoing, keeps, incoming.keepsOutgoingOnScreen(outgoing, true, 100, 50));
        assertEquals(false, incoming.keepsOutgoingOnScreen(outgoing, false, 100, 50));
        assertEquals(false, incoming.keepsOutgoingOnScreen(outgoing, true, 0, 0));
      }
    }
  }

  @Test
  public void translucentCanvasIsATextureView() {
    for (int sdkInt : BEFORE_ANDROID_12L) {
      assertEquals(BackingViewKind.TEXTURE_VIEW, BackingViewKind.forAutoSurfaceType(false, false, sdkInt));
    }
    for (int sdkInt : FROM_ANDROID_12L) {
      assertEquals(BackingViewKind.TEXTURE_VIEW, BackingViewKind.forAutoSurfaceType(false, true, sdkInt));
    }
  }

  @Test
  public void opaqueCanvasIsATextureViewBeforeAndroid12L() {
    for (int sdkInt : BEFORE_ANDROID_12L) {
      assertEquals(BackingViewKind.TEXTURE_VIEW, BackingViewKind.forAutoSurfaceType(true, false, sdkInt));
    }
  }

  @Test
  public void opaqueCanvasIsASurfaceViewFromAndroid12L() {
    for (int sdkInt : FROM_ANDROID_12L) {
      assertEquals(BackingViewKind.SURFACE_VIEW, BackingViewKind.forAutoSurfaceType(true, false, sdkInt));
    }
  }

  @Test
  public void opaqueCanvasAskingForZOrderOnTopIsASurfaceViewBeforeAndroid12L() {
    for (int sdkInt : BEFORE_ANDROID_12L) {
      assertEquals(BackingViewKind.SURFACE_VIEW, BackingViewKind.forAutoSurfaceType(true, true, sdkInt));
    }
  }

  @Test
  public void namedSurfaceTypeIsHonouredOnEveryRelease() {
    for (int[] releases : new int[][] {BEFORE_ANDROID_12L, FROM_ANDROID_12L}) {
      for (int sdkInt : releases) {
        assertEquals(BackingViewKind.SURFACE_VIEW, BackingViewKind.fromSurfaceType("SurfaceView", sdkInt));
        assertEquals(BackingViewKind.TEXTURE_VIEW, BackingViewKind.fromSurfaceType("TextureView", sdkInt));
      }
    }
  }

  @Test
  public void hardwareBufferViewNeedsAndroid10() {
    for (int sdkInt : BEFORE_ANDROID_10) {
      assertEquals(BackingViewKind.TEXTURE_VIEW, BackingViewKind.fromSurfaceType("HardwareBufferView", sdkInt));
    }
    for (int sdkInt : new int[] {Build.VERSION_CODES.Q, Build.VERSION_CODES.S_V2, Build.VERSION_CODES.UPSIDE_DOWN_CAKE}) {
      assertEquals(BackingViewKind.HARDWARE_BUFFER_VIEW, BackingViewKind.fromSurfaceType("HardwareBufferView", sdkInt));
    }
  }

  @Test
  public void autoLeavesTheChoiceToTheCanvas() {
    assertNull(BackingViewKind.fromSurfaceType("auto", Build.VERSION_CODES.Q));
    assertNull(BackingViewKind.fromSurfaceType(null, Build.VERSION_CODES.S_V2));
  }

  @Test
  public void onlyATextureViewPresentsBehindThePendingDraw() {
    for (BackingViewKind kind : BackingViewKind.values()) {
      assertEquals(kind == BackingViewKind.TEXTURE_VIEW, kind.presentsBehindPendingDraw());
    }
  }

  @Test
  public void onlyATextureViewOnAndroid11AcquiresAfterTheWindowDraws() {
    for (int[] releases : new int[][] {BEFORE_ANDROID_12L, FROM_ANDROID_12L}) {
      for (int sdkInt : releases) {
        for (BackingViewKind kind : BackingViewKind.values()) {
          boolean acquiresAfterDraw = kind == BackingViewKind.TEXTURE_VIEW && sdkInt == Build.VERSION_CODES.R;
          assertEquals(kind + " on API " + sdkInt, acquiresAfterDraw, kind.acquiresAfterDraw(sdkInt));
        }
      }
    }
  }
}
