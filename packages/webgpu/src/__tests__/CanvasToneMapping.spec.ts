import { client, itSkipsOnWeb } from "./setup";

// What RNWebGPU.describeCanvasLayer reports for the CAMetalLayer of a mounted
// canvas.
type LayerInfo = {
  colorspace?: string;
  wantsExtendedDynamicRangeContent?: boolean;
  preferredDynamicRange?: string;
  contentsHeadroom?: number;
};

type EvalResult =
  | { kind: "skip"; reason: string }
  | { kind: "ok"; extended: LayerInfo; standard: LayerInfo };

// The harness (apps/example/src/Tests.tsx) mounts two canvases, one with
// toneMapping "extended" and one with "standard". Since the 26 SDKs the layer
// only shows values above 1.0 brighter than SDR white when it asks for a high
// dynamic range and declares a headroom: wantsExtendedDynamicRangeContent
// alone no longer does it. preferredDynamicRange and contentsHeadroom are
// reported only on iOS, macOS and visionOS 26+, in an app built with Xcode 26+.
describe("Canvas toneMapping", () => {
  itSkipsOnWeb(
    "asks the layer for EDR with the extended mode, and only then",
    async () => {
      const result = await client.eval<Record<string, never>, EvalResult>(
        ({ views }) => {
          const ids = views?.toneMappingCanvases;
          if (!ids || ids.extended === null || ids.standard === null) {
            return { kind: "skip", reason: "no toneMapping canvases mounted" };
          }
          const describe = (contextId: number) => {
            const json = RNWebGPU.describeCanvasLayer(contextId);
            return json ? (JSON.parse(json) as LayerInfo) : null;
          };
          const extended = describe(ids.extended);
          const standard = describe(ids.standard);
          if (!extended || !standard) {
            return { kind: "skip", reason: "no Core Animation layer here" };
          }
          if (extended.preferredDynamicRange === undefined) {
            return {
              kind: "skip",
              reason: "preferredDynamicRange needs the 26 SDKs and OSes",
            };
          }
          return { kind: "ok", extended, standard };
        },
      );

      if (result.kind === "skip") {
        console.log(`CanvasToneMapping: skipping (${result.reason})`);
        return;
      }
      const { extended, standard } = result;

      expect(extended.preferredDynamicRange).toBe("high");
      expect(extended.contentsHeadroom).toBeGreaterThan(1);
      expect(extended.wantsExtendedDynamicRangeContent).toBe(true);

      expect(standard.preferredDynamicRange).toBe("standard");
      expect(standard.contentsHeadroom).toBe(0);
      expect(standard.wantsExtendedDynamicRangeContent).toBe(false);

      // Both default to colorSpace "srgb", tagged with the gamma-encoded
      // extended variant so that float formats keep values above 1.0.
      expect(extended.colorspace).toBe("kCGColorSpaceExtendedSRGB");
      expect(standard.colorspace).toBe("kCGColorSpaceExtendedSRGB");
    },
  );
});
