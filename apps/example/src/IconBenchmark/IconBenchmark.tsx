import React, {
  useCallback,
  useEffect,
  useLayoutEffect,
  useMemo,
  useRef,
  useState,
} from "react";
import {
  Platform,
  Pressable,
  StyleSheet,
  Text,
  useWindowDimensions,
  View,
} from "react-native";
import Animated, {
  cancelAnimation,
  Easing,
  useAnimatedStyle,
  useFrameCallback,
  useSharedValue,
  withRepeat,
  withTiming,
} from "react-native-reanimated";
import type { SkCanvas, SkPicture } from "react-native-skia";
import { Canvas, Picture, Skia, TileMode } from "react-native-skia";
import type {
  NativeBufferPixelSize,
  NativeBufferViewRef,
} from "react-native-webgpu";
import {
  createNativeBuffer,
  importDevice,
  NativeBufferView,
} from "react-native-webgpu";

// 200 icons on one screen, drawn by Skia, shown either by one Skia <Canvas>
// per icon or by one NativeBufferView per icon (Skia draws each icon once
// into a native buffer on its own device). Both draw the same SkPicture.

const ICONS = 200;
const COLUMNS = 10;
// Where results are POSTed (a local collector), or null.
const REPORT_URL: string | null = null;
// Runs the whole benchmark on mount.
const AUTORUN = false;
const RUNS = 3;

type Mode = "empty" | "skia" | "nbv" | "oneCanvas";
const MODES: Mode[] = ["skia", "nbv", "oneCanvas"];
const LABELS: Record<Mode, string> = {
  empty: "Empty",
  skia: "Skia Canvas x200",
  nbv: "NativeBufferView x200",
  oneCanvas: "One Skia Canvas",
};

// --- Icons -------------------------------------------------------------------

const star = () => {
  let d = "";
  for (let k = 0; k < 10; k++) {
    const r = k % 2 === 0 ? 9.5 : 4;
    const a = -Math.PI / 2 + (k * Math.PI) / 5;
    d += `${k === 0 ? "M" : "L"}${12 + r * Math.cos(a)} ${12 + r * Math.sin(a)} `;
  }
  return d + "Z";
};

const GLYPHS = [
  "M8 5 L19 12 L8 19 Z",
  "M10 4 H14 V10 H20 V14 H14 V20 H10 V14 H4 V10 H10 Z",
  "M13 2 L4 14 H11 L10 22 L20 9 H13 Z",
  "M12 21 L4.5 13.5 A4.5 4.5 0 0 1 12 6.5 A4.5 4.5 0 0 1 19.5 13.5 Z",
  star(),
  "M4 6 H20 V8 H4 Z M4 11 H20 V13 H4 Z M4 16 H20 V18 H4 Z",
  "M12 3 L21 12 L12 21 L3 12 Z",
  "M12 3 A9 9 0 1 0 12.01 3 Z M12 7 A5 5 0 1 1 11.99 7 Z",
];

const hsl = (h: number, s: number, l: number) => {
  const a = s * Math.min(l, 1 - l);
  const f = (n: number) => {
    const k = (n + h / 30) % 12;
    return l - a * Math.max(-1, Math.min(k - 3, 9 - k, 1));
  };
  return Skia.Color(
    `rgb(${Math.round(f(0) * 255)}, ${Math.round(f(8) * 255)}, ${Math.round(
      f(4) * 255,
    )})`,
  );
};

const drawIcon = (canvas: SkCanvas, index: number, size: number) => {
  const hue = (index * 37) % 360;
  const bg = Skia.Paint();
  bg.setAntiAlias(true);
  bg.setShader(
    Skia.Shader.MakeLinearGradient(
      { x: 0, y: 0 },
      { x: size, y: size },
      [hsl(hue, 0.75, 0.6), hsl((hue + 40) % 360, 0.75, 0.4)],
      null,
      TileMode.Clamp,
    ),
  );
  const rect = Skia.XYWHRect(1, 1, size - 2, size - 2);
  canvas.drawRRect(Skia.RRectXY(rect, size * 0.22, size * 0.22), bg);
  const border = Skia.Paint();
  border.setAntiAlias(true);
  border.setStyle(1);
  border.setStrokeWidth(1);
  border.setColor(Skia.Color("rgba(255, 255, 255, 0.5)"));
  canvas.drawRRect(Skia.RRectXY(rect, size * 0.22, size * 0.22), border);
  const glyph = Skia.Path.MakeFromSVGString(GLYPHS[index % GLYPHS.length])!;
  const white = Skia.Paint();
  white.setAntiAlias(true);
  white.setColor(Skia.Color("white"));
  canvas.save();
  canvas.translate(size * 0.2, size * 0.2);
  canvas.scale((size * 0.6) / 24, (size * 0.6) / 24);
  canvas.drawPath(glyph, white);
  canvas.restore();
};

