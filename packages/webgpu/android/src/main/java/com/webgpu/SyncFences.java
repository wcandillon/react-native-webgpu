package com.webgpu;

import android.hardware.SyncFence;
import android.media.Image;
import android.os.Build;
import android.os.Parcel;
import android.os.ParcelFileDescriptor;
import android.util.Log;

import androidx.annotation.NonNull;
import androidx.annotation.RequiresApi;

import java.io.IOException;

/**
 * Bridges the platform's {@link SyncFence} to the sync file descriptors Dawn imports as
 * {@code shared-fence-sync-fd} fences.
 *
 * <p>A buffer handed out by an {@link android.media.ImageReader} carries an acquire fence: the
 * producer (the video decoder, the camera, a {@link android.graphics.HardwareRenderer}) may still
 * be writing it when the consumer acquires it. The framework waits on that fence for CPU access
 * ({@link Image#getPlanes()}), but not for {@link Image#getHardwareBuffer()}, so a GPU consumer has
 * to wait on it itself. Since API 33 the fence is exposed as {@link Image#getFence()}.
 */
final class SyncFences {
  private static final String TAG = "WebGPUSyncFences";

  private SyncFences() {}

  /**
   * The acquire fence of {@code image} as a sync file descriptor the caller owns and must close,
   * or -1 when the frame has none (the producer was done when it was acquired, or the platform
   * predates {@link Image#getFence()}).
   */
  static int acquireFenceFd(@NonNull Image image) {
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) {
      return -1;
    }
    return acquireFenceFdApi33(image);
  }

  @RequiresApi(Build.VERSION_CODES.TIRAMISU)
  private static int acquireFenceFdApi33(@NonNull Image image) {
    SyncFence fence;
    try {
      fence = image.getFence();
    } catch (IOException | RuntimeException e) {
      Log.w(TAG, "could not read the acquire fence of the frame: " + e.getMessage());
      return -1;
    }
    try {
      return detachFd(fence);
    } finally {
      // getFence() returns a duplicate; the detached descriptor is ours, this one goes.
      fence.close();
    }
  }

  /**
   * The file descriptor of {@code fence}, duplicated and detached, so it stays valid after the
   * fence is closed and must be closed by the caller. -1 for an invalid fence.
   *
   * <p>{@link SyncFence} has no public accessor for its descriptor. Writing the fence to a {@link
   * Parcel} and reading it back yields one: the parcel form is a validity flag followed by the
   * file descriptor.
   */
  @RequiresApi(Build.VERSION_CODES.TIRAMISU)
  static int detachFd(@NonNull SyncFence fence) {
    if (!fence.isValid()) {
      return -1;
    }
    Parcel parcel = Parcel.obtain();
    try {
      fence.writeToParcel(parcel, 0);
      parcel.setDataPosition(0);
      if (!parcel.readBoolean()) {
        return -1;
      }
      ParcelFileDescriptor descriptor = parcel.readFileDescriptor();
      return descriptor == null ? -1 : descriptor.detachFd();
    } catch (RuntimeException e) {
      Log.w(TAG, "could not read the file descriptor of a SyncFence: " + e.getMessage());
      return -1;
    } finally {
      parcel.recycle();
    }
  }
}
