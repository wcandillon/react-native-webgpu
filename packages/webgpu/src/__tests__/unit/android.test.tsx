import React from "react";
import { Platform } from "react-native";
import { render, screen } from "@testing-library/react-native";

import { Canvas } from "../../Canvas";
import type { AndroidCanvasProps } from "../../Canvas";

Object.assign(globalThis, { RNWebGPU: { destroyContext: jest.fn() } });

let warn: jest.SpyInstance;

const runOn = (os: typeof Platform.OS, version: number) => {
  jest.replaceProperty(Platform, "OS", os);
  jest.spyOn(Platform, "Version", "get").mockReturnValue(version);
};

beforeEach(() => {
  runOn("android", 35);
  warn = jest.spyOn(console, "warn").mockImplementation(() => {});
});

afterEach(() => {
  jest.restoreAllMocks();
});

// What a JavaScript caller can pass that TypeScript refuses.
const untyped = (android: object) => android as AndroidCanvasProps;

// The props Canvas hands the native view for its `android` prop.
const nativeProps = async (android?: AndroidCanvasProps) => {
  const { root } = await render(<Canvas android={android} />);
  const views = root?.queryAll((node) => node.type === "WebGPUView") ?? [];
  expect(views).toHaveLength(1);
  const {
    androidBackingView,
    androidOpaque,
    androidZOrderOnTop,
    androidZOrderMediaOverlay,
  } = views[0].props;
  return {
    androidBackingView,
    androidOpaque,
    androidZOrderOnTop,
    androidZOrderMediaOverlay,
  };
};

describe("AndroidCanvasProps", () => {
  // Without `backingView` the union cannot tell the views apart, so an
  // option of another view on the default TextureView must not compile.
  it("takes each option on the views that honour it only", () => {
    const textureViewTakesZOrder: {
      opaque: false;
      zOrderOnTop: true;
    } extends AndroidCanvasProps
      ? true
      : false = false;
    const hardwareBufferViewTakesOpaque: {
      backingView: "HardwareBufferView";
      opaque: true;
    } extends AndroidCanvasProps
      ? true
      : false = false;
    const surfaceViewTakesZOrder: {
      backingView: "SurfaceView";
      opaque: false;
      zOrderOnTop: true;
    } extends AndroidCanvasProps
      ? true
      : false = true;
    expect([
      textureViewTakesZOrder,
      hardwareBufferViewTakesOpaque,
      surfaceViewTakesZOrder,
    ]).toEqual([false, false, true]);
  });
});

describe("Canvas's native props", () => {
  it("back a canvas with a translucent TextureView by default", async () => {
    expect(await nativeProps()).toEqual({
      androidBackingView: "TextureView",
      androidOpaque: false,
      androidZOrderOnTop: false,
      androidZOrderMediaOverlay: false,
    });
  });

  it.each<[AndroidCanvasProps, boolean]>([
    [{ opaque: true }, true],
    [{ backingView: "TextureView" }, false],
    [{ backingView: "SurfaceView" }, true],
    [{ backingView: "SurfaceView", opaque: false }, false],
    [{ backingView: "HardwareBufferView" }, false],
  ])("resolve the opacity of %j to %s", async (android, opaque) => {
    expect((await nativeProps(android)).androidOpaque).toBe(opaque);
  });

  it("keep the z-order options to a SurfaceView", async () => {
    const zOrder = { zOrderOnTop: true, zOrderMediaOverlay: true };
    expect(
      await nativeProps({ backingView: "SurfaceView", ...zOrder }),
    ).toMatchObject({
      androidBackingView: "SurfaceView",
      androidZOrderOnTop: true,
      androidZOrderMediaOverlay: true,
    });
    expect(await nativeProps(untyped(zOrder))).toMatchObject({
      androidBackingView: "TextureView",
      androidZOrderOnTop: false,
      androidZOrderMediaOverlay: false,
    });
  });

  // The generated string-enum parser aborts on a value it does not know.
  it("give a value no view takes the default view", async () => {
    const unknown = await nativeProps(
      untyped({ backingView: "SurfaceControl" }),
    );
    expect(unknown).toEqual(await nativeProps());
  });

  it("are the same on every render of the same props", async () => {
    const android: AndroidCanvasProps = Object.freeze({
      backingView: "SurfaceView",
      opaque: false,
      zOrderMediaOverlay: true,
    });
    const first = await nativeProps(android);
    expect(await nativeProps(android)).toEqual(first);
    expect(android).toEqual({
      backingView: "SurfaceView",
      opaque: false,
      zOrderMediaOverlay: true,
    });
  });
});