const recordIcon = (index: number, size: number) => {
  const recorder = Skia.PictureRecorder();
  const canvas = recorder.beginRecording(Skia.XYWHRect(0, 0, size, size));
  drawIcon(canvas, index, size);
  return recorder.finishRecordingAsPicture();
};

const recordGrid = (pictures: SkPicture[], size: number) => {
  const recorder = Skia.PictureRecorder();
  const canvas = recorder.beginRecording(
    Skia.XYWHRect(0, 0, size * COLUMNS, size * (ICONS / COLUMNS)),
  );
  pictures.forEach((picture, i) => {
    canvas.save();
    canvas.translate((i % COLUMNS) * size, Math.floor(i / COLUMNS) * size);
    canvas.drawPicture(picture);
    canvas.restore();
  });
  return recorder.finishRecordingAsPicture();
};

// --- NativeBufferView: Skia draws into a native buffer ------------------------

let sharedDevice: GPUDevice | null = null;
const skiaDevice = () => {
  if (!sharedDevice) {
    sharedDevice = importDevice(Skia.getNativeDevice());
  }
  return sharedDevice;
};

const drawIntoView = (
  view: NativeBufferViewRef,
  picture: SkPicture,
  size: number,
  pixels: NativeBufferPixelSize,
) => {
  const device = skiaDevice();
  const frame = createNativeBuffer(pixels.width, pixels.height);
  const memory = device.importSharedTextureMemory({ source: frame });
  const texture = memory.createTexture();
  // Skia's device is shared by every icon: an access left open by a failed
  // draw must still be ended.
  let accessing = false;
  try {
    memory.beginAccess(texture, false);
    accessing = true;
    const surface = Skia.Surface.MakeFromGPUTexture(texture);
    if (!surface) {
      throw new Error("Skia.Surface.MakeFromGPUTexture returned null");
    }
    const canvas = surface.getCanvas();
    canvas.clear(Skia.Color("transparent"));
    canvas.scale(pixels.width / size, pixels.height / size);
    canvas.drawPicture(picture);
    surface.flush();
    const done = memory.endAccess(texture);
    accessing = false;
    view.present(frame, done);
    surface.dispose();
  } finally {
    if (accessing) {
      memory.endAccess(texture);
    }
    texture.destroy();
    memory.destroy();
    frame.release();
  }
};

const NbvIcon = ({
  picture,
  size,
  onPresented,
}: {
  picture: SkPicture;
  size: number;
  onPresented: () => void;
}) => {
  const ref = useRef<NativeBufferViewRef>(null);
  const onPixelSizeChange = useCallback(
    (pixels: NativeBufferPixelSize) => {
      if (ref.current && pixels.width > 0 && pixels.height > 0) {
        drawIntoView(ref.current, picture, size, pixels);
        onPresented();
      }
    },
    [picture, size, onPresented],
  );
  return (
    <NativeBufferView
      ref={ref}
      style={{ width: size, height: size }}
      onPixelSizeChange={onPixelSizeChange}
    />
  );
};

// --- Grid --------------------------------------------------------------------

const Grid = ({
  mode,
  pictures,
  gridPicture,
  size,
  onMounted,
  onPresented,
}: {
  mode: Mode;
  pictures: SkPicture[];
  gridPicture: SkPicture;
  size: number;
  onMounted: () => void;
  onPresented: () => void;
}) => {
  useLayoutEffect(() => {
    onMounted();
  }, [onMounted]);
  if (mode === "empty") {
    return null;
  }
  if (mode === "oneCanvas") {
    return (
      <Canvas
        style={{ width: size * COLUMNS, height: size * (ICONS / COLUMNS) }}
      >
        <Picture picture={gridPicture} />
      </Canvas>
    );
  }
  return (
    <View style={[styles.grid, { width: size * COLUMNS }]}>
      {pictures.map((picture, i) =>
        mode === "skia" ? (
          <Canvas key={i} style={{ width: size, height: size }}>
            <Picture picture={picture} />
          </Canvas>
        ) : (
          <NbvIcon
            key={i}
            picture={picture}
            size={size}
            onPresented={onPresented}
          />
        ),
      )}
    </View>
  );
};

// --- Frame statistics on the UI thread ----------------------------------------

// 0.25 ms buckets up to 100 ms.
const BUCKETS = 400;
interface FrameStats {
  frames: number;
  sum: number;
  max: number;
  hist: number[];
}
const emptyStats = (): FrameStats => ({
  frames: 0,
  sum: 0,
  max: 0,
  hist: new Array(BUCKETS + 1).fill(0),
});

