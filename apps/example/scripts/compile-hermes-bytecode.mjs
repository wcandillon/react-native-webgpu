#!/usr/bin/env node
// Compile a JavaScript bundle to Hermes bytecode, as React Native's Gradle plugin does for a
// release build: react-native-test-app packages the bundle it is given as it is.
//
// Usage:
//   node scripts/compile-hermes-bytecode.mjs <bundle.js> <output>
//
// The compiler is the one the installed react-native compiles with: its hermes-compiler
// dependency, else the hermesc in its sdks/hermesc. Another package's hermes-compiler can be
// installed beside it, and its bytecode would not load.

import { execFileSync } from "node:child_process";
import { createRequire } from "node:module";
import { dirname, join } from "node:path";

const require = createRequire(import.meta.url);

const [input, output] = process.argv.slice(2);
if (!input || !output) {
  throw new Error(
    "Usage: node scripts/compile-hermes-bytecode.mjs <bundle.js> <output>"
  );
}

const OS_BIN = { darwin: "osx-bin", linux: "linux64-bin", win32: "win64-bin" };
const osBin = OS_BIN[process.platform];
if (!osBin) {
  throw new Error(`hermesc is not built for ${process.platform}`);
}

const reactNativePackage = require.resolve("react-native/package.json");
const reactNativeRoot = dirname(reactNativePackage);
const { dependencies } = require(reactNativePackage);
const resolveFromReactNative = (request) =>
  require.resolve(request, { paths: [reactNativeRoot] });
const hermescRoot =
  "hermes-compiler" in dependencies
    ? join(dirname(resolveFromReactNative("hermes-compiler")), "hermesc")
    : join(reactNativeRoot, "sdks", "hermesc");
const executable = process.platform === "win32" ? "hermesc.exe" : "hermesc";
const hermesc = join(hermescRoot, osBin, executable);

execFileSync(hermesc, ["-w", "-emit-binary", "-O", "-out", output, input], {
  stdio: "inherit",
  windowsHide: true,
});
