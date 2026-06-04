import { client } from "./setup";

describe("Dawn toggles", () => {
  it("requests a device with enabled and disabled dawnToggles", async () => {
    const result = await client.eval(({ gpu }) => {
      return gpu.requestAdapter().then((adapter) =>
        adapter!
          .requestDevice({
            dawnToggles: {
              enabledToggles: ["disable_symbol_renaming"],
              disabledToggles: ["lazy_clear_resource_on_first_use"],
            },
          })
          .then((device) => !!device),
      );
    });
    expect(result).toBe(true);
  });

  it("requests a device with no dawnToggles (unchanged behavior)", async () => {
    const result = await client.eval(({ gpu }) => {
      return gpu
        .requestAdapter()
        .then((adapter) => adapter!.requestDevice().then((device) => !!device));
    });
    expect(result).toBe(true);
  });

  it("ignores unknown toggle names without failing device creation", async () => {
    const result = await client.eval(({ gpu }) => {
      return gpu.requestAdapter().then((adapter) =>
        adapter!
          .requestDevice({
            dawnToggles: { enabledToggles: ["this_toggle_does_not_exist"] },
          })
          .then((device) => !!device),
      );
    });
    expect(result).toBe(true);
  });

  // The tests above only assert that requestDevice resolves, which is true
  // whether or not the toggle is parsed and chained onto the device descriptor.
  // This test instead activates a real toggle (skip_validation) and observes a
  // behavioral difference, so it fails if dawnToggles ever stops being applied.
  it("applies skip_validation so a normally-invalid buffer creates without error", async () => {
    // dawnToggles is a non-standard Dawn extension; browsers ignore it, so the
    // behavioral difference only exists on the native (Dawn) backends.
    if (client.OS === "web") {
      return;
    }
    const result = await client.eval(({ gpu }) => {
      // MAP_READ may only be combined with COPY_DST and MAP_WRITE only with
      // COPY_SRC, so MAP_READ | MAP_WRITE is a validation error by default. The
      // buffer is never used on the GPU, so creating it with validation skipped
      // is harmless on every backend.
      const invalid = {
        size: 16,
        usage: GPUBufferUsage.MAP_READ | GPUBufferUsage.MAP_WRITE,
      };
      // A fresh adapter per device: an adapter only vends a single device.
      return gpu
        .requestAdapter()
        .then((adapter) => adapter!.requestDevice())
        .then((control) => {
          control.pushErrorScope("validation");
          control.createBuffer(invalid);
          return control.popErrorScope();
        })
        .then((controlError) =>
          gpu
            .requestAdapter()
            .then((adapter) =>
              adapter!.requestDevice({
                dawnToggles: { enabledToggles: ["skip_validation"] },
              }),
            )
            .then((toggled) => {
              toggled.pushErrorScope("validation");
              toggled.createBuffer(invalid);
              return toggled.popErrorScope();
            })
            .then((toggledError) => ({
              controlHadError: controlError !== null,
              toggledHadError: toggledError !== null,
            })),
        );
    });
    // The operation is genuinely invalid on this build (guards against the test
    // silently passing because the buffer became valid).
    expect(result.controlHadError).toBe(true);
    // skip_validation took effect, which is only possible if dawnToggles was
    // parsed and chained onto the device descriptor.
    expect(result.toggledHadError).toBe(false);
  });
});
