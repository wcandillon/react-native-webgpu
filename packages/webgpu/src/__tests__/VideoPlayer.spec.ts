import { client } from "./setup";

// The player decodes on the device: AVPlayer on iOS, MediaPlayer on Android.
// The test video comes from writeTestVideoFile() (256x256, 30 fps, 3 seconds),
// which exists on Apple platforms only, so the specs skip elsewhere.
type EvalResult =
  | { kind: "skip"; reason: string }
  | {
      kind: "ok" | "fail";
      duration: number;
      videoWidth: number;
      videoHeight: number;
      frameRate: number;
      rotation: number;
      paused: boolean;
      loop: boolean;
      frames: number;
      frameWidth: number;
      frameHeight: number;
      currentTime: number;
    };

describe("VideoPlayer", () => {
  it("decodes a test video and reports its metadata", async () => {
    const result = await client.eval<Record<string, never>, EvalResult>(() => {
      if (typeof RNWebGPU?.createVideoPlayer !== "function") {
        return {
          kind: "skip",
          reason: "RNWebGPU.createVideoPlayer is unavailable",
        };
      }
      let path: string;
      try {
        path = RNWebGPU.writeTestVideoFile();
      } catch (e) {
        return {
          kind: "skip",
          reason: `writeTestVideoFile is unavailable: ${e}`,
        };
      }
      const player = RNWebGPU.createVideoPlayer(path);
      player.loop = true;
      player.play();
      return new Promise<EvalResult>((resolve) => {
        const started = Date.now();
        let frames = 0;
        let frameWidth = 0;
        let frameHeight = 0;
        const tick = () => {
          const frame = player.copyLatestFrame();
          if (frame) {
            frames++;
            frameWidth = frame.width;
            frameHeight = frame.height;
            frame.release();
          }
          // The metadata loads asynchronously, and a few frames prove that
          // the decoder runs.
          const ready =
            player.duration > 0 && player.videoWidth > 0 && frames >= 3;
          if (ready || Date.now() - started > 8000) {
            const snapshot = {
              kind: ready ? ("ok" as const) : ("fail" as const),
              duration: player.duration,
              videoWidth: player.videoWidth,
              videoHeight: player.videoHeight,
              frameRate: player.frameRate,
              rotation: player.rotation,
              paused: player.paused,
              loop: player.loop,
              frames,
              frameWidth,
              frameHeight,
              currentTime: player.currentTime,
            };
            player.release();
            resolve(snapshot);
            return;
          }
          setTimeout(tick, 16);
        };
        tick();
      });
    });
    if (result.kind === "skip") {
      console.warn(`[VideoPlayer] skipped: ${result.reason}`);
      return;
    }
    expect(result.kind).toBe("ok");
    expect(result.videoWidth).toBe(256);
    expect(result.videoHeight).toBe(256);
    expect(result.frameWidth).toBe(256);
    expect(result.frameHeight).toBe(256);
    expect(result.duration).toBeCloseTo(3, 0);
    expect(result.frameRate).toBeCloseTo(30, 0);
    expect(result.rotation).toBe(0);
    expect(result.paused).toBe(false);
    expect(result.loop).toBe(true);
    expect(result.frames).toBeGreaterThanOrEqual(3);
    expect(result.currentTime).toBeGreaterThan(0);
  });

  it("pauses and seeks", async () => {
    const result = await client.eval<
      Record<string, never>,
      | { kind: "skip"; reason: string }
      | {
          kind: "ok" | "fail";
          paused: boolean;
          currentTime: number;
          frames: number;
        }
    >(() => {
      if (typeof RNWebGPU?.createVideoPlayer !== "function") {
        return {
          kind: "skip",
          reason: "RNWebGPU.createVideoPlayer is unavailable",
        };
      }
      let path: string;
      try {
        path = RNWebGPU.writeTestVideoFile();
      } catch (e) {
        return {
          kind: "skip",
          reason: `writeTestVideoFile is unavailable: ${e}`,
        };
      }
      const player = RNWebGPU.createVideoPlayer(path);
      player.pause();
      // Seeking while paused delivers the frame at the new position.
      player.currentTime = 1.5;
      return new Promise((resolve) => {
        const started = Date.now();
        let frames = 0;
        const tick = () => {
          const frame = player.copyLatestFrame();
          if (frame) {
            frames++;
            frame.release();
          }
          const { currentTime } = player;
          const ready = frames >= 1 && Math.abs(currentTime - 1.5) < 0.2;
          if (ready || Date.now() - started > 8000) {
            const snapshot = {
              kind: ready ? ("ok" as const) : ("fail" as const),
              paused: player.paused,
              currentTime,
              frames,
            };
            player.release();
            resolve(snapshot);
            return;
          }
          setTimeout(tick, 16);
        };
        tick();
      });
    });
    if (result.kind === "skip") {
      console.warn(`[VideoPlayer] skipped: ${result.reason}`);
      return;
    }
    expect(result.kind).toBe("ok");
    expect(result.paused).toBe(true);
    expect(result.currentTime).toBeCloseTo(1.5, 0);
    expect(result.frames).toBeGreaterThanOrEqual(1);
  });
});
