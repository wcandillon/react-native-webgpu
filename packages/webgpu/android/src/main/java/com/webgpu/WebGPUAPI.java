package com.webgpu;

import android.view.Surface;

import com.facebook.proguard.annotations.DoNotStrip;

public interface WebGPUAPI {

  void surfaceCreated(
    Surface surface
  );

  void surfaceChanged(
    Surface surface
  );

  void surfaceDestroyed();

  void surfaceOffscreen();
}
