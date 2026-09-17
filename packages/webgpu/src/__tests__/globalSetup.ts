import fs from "fs";
import http from "http";
import path from "path";
import { execFileSync } from "child_process";

import { WebSocketServer } from "ws";

import { NODE_WEBGPU, REFERENCE, TEST_SERVER_PORT } from "./config";

const isOS = (os: string): os is "android" | "ios" | "web" => {
  return ["ios", "android", "web"].indexOf(os) !== -1;
};

const isArch = (arc: string): arc is "paper" | "fabric" => {
  return ["paper", "fabric"].indexOf(arc) !== -1;
};

// Static file server for the fixtures in ./assets, sharing the port with the
// WebSocket endpoint. Device tests that need a Blob load their PNGs from here:
// React Native's Android networking stack (OkHttp) cannot fetch data: URIs, so
// `fetch("data:image/png;base64,...")` rejects with "Network request failed"
// there even though it works on iOS (NSURLSession handles data: natively).
const assetsDir = path.resolve(__dirname, "assets");
const createFixtureServer = () =>
  http.createServer((req, res) => {
    const name = path.basename(
      decodeURIComponent((req.url ?? "").split("?")[0]),
    );
    const file = path.join(assetsDir, name);
    if (!file.startsWith(assetsDir + path.sep) || !fs.existsSync(file)) {
      res.writeHead(404).end();
      return;
    }
    res.writeHead(200, {
      "Content-Type": "image/png",
      "Cache-Control": "no-store",
      "Access-Control-Allow-Origin": "*",
    });
    res.end(fs.readFileSync(file));
  });

// Map the device's own localhost:4242 onto this machine, the same way the React
// Native CLI does for Metro on 8081. With it, an Android emulator and a physical
// device both reach the test server (WebSocket and fixtures alike) at
// "localhost", so useClient needs no per-platform host and no LAN address.
// Best effort: no adb, no device, or several devices attached just leaves the
// connection to whatever host the app is configured with.
const reversePort = (port: number) => {
  try {
    execFileSync("adb", ["reverse", `tcp:${port}`, `tcp:${port}`], {
      stdio: "ignore",
    });
  } catch {
    // Not an Android run (or adb is unavailable); iOS and the emulator default
    // work without it.
  }
};

const globalSetup = () => {
  return new Promise<void>((resolve) => {
    // The reference (Chrome) and node (dawn.node) clients run in-process, so
    // no device connection is needed.
    if (REFERENCE || NODE_WEBGPU) {
      resolve();
      return;
    }
    const port = TEST_SERVER_PORT;
    reversePort(port);
    global.testFixtureServer = createFixtureServer();
    global.testServer = new WebSocketServer({
      server: global.testFixtureServer,
    });
    global.testFixtureServer.listen(port);
    console.log(
      `\n\nTest server listening on port ${port} (waiting for the example app to open on E2E tests screen)`,
    );
    global.testServer.on("connection", (client) => {
      global.testClient = client;
      client.once("message", (msg) => {
        const obj = JSON.parse(msg.toString("utf8"));
        const { OS, arch, host } = obj;
        if (!isOS(OS)) {
          throw new Error("Unknown testing platform: " + OS);
        }
        if (!isArch(arch)) {
          throw new Error("Unknown testing architecture: " + arch);
        }
        global.testOS = OS;
        global.testArch = arch;
        // The host the device reached us on (localhost for the simulator, a LAN
        // address for a physical device); fixture URLs are built from it.
        global.testHost = typeof host === "string" ? host : "localhost";
        console.log(`${OS} device connected (${arch}) from ${global.testHost}`);
        resolve();
      });
    });
  });
};

// eslint-disable-next-line import/no-default-export
export default globalSetup;
