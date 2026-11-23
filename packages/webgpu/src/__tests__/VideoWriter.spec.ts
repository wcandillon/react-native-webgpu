import { execFileSync } from "child_process";
import fs from "fs";
import os from "os";
import path from "path";

import { client } from "./setup";

// createVideoWriter encodes frames drawn by the GPU into an .mp4 file on the
// device: AVAssetWriter on Apple platforms, MediaCodec on Android. The frames
// here are cleared to a color by a render pass: red for the first half
// second, green for the second.
type EvalResult =
  | { kind: "skip"; reason: string }
  | { kind: "fail"; reason: string }
  | { kind: "ok"; path: string; frames: number };

const hasTool = (tool: string) => {
  try {
    execFileSync(tool, ["-version"], { stdio: "ignore" });
    return true;
  } catch {
    return false;
  }
};

// Copies a file the app wrote to this machine: the iOS Simulator's files are
// this machine's already, an Android app's private storage is read through
// adb (run-as, which a debug build allows).
const pullDeviceFile = (devicePath: string) => {
  const local = path.join(
    fs.mkdtempSync(path.join(os.tmpdir(), "rnwgpu-e2e-")),
    path.basename(devicePath),
  );
  const match = /^\/data\/(?:user\/\d+|data)\/([^/]+)\//.exec(devicePath);
  if (match) {
    fs.writeFileSync(
      local,
      execFileSync("adb", ["exec-out", "run-as", match[1], "cat", devicePath], {
        maxBuffer: 64 * 1024 * 1024,
      }),
    );
  } else if (fs.existsSync(devicePath)) {
    fs.copyFileSync(devicePath, local);
  } else {
    return null;
  }
  return local;
};

const pixel = (file: string, time: number) => {
  const raw = execFileSync(
    "ffmpeg",
    [
      "-loglevel",
      "error",
      "-ss",
      String(time),
      "-i",
      file,
      "-frames:v",
      "1",
      "-vf",
      "format=rgb24,crop=1:1:64:64",
      "-f",
      "rawvideo",
      "-pix_fmt",
      "rgb24",
      "-",
    ],
    { maxBuffer: 1024 },
  );
  return [raw[0], raw[1], raw[2]];
};

describe("VideoWriter", () => {
  it("encodes the frames it is given", async () => {
    const result = await client.eval<Record<string, never>, EvalResult>(
      ({ device }) => {
        if (typeof RNWebGPU?.createVideoWriter !== "function") {
          return {
            kind: "skip",
            reason: "RNWebGPU.createVideoWriter is unavailable",
          };
        }
        const frames = 10;
        const frameRate = 10;
        let writer: ReturnType<typeof RNWebGPU.createVideoWriter>;
        try {
          writer = RNWebGPU.createVideoWriter("e2e-video-writer.mp4", {
            width: 128,
            height: 128,
            frameRate,
            device,
          });
        } catch (e) {
          const error = e as Error & { code?: string };
          if (error.code === "unsupported") {
            return { kind: "skip", reason: error.message };
          }
          return { kind: "fail", reason: `${error.code}: ${error.message}` };
        }
        // Chained rather than async: the function runs on the device as
        // source, without the helpers Babel would compile async functions to.
        const writeFrame = (i: number): Promise<void> => {
          if (i === frames) {
            return writer.finish();
          }
          return writer.dequeueBuffer().then((buffer) => {
            const memory = device.importSharedTextureMemory({ source: buffer });
            const texture = memory.createTexture();
            memory.beginAccess(texture, false);
            const encoder = device.createCommandEncoder();
            const pass = encoder.beginRenderPass({
              colorAttachments: [
                {
                  view: texture.createView(),
                  clearValue: i < frames / 2 ? [1, 0, 0, 1] : [0, 1, 0, 1],
                  loadOp: "clear",
                  storeOp: "store",
                },
              ],
            });
            pass.end();
            device.queue.submit([encoder.finish()]);
            writer.append(buffer, memory.endAccess(texture), i / frameRate);
            texture.destroy();
            memory.destroy();
            return writeFrame(i + 1);
          });
        };
        return writeFrame(0).then(
          (): EvalResult => ({ kind: "ok", path: writer.path, frames }),
          (e: Error & { code?: string }): EvalResult => {
            writer.cancel();
            return { kind: "fail", reason: `${e.code}: ${e.message}` };
          },
        );
      },
    );
    if (result.kind === "skip") {
      console.warn(`[VideoWriter] skipped: ${result.reason}`);
      return;
    }
    if (result.kind === "fail") {
      throw new Error(result.reason);
    }
    expect(result.path.endsWith("/e2e-video-writer.mp4")).toBe(true);
    if (!hasTool("ffprobe") || !hasTool("ffmpeg")) {
      return;
    }
    const file = pullDeviceFile(result.path);
    if (!file) {
      return;
    }
    const probe = execFileSync(
      "ffprobe",
      [
        "-v",
        "error",
        "-count_frames",
        "-show_entries",
        "stream=codec_name,width,height,nb_read_frames",
        "-of",
        "json",
        file,
      ],
      { encoding: "utf8" },
    );
    const [stream] = JSON.parse(probe).streams;
    expect([
      stream.codec_name,
      stream.width,
      stream.height,
      stream.nb_read_frames,
    ]).toEqual(["h264", 128, 128, "10"]);
    const near = (actual: number[], expected: number[]) =>
      actual.forEach((channel, i) =>
        expect(Math.abs(channel - expected[i])).toBeLessThan(40),
      );
    near(pixel(file, 0.25), [255, 0, 0]);
    near(pixel(file, 0.75), [0, 255, 0]);
  });

  it("reports what it cannot do with a code", async () => {
    const result = await client.eval<
      Record<string, never>,
      { kind: "skip" } | { kind: "ok"; codes: string[] }
    >(({ device }) => {
      if (typeof RNWebGPU?.createVideoWriter !== "function") {
        return { kind: "skip" };
      }
      const options = { width: 64, height: 64, frameRate: 10, device };
      const attempt = (file: string, extra: Record<string, unknown>) => {
        try {
          RNWebGPU.createVideoWriter(file, {
            ...options,
            ...extra,
          } as never).cancel();
          return "created";
        } catch (e) {
          return String((e as Error & { code?: string }).code);
        }
      };
      return {
        kind: "ok",
        codes: [
          attempt("e2e-invalid.mp4", { width: 0 }),
          attempt("e2e-invalid.mp4", { width: 65 }),
          attempt("e2e-invalid.mp4", { device: undefined }),
          attempt("/no/such/directory/out.mp4", {}),
          attempt("e2e-invalid.mp4", { audio: "/no/such/sound.m4a" }),
        ],
      };
    });
    if (result.kind === "skip") {
      return;
    }
    expect(result.codes).toEqual([
      "invalid-argument",
      "invalid-argument",
      "invalid-argument",
      "write-failed",
      "audio-failed",
    ]);
  });
});
