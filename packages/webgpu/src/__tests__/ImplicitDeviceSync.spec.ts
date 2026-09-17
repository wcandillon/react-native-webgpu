import { client } from "./setup";

// Dawn's ImplicitDeviceSynchronization is a native-only feature: it is
// requested by default so one device can be shared across runtimes (JS thread
// + worklets). None of this exists on the web backend, so every test skips
// there.
describe("Implicit device synchronization", () => {
  it("is enabled by default on a device requested with no descriptor", async () => {
    if (client.OS === "web") {
      return;
    }
    const result = await client.eval(({ gpu }) => {
      return gpu
        .requestAdapter()
        .then((adapter) => adapter!.requestDevice())
        .then((device) =>
          device.features.has("implicit-device-synchronization"),
        );
    });
    expect(result).toBe(true);
  });

  it("is enabled by default when other features are requested", async () => {
    if (client.OS === "web") {
      return;
    }
    const result = await client.eval(({ gpu }) => {
      return gpu.requestAdapter().then((adapter) => {
        const feature = "depth-clip-control" as GPUFeatureName;
        const requiredFeatures = adapter!.features.has(feature)
          ? [feature]
          : [];
        return adapter!.requestDevice({ requiredFeatures }).then((device) => ({
          implicitSync: device.features.has("implicit-device-synchronization"),
          keptRequested:
            requiredFeatures.length === 0 || device.features.has(feature),
        }));
      });
    });
    expect(result.implicitSync).toBe(true);
    expect(result.keptRequested).toBe(true);
  });

  it("can be opted out with implicitDeviceSynchronization: false", async () => {
    if (client.OS === "web") {
      return;
    }
    const result = await client.eval(({ gpu }) => {
      return gpu
        .requestAdapter()
        .then((adapter) =>
          adapter!.requestDevice({ implicitDeviceSynchronization: false }),
        )
        .then((device) =>
          device.features.has("implicit-device-synchronization"),
        );
    });
    expect(result).toBe(false);
  });

  it("stays enabled when explicitly listed in requiredFeatures alongside the flag", async () => {
    if (client.OS === "web") {
      return;
    }
    // Opting out of the default injection but explicitly requesting the
    // feature must still enable it: the flag only controls the injection.
    const result = await client.eval(({ gpu }) => {
      return gpu
        .requestAdapter()
        .then((adapter) =>
          adapter!.requestDevice({
            implicitDeviceSynchronization: false,
            requiredFeatures: [
              "implicit-device-synchronization" as GPUFeatureName,
            ],
          }),
        )
        .then((device) =>
          device.features.has("implicit-device-synchronization"),
        );
    });
    expect(result).toBe(true);
  });

  // Smoke-tests concurrent use of a single device from two runtimes via the
  // app-side workletDeviceStress helper (a "worklet" directive only works when
  // the babel worklets plugin compiles it into the app bundle, so the stress
  // loop lives in apps/example, not in this eval'd body): the JS runtime keeps
  // creating resources while a worklet runtime encodes and submits work on the
  // same device. Without ImplicitDeviceSynchronization this is a data race
  // inside Dawn; with it, it must complete cleanly. (A TSAN build is the real
  // proof; this catches crashes and validation errors.)
  it("supports concurrent device use from a worklet runtime", async () => {
    if (client.OS === "web") {
      return;
    }
    const result = await client.eval<
      Record<string, unknown>,
      { jsOk: boolean; workletOk: boolean } | "unsupported"
    >(({ gpu, workletDeviceStress }) => {
      if (!workletDeviceStress) {
        return "unsupported" as const;
      }
      // A fresh device so the stress can't disturb the shared harness device.
      return gpu
        .requestAdapter()
        .then((adapter) => adapter!.requestDevice())
        .then((device) => workletDeviceStress(device));
    });
    if (result === "unsupported") {
      return;
    }
    expect(result.jsOk).toBe(true);
    expect(result.workletOk).toBe(true);
  });
});
