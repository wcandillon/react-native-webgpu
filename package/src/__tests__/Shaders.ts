import { client } from "./setup";

describe("Triangle", () => {
  it("creates a shader module", async () => {
    const result = await client.eval(({ device, triangleVertWGSL }) => {
      const mod = device.createShaderModule({
        code: triangleVertWGSL,
        label: "triangleVertWGSL",
      });
      return mod.label;
    });
    expect(result).toBe("triangleVertWGSL");
  });
  // it("creates a shader module", async () => {
  //   const result = await client.eval(({ device, triangleVertWGSL }) => {
  //     const mod = device.createShaderModule({
  //       code: triangleVertWGSL,
  //       label: "triangleVertWGSL",
  //     });
  //     //const { label } = mod;
  //     return mod.getCompilationInfo().then((info) => JSON.stringify(info));
  //   });
  //   console.log({ result });
  //   expect(true).toBe(true);
  // });
  // it("create the pipeline", async () => {
  //   const result = await client.eval(
  //     ({ device, triangleVertWGSL, redFragWGSL }) => {
  //       const pipeline = device.createRenderPipeline({
  //         layout: "auto",
  //         vertex: {
  //           module: device.createShaderModule({
  //             code: triangleVertWGSL,
  //           }),
  //         },
  //         fragment: {
  //           module: device.createShaderModule({
  //             code: redFragWGSL,
  //           }),
  //           targets: [
  //             {
  //               format: "rgba8unorm",
  //             },
  //           ],
  //         },
  //         primitive: {
  //           topology: "triangle-list",
  //         },
  //       });
  //       return pipeline !== null;
  //     },
  //   );
  //   expect(result).toBe(true);
  // });
});
