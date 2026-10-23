import { client } from "./setup";

// importDevice() wraps a device another library created, here Skia's Graphite
// device. Its async operations settle through Dawn's ProcessEvents pump, which
// has to run on the Dawn instance that owns the device. That is not webgpu's
// own instance when react-native-skia installs after react-native-webgpu (the
// example app's order, see apps/example/src/Tests.tsx), so a pump bound to the
// wrong instance leaves every promise below pending forever. The web and node
// runners have no device to import and return null, which skips the
// assertions; so does a native run on a device where Skia has no GPU.
describe("importDevice", () => {
  it("settles async operations on a device imported from Skia", async () => {
    const result = await client.eval(
      ({ importSkiaDevice, shaders: { triangleVertWGSL, redFragWGSL } }) => {
        const device = importSkiaDevice ? importSkiaDevice() : null;
        if (device === null) {
          return null;
        }
        // Each operation races a timer, so a hang names the operation instead
        // of running into the harness' eval timeout.
        const within = <T>(promise: Promise<T>) =>
          Promise.race([
            promise.then((value) => ({ settled: true, value })),
            new Promise<{ settled: false; value: null }>((resolve) =>
              setTimeout(() => resolve({ settled: false, value: null }), 5000),
            ),
          ]);

        device.pushErrorScope("validation");
        // Invalid, maxAnisotropy must be at least 1.
        device.createSampler({ maxAnisotropy: 0 });
        const errorScope = within(device.popErrorScope()).then((r) => ({
          settled: r.settled,
          hasError: r.settled && r.value !== null,
        }));

        const pipeline = within(
          device.createRenderPipelineAsync({
            layout: "auto",
            vertex: {
              module: device.createShaderModule({ code: triangleVertWGSL }),
            },
            fragment: {
              module: device.createShaderModule({ code: redFragWGSL }),
              targets: [{ format: "rgba8unorm" }],
            },
          }),
        ).then((r) => ({
          settled: r.settled,
          isPipeline:
            r.settled && typeof r.value.getBindGroupLayout === "function",
        }));

        const src = device.createBuffer({
          size: 16,
          usage: GPUBufferUsage.COPY_SRC | GPUBufferUsage.COPY_DST,
        });
        const dst = device.createBuffer({
          size: 16,
          usage: GPUBufferUsage.COPY_DST | GPUBufferUsage.MAP_READ,
        });
        device.queue.writeBuffer(src, 0, new Uint32Array([0xc0ffee, 1, 2, 3]));
        const encoder = device.createCommandEncoder();
        encoder.copyBufferToBuffer(src, 0, dst, 0, 16);
        device.queue.submit([encoder.finish()]);
        const workDone = within(device.queue.onSubmittedWorkDone()).then(
          (r) => r.settled,
        );
        const mapped = within(dst.mapAsync(GPUMapMode.READ)).then((r) => {
          if (!r.settled) {
            return { settled: false, first: -1 };
          }
          const first = new DataView(dst.getMappedRange()).getUint32(0, true);
          dst.unmap();
          return { settled: true, first };
        });

        return Promise.all([errorScope, pipeline, workDone, mapped]).then(
          (results) => ({
            errorScope: results[0],
            pipeline: results[1],
            workDoneSettled: results[2],
            mapped: results[3],
          }),
        );
      },
    );
    if (result === null) {
      return;
    }
    expect(result.errorScope).toEqual({ settled: true, hasError: true });
    expect(result.pipeline).toEqual({ settled: true, isPipeline: true });
    expect(result.workDoneSettled).toBe(true);
    expect(result.mapped).toEqual({ settled: true, first: 0xc0ffee });
  });
});