describe("Canvas's warnings at mount", () => {
  it("say when the z-order options reach a view that ignores them", async () => {
    await render(<Canvas android={untyped({ zOrderOnTop: true })} />);
    expect(warn).toHaveBeenCalledTimes(1);
    expect(warn.mock.calls[0][0]).toContain("only apply to a SurfaceView");
  });

  it("say when a HardwareBufferView falls back below Android 10", async () => {
    runOn("android", 28);
    await render(<Canvas android={{ backingView: "HardwareBufferView" }} />);
    expect(warn).toHaveBeenCalledTimes(1);
    expect(warn.mock.calls[0][0]).toContain("needs Android 10");
  });

  it("stay quiet about options the view honours", async () => {
    await render(
      <Canvas
        android={{ backingView: "SurfaceView", zOrderMediaOverlay: true }}
      />,
    );
    expect(warn).not.toHaveBeenCalled();
  });

  it("stay quiet on the other platforms", async () => {
    runOn("ios", 18);
    await render(<Canvas android={untyped({ zOrderOnTop: true })} />);
    expect(warn).not.toHaveBeenCalled();
  });
});

describe("Canvas's warnings about a change", () => {
  const changeOnMountedSurfaceView = async (
    android: AndroidCanvasProps,
    mounted: AndroidCanvasProps = { backingView: "SurfaceView" },
  ) => {
    await render(<Canvas android={mounted} />);
    await screen.rerender(<Canvas android={android} />);
  };
  const warnings = () => warn.mock.calls.map(([message]) => message);

  it("say that a change of backingView is ignored", async () => {
    await changeOnMountedSurfaceView({ backingView: "TextureView" });
    expect(warn).toHaveBeenCalledTimes(1);
    expect(warn.mock.calls[0][0]).toContain(
      "Ignoring a change of android.backingView",
    );
  });

  // SurfaceView moves a live surface on a changed zOrderOnTop from Android
  // 11, and on a changed zOrderMediaOverlay only from Android 14: up to
  // Android 13 the media overlay sublayer is read when the surface is created.
  it.each<[number, boolean, boolean]>([
    [29, true, true],
    [30, false, true],
    [33, false, true],
    [34, false, false],
  ])(
    "on API %i, say that zOrderOnTop is ignored: %s, zOrderMediaOverlay: %s",
    async (version, onTopIgnored, mediaOverlayIgnored) => {
      runOn("android", version);
      await changeOnMountedSurfaceView({
        backingView: "SurfaceView",
        zOrderOnTop: true,
      });
      expect(warnings()).toEqual(
        onTopIgnored
          ? [
              expect.stringContaining(
                "Ignoring a change of android.zOrderOnTop",
              ),
            ]
          : [],
      );
      warn.mockClear();
      await changeOnMountedSurfaceView({
        backingView: "SurfaceView",
        zOrderMediaOverlay: true,
      });
      expect(warnings()).toEqual(
        mediaOverlayIgnored
          ? [
              expect.stringContaining(
                "Ignoring a change of android.zOrderMediaOverlay",
              ),
            ]
          : [],
      );
    },
  );

  // Android 13 moves a surface leaving the top to the default sublayer,
  // whatever sublayer was asked for: a media overlay would come back among
  // the other SurfaceViews, so there it reads zOrderOnTop when it mounts.
  it.each<[number, boolean]>([
    [30, false],
    [32, false],
    [33, true],
    [34, false],
  ])(
    "on API %i, say that zOrderOnTop on a media overlay is ignored: %s",
    async (version, ignored) => {
      runOn("android", version);
      const mediaOverlay = {
        backingView: "SurfaceView",
        zOrderMediaOverlay: true,
      } as const;
      await changeOnMountedSurfaceView(
        { ...mediaOverlay, zOrderOnTop: true },
        mediaOverlay,
      );
      expect(warnings()).toEqual(
        ignored
          ? [
              expect.stringContaining(
                "Ignoring a change of android.zOrderOnTop",
              ),
            ]
          : [],
      );
    },
  );
});
