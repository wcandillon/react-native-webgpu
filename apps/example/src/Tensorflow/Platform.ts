import type { Platform } from "@tensorflow/tfjs-core";
import type { RequestDetails } from "@tensorflow/tfjs-core/dist/io/types";

export class PlatformReactNative implements Platform {
  fetch(path: string, init?: RequestInit, _options?: RequestDetails) {
    return fetch(path, init);
  }

  encode(text: string, encoding: BufferEncoding) {
    return new Uint8Array(Buffer.from(text, encoding));
  }

  decode(bytes: Uint8Array, encoding: BufferEncoding) {
    return Buffer.from(bytes).toString(encoding);
  }

  now() {
    return Date.now();
  }

  setTimeoutCustom() {
    throw new Error("react native does not support setTimeoutCustom");
  }

  isTypedArray(
    a: unknown,
  ): a is Uint8Array | Uint8ClampedArray | Int32Array | Float32Array {
    return (
      a instanceof Float32Array ||
      a instanceof Int32Array ||
      a instanceof Uint8Array ||
      a instanceof Uint8ClampedArray
    );
  }
}
