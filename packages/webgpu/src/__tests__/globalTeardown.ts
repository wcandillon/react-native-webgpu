import { execFileSync } from "child_process";

import { NODE_WEBGPU, REFERENCE, TEST_SERVER_PORT } from "./config";

// Left on the device, the mapping keeps a retrying app reaching this machine.
const removeReversedPort = (port: number) => {
  try {
    execFileSync("adb", ["reverse", "--remove", `tcp:${port}`], {
      stdio: "ignore",
    });
  } catch {
    // Not an Android run, or adb is unavailable.
  }
};

const globalTeardown = () => {
  if (REFERENCE || NODE_WEBGPU) {
    return;
  }
  global.testServer.clients.forEach((client) => client.close());
  global.testServer.close();
  global.testFixtureServer.close();
  removeReversedPort(TEST_SERVER_PORT);
};

// eslint-disable-next-line import/no-default-export
export default globalTeardown;
