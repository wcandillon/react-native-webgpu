import { client } from "./setup";

// Native only: the web has nothing to purge.
describe("purgeGpuResources", () => {
  it("keeps what is alive, with destroyed resources and submitted work around", async () => {
    if (client.OS === "web") {
      return;
    }
    const result = await client.eval(({ device }) => {
      const alive = device.createBuffer({
        size: 16,
        usage: GPUBufferUsage.COPY_SRC | GPUBufferUsage.COPY_DST,
      });
      device.queue.writeBuffer(alive, 0, new Uint32Array([1, 2, 3, 4]));
      for (let i = 0; i < 20; i++) {
        const texture = device.createTexture({
          size: [256, 256],
          format: "rgba8unorm",
          usage: GPUTextureUsage.RENDER_ATTACHMENT,
        });
        const encoder = device.createCommandEncoder();
        encoder
          .beginRenderPass({
            colorAttachments: [
              {
                view: texture.createView(),
                clearValue: [1, 0, 0, 1],
                loadOp: "clear",
                storeOp: "store",
              },
            ],
          })
          .end();
        device.queue.submit([encoder.finish()]);
        texture.destroy();
        RNWebGPU.purgeGpuResources();
      }
      const readback = device.createBuffer({
        size: 16,
        usage: GPUBufferUsage.MAP_READ | GPUBufferUsage.COPY_DST,
      });
      const encoder = device.createCommandEncoder();
      encoder.copyBufferToBuffer(alive, 0, readback, 0, 16);
      device.queue.submit([encoder.finish()]);
      return readback
        .mapAsync(GPUMapMode.READ)
        .then(() => Array.from(new Uint32Array(readback.getMappedRange())));
    });
    expect(result).toEqual([1, 2, 3, 4]);
  });

  it("accepts a destroyed device and one its runtime's thread owns", async () => {
    if (client.OS === "web") {
      return;
    }
    const result = await client.eval(({ gpu }) => {
      const request = (implicitDeviceSynchronization: boolean) =>
        gpu
          .requestAdapter()
          .then((adapter) =>
            adapter!.requestDevice({ implicitDeviceSynchronization }),
          );
      return Promise.all([request(true), request(false)]).then(
        ([destroyed, unsynchronized]) => {
          destroyed.destroy();
          RNWebGPU.purgeGpuResources();
          return unsynchronized.queue.onSubmittedWorkDone().then(() => {
            unsynchronized.destroy();
            return true;
          });
        },
      );
    });
    expect(result).toBe(true);
  });
});