const summarize = (stats: FrameStats) => {
  const percentile = (p: number) => {
    const target = stats.frames * p;
    let seen = 0;
    for (let b = 0; b <= BUCKETS; b++) {
      seen += stats.hist[b];
      if (seen >= target) {
        return b / 4;
      }
    }
    return stats.max;
  };
  const median = percentile(0.5);
  // Frames missed: a frame of k refresh intervals hides k - 1 frames.
  let missed = 0;
  if (median > 0) {
    for (let b = 0; b <= BUCKETS; b++) {
      const k = Math.round(b / 4 / median);
      if (k > 1) {
        missed += (k - 1) * stats.hist[b];
      }
    }
  }
  return {
    frames: stats.frames,
    seconds: Math.round(stats.sum) / 1000,
    medianMs: median,
    p95Ms: percentile(0.95),
    p99Ms: percentile(0.99),
    maxMs: Math.round(stats.max * 10) / 10,
    missed,
  };
};

const wait = (ms: number) => new Promise((resolve) => setTimeout(resolve, ms));

const report = async (body: object) => {
  if (!REPORT_URL) {
    return;
  }
  try {
    await fetch(REPORT_URL, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ platform: Platform.OS, ...body }),
    });
  } catch {
    // No collector: the results stay on screen.
  }
};

// --- Screen ------------------------------------------------------------------

