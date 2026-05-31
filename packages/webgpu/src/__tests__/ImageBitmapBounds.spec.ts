import { client, itSkipsOnWeb } from "./setup";

// These tests exercise the same BufferSource bounds checking as
// ArrayBufferBounds.spec.ts, but through the non-standard createImageBitmap
// overload that accepts an ArrayBuffer / TypedArray of encoded image bytes.
//
// That path lives in cpp/rnwgpu/api/RNWebGPU.h and now reuses the shared
// rnwgpu::ArrayBuffer converter (cpp/rnwgpu/ArrayBuffer.h), so a spoofed
// BufferSource that lies about byteOffset / byteLength must be rejected rather
// than producing an out-of-bounds read when the bytes are copied. Before the
// fix these cases could read past the backing ArrayBuffer and crash.
//
// The overload is native-only (not part of the standard web API), so these are
// skipped on web.

describe("createImageBitmap bounds", () => {
  itSkipsOnWeb(
    "rejects a spoofed BufferSource whose byteLength exceeds the buffer",
    async () => {
      await expect(
        client.eval(() => {
          const realBuffer = new ArrayBuffer(4);
          const spoofed = {
            buffer: realBuffer,
            byteOffset: 0,
            byteLength: 1 << 24, // 16 MB, far beyond the 4-byte backing store
            BYTES_PER_ELEMENT: 1,
          };
          // eslint-disable-next-line @typescript-eslint/no-explicit-any
          return createImageBitmap(spoofed as any).then(() => true);
        }),
      ).rejects.toBeDefined();
    },
  );

  itSkipsOnWeb(
    "rejects a spoofed BufferSource whose byteOffset is past the end",
    async () => {
      await expect(
        client.eval(() => {
          const realBuffer = new ArrayBuffer(4);
          const spoofed = {
            buffer: realBuffer,
            byteOffset: 1 << 24,
            byteLength: 4,
            BYTES_PER_ELEMENT: 1,
          };
          // eslint-disable-next-line @typescript-eslint/no-explicit-any
          return createImageBitmap(spoofed as any).then(() => true);
        }),
      ).rejects.toBeDefined();
    },
  );

  itSkipsOnWeb(
    "rejects a BufferSource with a negative byteOffset",
    async () => {
      await expect(
        client.eval(() => {
          const realBuffer = new ArrayBuffer(16);
          const spoofed = {
            buffer: realBuffer,
            byteOffset: -8, // wraps to a huge size_t in native code
            byteLength: 8,
            BYTES_PER_ELEMENT: 1,
          };
          // eslint-disable-next-line @typescript-eslint/no-explicit-any
          return createImageBitmap(spoofed as any).then(() => true);
        }),
      ).rejects.toBeDefined();
    },
  );
});
