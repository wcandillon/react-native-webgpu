import { client } from "./setup";

describe("Triangle", () => {
  it("create the pipeline", async () => {
    const result = await client.eval(
      ({ device, triangleVertWGSL, redFragWGSL }) => {
        const pipeline = device.createRenderPipeline({
          layout: "auto",
          vertex: {
            module: device.createShaderModule({
              code: triangleVertWGSL,
            }),
          },
          fragment: {
            module: device.createShaderModule({
              code: redFragWGSL,
            }),
            targets: [
              {
                format: "rgba8unorm",
              },
            ],
          },
          primitive: {
            topology: "triangle-list",
          },
        });
        return pipeline !== null;
      },
    );
    expect(result).toBe(true);
  });
});