export const IconBenchmark = () => {
  const { width, height } = useWindowDimensions();
  // Every icon on screen, below the buttons and the header.
  const size = Math.floor(
    Math.min((width - 16) / COLUMNS, (height - 260) / (ICONS / COLUMNS)),
  );
  const pictures = useMemo(
    () => Array.from({ length: ICONS }, (_, i) => recordIcon(i, size)),
    [size],
  );
  const gridPicture = useMemo(
    () => recordGrid(pictures, size),
    [pictures, size],
  );

  const [mode, setMode] = useState<Mode | null>(null);
  const [epoch, setEpoch] = useState(0);
  const [lines, setLines] = useState<string[]>([]);
  const [running, setRunning] = useState(false);

  const recording = useSharedValue(false);
  const stats = useSharedValue<FrameStats>(emptyStats());
  const offset = useSharedValue(0);
  useFrameCallback((info) => {
    if (!recording.value || info.timeSincePreviousFrame === null) {
      return;
    }
    const dt = info.timeSincePreviousFrame;
    stats.modify((s) => {
      "worklet";
      s.frames += 1;
      s.sum += dt;
      s.max = Math.max(s.max, dt);
      s.hist[Math.min(BUCKETS, Math.floor(dt * 4))] += 1;
      return s;
    }, false);
  });
  const animated = useAnimatedStyle(() => ({
    transform: [{ translateY: offset.value }],
  }));

  const timings = useRef({ start: 0, mounted: 0, presented: 0, count: 0 });
  const onMounted = useCallback(() => {
    timings.current.mounted = performance.now();
  }, []);
  const onPresented = useCallback(() => {
    const t = timings.current;
    t.count += 1;
    if (t.count === ICONS) {
      t.presented = performance.now();
    }
  }, []);

  const log = useCallback((line: string) => {
    console.log(`[IconBenchmark] ${line}`);
    setLines((previous) => [...previous, line]);
  }, []);

  const startStats = () => {
    stats.value = emptyStats();
    recording.value = true;
  };
  const stopStats = async () => {
    recording.value = false;
    await wait(50);
    return summarize(stats.value);
  };

  const runOne = async (m: Mode, run: number) => {
    setMode(null);
    await wait(1500);
    timings.current = { start: 0, mounted: 0, presented: 0, count: 0 };
    startStats();
    timings.current.start = performance.now();
    setEpoch((e) => e + 1);
    setMode(m);
    await wait(2500);
    const mount = await stopStats();
    const t = timings.current;
    const mountMs = Math.round(t.mounted - t.start);
    const presentedMs = t.presented ? Math.round(t.presented - t.start) : null;
    // The collector samples memory while the screen holds still.
    await report({ event: "hold", mode: m, run });
    await wait(500);
    await report({ event: "animStart", mode: m, run });
    startStats();
    offset.value = 0;
    offset.value = withRepeat(
      withTiming(-120, { duration: 1000, easing: Easing.inOut(Easing.sin) }),
      -1,
      true,
    );
    await wait(5000);
    cancelAnimation(offset);
    offset.value = 0;
    const anim = await stopStats();
    await report({ event: "animEnd", mode: m, run });
    const result = { mode: m, run, mountMs, presentedMs, mount, anim };
    await report({ event: "result", ...result });
    log(
      `${LABELS[m]} #${run}: mount ${mountMs} ms${
        presentedMs !== null ? `, presented ${presentedMs} ms` : ""
      }, mount max frame ${mount.maxMs} ms (${mount.missed} missed); ` +
        `anim median ${anim.medianMs} p99 ${anim.p99Ms} max ${anim.maxMs} ms, ` +
        `${anim.missed} missed / ${anim.frames}`,
    );
  };

  const runVisual = async () => {
    await wait(3000);
    for (const m of MODES) {
      setMode(m);
      await wait(1500);
    }
    await report({ event: "visualBegin", modes: MODES, runs: RUNS });
    for (let run = 1; run <= RUNS; run++) {
      for (const m of MODES) {
        setMode(null);
        await wait(1500);
        setEpoch((e) => e + 1);
        setMode(m);
        await wait(2000);
      }
    }
    setMode(null);
    await report({ event: "end" });
  };

  const runAll = async () => {
    setRunning(true);
    setLines([]);
    await report({ event: "begin" });
    // Warm-up: shaders and pipelines compile on first use in each mode.
    for (const m of MODES) {
      setMode(m);
      await wait(1500);
    }
    await runOne("empty", 0);
    for (let run = 1; run <= RUNS; run++) {
      for (const m of MODES) {
        await runOne(m, run);
      }
    }
    setMode(null);
    await report({ event: "end" });
    setRunning(false);
  };

  const autorun = useRef(false);
  useEffect(() => {
    if (!AUTORUN || autorun.current) {
      return;
    }
    autorun.current = true;
    // The collector picks what to run: { visual } for the screen-recording
    // pass, { memoryMode } to mount one mode for a memory sample, or nothing
    // for the benchmark.
    const config = async (): Promise<{
      visual?: boolean;
      memoryMode?: Mode;
    }> => {
      try {
        const response = await fetch(`${REPORT_URL}/config`);
        return await response.json();
      } catch {
        return {};
      }
    };
    config().then(({ visual, memoryMode }) => {
      if (visual) {
        runVisual();
      } else if (memoryMode) {
        setMode(memoryMode);
        wait(6000).then(() =>
          report({ event: "hold", mode: memoryMode, run: -1 }),
        );
      } else {
        runAll();
      }
    });
    // Once, on mount.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  return (
    <View style={styles.container}>
      <View style={styles.row}>
        <Pressable
          style={[styles.button, running && styles.disabled]}
          disabled={running}
          onPress={runAll}
        >
          <Text style={styles.buttonText}>Run benchmark</Text>
        </Pressable>
        {MODES.map((m) => (
          <Pressable
            key={m}
            style={[styles.button, mode === m && styles.selected]}
            disabled={running}
            onPress={() => {
              setEpoch((e) => e + 1);
              setMode(mode === m ? null : m);
            }}
          >
            <Text style={styles.buttonText}>{LABELS[m]}</Text>
          </Pressable>
        ))}
      </View>
      {mode && <View style={styles.marker} />}
      <Animated.View
        style={[styles.stage, animated]}
        onLayout={(event) => {
          const { x, y } = event.nativeEvent.layout;
          report({ event: "layout", x, y, size, padding: 8, columns: COLUMNS });
        }}
      >
        {mode && (
          <Grid
            key={`${mode}-${epoch}`}
            mode={mode}
            pictures={pictures}
            gridPicture={gridPicture}
            size={size}
            onMounted={onMounted}
            onPresented={onPresented}
          />
        )}
      </Animated.View>
      <View style={styles.log} pointerEvents="none">
        {lines.slice(-6).map((line, i) => (
          <Text key={i} style={styles.logText}>
            {line}
          </Text>
        ))}
      </View>
    </View>
  );
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: "white",
  },
  row: {
    flexDirection: "row",
    flexWrap: "wrap",
    padding: 4,
  },
  button: {
    paddingHorizontal: 10,
    paddingVertical: 6,
    margin: 3,
    borderRadius: 6,
    backgroundColor: "#ddd",
  },
  selected: {
    backgroundColor: "#9cf",
  },
  disabled: {
    opacity: 0.5,
  },
  buttonText: {
    color: "black",
    fontSize: 12,
  },
  stage: {
    flex: 1,
    paddingHorizontal: 8,
  },
  grid: {
    flexDirection: "row",
    flexWrap: "wrap",
  },
  marker: {
    position: "absolute",
    top: 0,
    right: 0,
    width: 24,
    height: 24,
    backgroundColor: "black",
  },
  log: {
    position: "absolute",
    left: 8,
    right: 8,
    bottom: 8,
    backgroundColor: "rgba(255, 255, 255, 0.85)",
  },
  logText: {
    color: "black",
    fontSize: 10,
  },
});
