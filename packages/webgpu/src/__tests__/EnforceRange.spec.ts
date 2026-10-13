import { client } from "./setup";

// WebGPU takes nearly every integer as a WebIDL [EnforceRange] integer: a
// value outside the type's range (or NaN / Infinity) is a TypeError at the call
// site, and a fraction is truncated toward zero. These run against the
// reference implementation too, so the expectations are the browser's.
describe("EnforceRange", () => {
  it("throws a TypeError for integers outside their range", async () => {
    const result = await client.eval(({ device }) => {
      const attempt = (fn: () => unknown) => {
        try {
          fn();
          return "ok";
        } catch (e) {
          return e instanceof TypeError
            ? "TypeError"
            : `${(e as Error).name}: ${(e as Error).message}`;
        }
      };
      const buffer = device.createBuffer({
        size: 16,
        usage: GPUBufferUsage.COPY_SRC | GPUBufferUsage.COPY_DST,
      });
      return [
        // GPUSize64 in a descriptor
        attempt(() =>
          device.createBuffer({ size: -1, usage: GPUBufferUsage.COPY_DST }),
        ),
        attempt(() =>
          device.createBuffer({ size: NaN, usage: GPUBufferUsage.COPY_DST }),
        ),
        // GPUIntegerCoordinate in a descriptor
        attempt(() =>
          device.createTexture({
            size: [4, 4],
            format: "rgba8unorm",
            usage: GPUTextureUsage.COPY_DST,
            mipLevelCount: Infinity,
          }),
        ),
        // GPUSize32 in a descriptor
        attempt(() =>
          device.createQuerySet({ type: "occlusion", count: 2 ** 32 }),
        ),
        // GPUSize64 as a direct argument
        attempt(() => {
          const encoder = device.createCommandEncoder();
          encoder.copyBufferToBuffer(buffer, -4, buffer, 0, 4);
        }),
      ];
    });
    expect(result).toEqual([
      "TypeError",
      "TypeError",
      "TypeError",
      "TypeError",
      "TypeError",
    ]);
  });

  it("truncates fractional integers toward zero", async () => {
    const result = await client.eval(({ device }) => {
      const buffer = device.createBuffer({
        size: 16.9,
        usage: GPUBufferUsage.COPY_DST,
      });
      const texture = device.createTexture({
        size: [4, 4],
        format: "rgba8unorm",
        usage: GPUTextureUsage.COPY_DST,
        mipLevelCount: 2.5,
      });
      return [buffer.size, texture.mipLevelCount];
    });
    expect(result).toEqual([16, 2]);
  });
});
