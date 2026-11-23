import { execFileSync } from "child_process";
import fs from "fs";
import os from "os";
import path from "path";

import { client } from "./setup";

// createVideoWriter encodes frames drawn by the GPU into an .mp4 file on the
// device: AVAssetWriter on Apple platforms, MediaCodec on Android. The frames
// here are cleared to a color by a render pass: red for the first half of
// them, green for the rest. The files are read back with ffprobe and ffmpeg.
//
// The functions passed to client.eval run on the device as source: chained
// rather than async, without the helpers Babel would compile async functions
// to. The helper writing the frames is passed along as source too.
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

const native = () => client.OS !== "web";
const probes = () => hasTool("ffprobe") && hasTool("ffmpeg");
// The tests that read the files back: on a device, with ffprobe and ffmpeg.
const itProbes = (name: string, fn: () => Promise<void>, timeout?: number) =>
  it(name, () => (native() && probes() ? fn() : Promise.resolve()), timeout);

// The package of the Android app whose private storage `devicePath` is in.
const androidPackageOf = (devicePath: string) =>
  /^\/data\/(?:user\/\d+|data)\/([^/]+)\//.exec(devicePath)?.[1];

// Copies a file the app wrote to this machine: the iOS Simulator's and the
// macOS app's files are this machine's already, an Android app's private
// storage is read through adb (run-as, which a debug build allows).
const pullDeviceFile = (devicePath: string) => {
  const local = path.join(
    fs.mkdtempSync(path.join(os.tmpdir(), "rnwgpu-e2e-")),
    path.basename(devicePath),
  );
  const pkg = androidPackageOf(devicePath);
  if (pkg) {
    fs.writeFileSync(
      local,
      execFileSync("adb", ["exec-out", "run-as", pkg, "cat", devicePath], {
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

// Copies a file of this machine to where the app reads.
const pushDeviceFile = (local: string, devicePath: string) => {
  const pkg = androidPackageOf(devicePath);
  if (pkg) {
    execFileSync(
      "adb",
      ["shell", "run-as", pkg, "sh", "-c", `'cat > ${devicePath}'`],
      { input: fs.readFileSync(local) },
    );
  } else {
    fs.copyFileSync(local, devicePath);
  }
};

const deviceFileExists = (devicePath: string) => {
  const pkg = androidPackageOf(devicePath);
  if (pkg) {
    try {
      execFileSync("adb", ["shell", "run-as", pkg, "ls", devicePath], {
        stdio: "ignore",
      });
      return true;
    } catch {
      return false;
    }
  }
  return fs.existsSync(devicePath);
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

interface Stream {
  codec_type: string;
  codec_name: string;
  width?: number;
  height?: number;
  duration: string;
  nb_read_frames?: string;
}

// The streams of a file: their duration is what plays, after the edit list.
const streamsOf = (file: string): Stream[] =>
  JSON.parse(
    execFileSync(
      "ffprobe",
      [
        "-v",
        "error",
        "-count_frames",
        "-show_entries",
        "stream=codec_type,codec_name,width,height,duration,nb_read_frames",
        "-of",
        "json",
        file,
      ],
      { encoding: "utf8" },
    ),
  ).streams;

const streamOf = (file: string, type: string) => {
  const stream = streamsOf(file).find((s) => s.codec_type === type);
  if (!stream) {
    throw new Error(`${file} has no ${type} stream`);
  }
  return stream;
};

// Where the app writes: the directory of a writer opened on a relative path.
let directory: Promise<string> | undefined;
const appDirectory = () =>
  (directory ??= client.eval(({ device }) => {
    const writer = RNWebGPU.createVideoWriter("e2e-probe.mp4", {
      width: 16,
      height: 16,
      frameRate: 1,
      device,
    });
    return writer
      .cancel()
      .then(() => writer.path.slice(0, writer.path.lastIndexOf("/")));
  }));

// Copies a sound of assets/ to the app's directory, and says where.
const pushSound = (name: string) =>
  appDirectory().then((dir) => {
    const destination = `${dir}/e2e-${name}`;
    pushDeviceFile(path.join(__dirname, "assets", name), destination);
    return destination;
  });

// Writes a frame at each of `times` (seconds), red for the first half and
// green for the rest, then finishes unless `finish` is false. Runs on the
// device: passed as source.
const writeFrames = (
  device: GPUDevice,
  writer: ReturnType<typeof RNWebGPU.createVideoWriter>,
  times: number[],
  finish = true,
): Promise<void> => {
  const write = (i: number): Promise<void> => {
    if (i === times.length) {
      return finish ? writer.finish() : Promise.resolve();
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
            clearValue: i < times.length / 2 ? [1, 0, 0, 1] : [0, 1, 0, 1],
            loadOp: "clear",
            storeOp: "store",
          },
        ],
      });
      pass.end();
      device.queue.submit([encoder.finish()]);
      writer.append(buffer, memory.endAccess(texture), times[i]);
      texture.destroy();
      memory.destroy();
      return write(i + 1);
    });
  };
  return write(0);
};

interface Export {
  path: string;
  error: string;
}

// Writes the frames at `times` to `file`, with the sound at `audio` if set.
// The error is "code: message", empty once written.
const exportFrames = (
  file: string,
  times: number[],
  frameRate: number,
  audio?: string,
) =>
  client.eval(
    ({ device, ctx }) => {
      const write = eval(`(${ctx.writeFrames})`);
      const writer = RNWebGPU.createVideoWriter(ctx.file, {
        width: 128,
        height: 128,
        frameRate: ctx.frameRate,
        device,
        ...(ctx.audio ? { audio: ctx.audio } : {}),
      });
      return write(device, writer, ctx.times).then(
        (): Export => ({ path: writer.path, error: "" }),
        (e: Error & { code?: string }) =>
          writer.cancel().then((): Export => ({
            path: "",
            error: `${e.code}: ${e.message}`,
          })),
      );
    },
    {
      ctx: {
        writeFrames: writeFrames.toString(),
        file,
        times,
        frameRate,
        audio: audio ?? "",
      },
    },
  ) as Promise<Export>;

const framesAt = (count: number, frameRate: number) =>
  Array.from({ length: count }, (_, i) => i / frameRate);

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
          (e: Error & { code?: string }) =>
            writer.cancel().then((): EvalResult => ({
              kind: "fail",
              reason: `${e.code}: ${e.message}`,
            })),
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
    if (!probes()) {
      return;
    }
    const file = pullDeviceFile(result.path);
    if (!file) {
      return;
    }
    const stream = streamOf(file, "video");
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

  // The sound is cut where the video ends: exactly through the edit list on
  // Apple platforms, at a whole AAC frame (within half of one, 10.7 ms at
  // 48 kHz) on Android, whose MediaMuxer writes no edit list.
  const kSoundEnd = 0.012;

  itProbes(
    "muxes an AAC sound as it is, cut where the video ends",
    async () => {
      const sound = await pushSound("sine-8s.m4a");
      const result = await exportFrames(
        "e2e-video-writer-m4a.mp4",
        framesAt(20, 10),
        10,
        sound,
      );
      expect(result.error).toBe("");
      const file = pullDeviceFile(result.path)!;
      const video = streamOf(file, "video");
      const audio = streamOf(file, "audio");
      expect(video.nb_read_frames).toBe("20");
      expect(Math.abs(Number(video.duration) - 2)).toBeLessThan(0.002);
      expect(audio.codec_name).toBe("aac");
      expect(Math.abs(Number(audio.duration) - 2)).toBeLessThan(kSoundEnd);
    },
  );

  itProbes(
    "encodes an MP3 sound to AAC, cut where the video ends",
    async () => {
      const sound = await pushSound("sine-8s.mp3");
      const result = await exportFrames(
        "e2e-video-writer-mp3.mp4",
        framesAt(20, 10),
        10,
        sound,
      );
      expect(result.error).toBe("");
      const file = pullDeviceFile(result.path)!;
      const audio = streamOf(file, "audio");
      expect(audio.codec_name).toBe("aac");
      expect(Math.abs(Number(audio.duration) - 2)).toBeLessThan(kSoundEnd);
    },
  );

  // Frames far apart, as a slideshow appends them: the last one lasts
  // 1 / frameRate (MediaMuxer would give it the gap before it), and the sound
  // keeps up with frames that lead it by seconds.
  itProbes(
    "ends the video one frame after the last of frames far apart",
    async () => {
      const sound = await pushSound("sine-8s.m4a");
      const result = await exportFrames(
        "e2e-video-writer-slideshow.mp4",
        [0, 2, 4, 6],
        30,
        sound,
      );
      expect(result.error).toBe("");
      const file = pullDeviceFile(result.path)!;
      const video = streamOf(file, "video");
      const audio = streamOf(file, "audio");
      const end = 6 + 1 / 30;
      expect(video.nb_read_frames).toBe("4");
      expect(Math.abs(Number(video.duration) - end)).toBeLessThan(0.002);
      expect(Math.abs(Number(audio.duration) - end)).toBeLessThan(kSoundEnd);
    },
    60000,
  );

  itProbes(
    "cancels: the file is deleted, pending calls reject, the path is free",
    async () => {
      const result = await client.eval(
        ({ device, ctx }) => {
          const write = eval(`(${ctx.writeFrames})`);
          const writer = RNWebGPU.createVideoWriter(
            "e2e-video-writer-cancelled.mp4",
            { width: 128, height: 128, frameRate: 10, device },
          );
          const codeOf = (p: Promise<unknown>) =>
            p.then(
              () => "resolved",
              (e: Error & { code?: string }) => String(e.code),
            );
          // Five frames written, then a dequeue and the finish pending.
          return write(device, writer, [0, 0.1, 0.2, 0.3, 0.4], false).then(
            () => {
              const dequeued = codeOf(writer.dequeueBuffer());
              const cancelled = writer.cancel();
              const finished = codeOf(writer.finish());
              return Promise.all([dequeued, finished, cancelled]).then(
                ([dequeue, finish]) => ({ dequeue, finish, path: writer.path }),
              );
            },
          );
        },
        { ctx: { writeFrames: writeFrames.toString() } },
      );
      expect(result.finish).toBe("cancelled");
      expect(["cancelled", "resolved"]).toContain(result.dequeue);
      // Deleted once cancel() resolved.
      expect(deviceFileExists(result.path)).toBe(false);
      // A new export to the same path is not deleted by the cancelled one.
      const again = await exportFrames(
        "e2e-video-writer-cancelled.mp4",
        framesAt(5, 10),
        10,
      );
      expect(again.error).toBe("");
      await new Promise((resolve) => setTimeout(resolve, 1000));
      const file = pullDeviceFile(again.path)!;
      expect(streamOf(file, "video").nb_read_frames).toBe("5");
    },
    60000,
  );

  it("takes back the buffers released without being appended", async () => {
    if (!native()) {
      return;
    }
    const result = await client.eval(({ device }) => {
      const writer = RNWebGPU.createVideoWriter(
        "e2e-video-writer-released.mp4",
        {
          width: 64,
          height: 64,
          frameRate: 10,
          device,
        },
      );
      // More than the writer has in flight, each released unused.
      const take = (n: number): Promise<string> =>
        n === 0
          ? Promise.resolve("ok")
          : Promise.race([
              writer.dequeueBuffer().then((buffer) => {
                buffer.release();
                return take(n - 1);
              }),
              new Promise<string>((resolve) =>
                setTimeout(() => resolve(`hung at ${n}`), 2000),
              ),
            ]);
      return take(6).then((r) => writer.cancel().then(() => r));
    });
    expect(result).toBe("ok");
  });

  it("rejects the dequeues still waiting once finish() is called", async () => {
    if (!native()) {
      return;
    }
    const result = await client.eval(({ device }) => {
      const writer = RNWebGPU.createVideoWriter("e2e-video-writer-finish.mp4", {
        width: 64,
        height: 64,
        frameRate: 10,
        device,
      });
      const held: { release(): void }[] = [];
      // More dequeues than the writer has buffers in flight: the last wait.
      const dequeues = Array.from({ length: 8 }, () =>
        Promise.race([
          writer.dequeueBuffer().then(
            (buffer) => {
              held.push(buffer);
              return "buffer";
            },
            (e: Error & { code?: string }) => String(e.code),
          ),
          new Promise<string>((resolve) =>
            setTimeout(() => resolve("hung"), 3000),
          ),
        ]),
      );
      // Once the first buffers are out.
      return new Promise((resolve) => setTimeout(resolve, 500)).then(() => {
        const finished = writer.finish().then(
          () => "resolved",
          (e: Error & { code?: string }) => String(e.code),
        );
        return Promise.all([finished, ...dequeues]).then(([finish, ...d]) => {
          held.forEach((buffer) => buffer.release());
          return writer.cancel().then(() => ({ finish, dequeues: d }));
        });
      });
    });
    // No frame was appended.
    expect(result.finish).toBe("invalid-argument");
    expect(result.dequeues).not.toContain("hung");
    expect(result.dequeues).toContain("buffer");
    expect(result.dequeues).toContain("invalid-argument");
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
