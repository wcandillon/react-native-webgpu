import { client } from "./setup";

describe("TextDecoder", () => {
  it("Text Decoder (1)", async () => {
    const result = await client.eval(() => {
      return RNWebGPU.DecodeToUTF8(new Uint8Array([240, 160, 174, 183]));
    });
    expect(result).toEqual("𠮷");
  });
  it("Text Decoder (2)", async () => {
    const result = await client.eval(() => {
      return RNWebGPU.DecodeToUTF8(new Int8Array([-16, -96, -82, -73]));
    });
    expect(result).toEqual("𠮷");
  });
  it("Text Decoder (3)", async () => {
    const result = await client.eval(() => {
      const utf8decoder = new TextDecoder();
      const i32arr = new Int32Array([-1213292304]);
      return utf8decoder.decode(i32arr);
    });
    expect(result).toEqual("𠮷");
  });
});
