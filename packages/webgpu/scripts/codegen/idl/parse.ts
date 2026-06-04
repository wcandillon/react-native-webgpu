import { existsSync, readFileSync } from "fs";
import * as path from "path";

import { parse } from "webidl2";
import type * as webidl2 from "webidl2";

// The IDL ground truth lives in the Dawn submodule. We consume the exact same
// inputs Dawn's own `src/dawn/node` bindings are generated from:
//   - third_party/gpuweb/webgpu.idl           (the W3C standard surface)
//   - src/dawn/node/interop/DawnExtensions.idl (Dawn-specific extensions)
// Browser.idl (host-environment shims: EventTarget, ArrayBuffer, …) is not a
// source of GPU types, so we don't parse it; the handful of shim types it
// declares are handled as built-ins in resolveType.
const DAWN_ROOT = path.resolve(__dirname, "../../../../../externals/dawn");

export const IDL_FILES = {
  webgpu: path.join(DAWN_ROOT, "third_party/gpuweb/webgpu.idl"),
  extensions: path.join(DAWN_ROOT, "src/dawn/node/interop/DawnExtensions.idl"),
};

const assertExists = (file: string) => {
  if (!existsSync(file)) {
    throw new Error(
      `Cannot find IDL file:\n  ${file}\n\n` +
        "The codegen reads the WebGPU IDL ground truth from the Dawn submodule. " +
        "Initialize it (recursively, to pull third_party/gpuweb) before running codegen:\n\n" +
        "  git submodule update --init --recursive externals/dawn\n",
    );
  }
};

// Dawn's idlgen uses a lenient Go WebIDL parser; DawnExtensions.idl relies on
// that leniency in a couple of ways that the stricter webidl2 rejects:
//   - it omits the trailing `;` after some `}` block terminators;
//   - it gives a non-`optional` nullable argument an empty default `= {}`.
// We normalise both before parsing without altering any generated semantics.
const sanitize = (source: string): string =>
  source.replace(/^(\s*)\}\s*$/gm, "$1};").replace(/\s*=\s*\{\}/g, "");

export const parseIDL = (): webidl2.IDLRootType[] => {
  const roots: webidl2.IDLRootType[] = [];
  for (const file of [IDL_FILES.webgpu, IDL_FILES.extensions]) {
    assertExists(file);
    const source = sanitize(readFileSync(file, "utf8"));
    roots.push(...parse(source));
  }
  return roots;
};
