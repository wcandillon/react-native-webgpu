import { checkImage, client, encodeImage } from "./setup";

describe("Alpha", () => {
  it("Premultiplied Color", async () => {
    const result = await client.eval(({ device, ctx, canvas }) => {
      const commandEncoder = device.createCommandEncoder();
      const textureView = ctx.getCurrentTexture().createView();
      const alpha = 0.5;
      const renderPassDescriptor: GPURenderPassDescriptor = {
        colorAttachments: [
          {
            view: textureView,
            clearValue: [0.3 * alpha, 0.6 * alpha, 1 * alpha, alpha],
            loadOp: "clear",
            storeOp: "store",
          },
        ],
      };

      const passEncoder = commandEncoder.beginRenderPass(renderPassDescriptor);
      passEncoder.end();
      device.queue.submit([commandEncoder.finish()]);
      return canvas.getImageData();
    });
    const image = encodeImage(result);
    checkImage(image, "snapshots/semi-opaque-cyan.png");
  });
});
