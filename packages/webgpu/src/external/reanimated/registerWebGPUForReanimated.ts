// Declare global WebGPU worklet helper functions (installed by native module
// on the main JS runtime only — secondary worklet runtimes like Vision
// Camera's frame-processor thread do NOT have these on their globalThis).
declare function __webgpuBox(obj: object): {
  unbox: () => object;
  __boxedWebGPU: true;
};

console.log("[WebGPU] registerWebGPUForReanimated module loaded");

let isRegistered = false;

/**
 * Register WebGPU objects for Worklets serialization.
 * This allows GPUDevice, GPUCanvasContext, etc. to be passed to worklets.
 *
 * This is called automatically when the module loads if react-native-worklets is installed.
 */
export const registerWebGPUForReanimated = () => {
  if (isRegistered) {
    return;
  }
  isRegistered = true;

  try {
    const { registerCustomSerializable } = require("react-native-worklets");

    console.log(
      "[WebGPU] registering custom serializer (v2: __brand-getter check)",
    );

    registerCustomSerializable({
      name: "WebGPU",
      // `determine` is invoked by Worklets in *both* runtimes during the
      // serialization handshake, so its body must not rely on any globals
      // beyond the JS built-ins. We inline the WebGPU-object check (native
      // state + Symbol.toStringTag on prototype, same as the native
      // __webgpuIsWebGPUObject helper) here so the function works on the
      // main runtime and on every worklet runtime — including ones we don't
      // own (Vision Camera, etc.).
      determine: (value: object): value is object => {
        "worklet";
        if (value == null || typeof value !== "object") {
          return false;
        }
        if ((value as { __boxedWebGPU?: boolean }).__boxedWebGPU === true) {
          console.log("[WebGPU determine] matched boxed object");
          return true;
        }
        const proto = Object.getPrototypeOf(value);
        if (proto == null) {
          return false;
        }
        const desc = Object.getOwnPropertyDescriptor(proto, "__brand");
        const matched = desc != null && typeof desc.get === "function";
        // Skip plain JS objects — they're handled by Worklets natively and
        // would spam the log with every captured plain object.
        if (proto !== Object.prototype) {
          let brand: string | undefined;
          try {
            brand = desc?.get?.call(value) as string | undefined;
          } catch {
            brand = "<threw>";
          }

          console.log(
            "[WebGPU determine] matched=" +
              String(matched) +
              " brand=" +
              String(brand) +
              " protoCtor=" +
              String((proto.constructor as { name?: string })?.name),
          );
        }
        return matched;
      },
      // `pack` runs on the source runtime, which is always the main JS
      // runtime in our setup (you can't create raw WebGPU objects inside a
      // worklet), so __webgpuBox is available here.
      pack: (value: object) => {
        "worklet";
        return __webgpuBox(value);
      },
      // `unpack` runs on the destination runtime and just calls the
      // BoxedWebGPUObject's own `unbox` method — no globals needed.
      unpack: (boxed: { unbox: () => object }) => {
        "worklet";
        return boxed.unbox();
      },
    });

    console.log("[WebGPU] registerCustomSerializable call returned OK");
  } catch (e) {
    console.warn("[WebGPU] registerCustomSerializable threw: " + String(e));
  }
};
