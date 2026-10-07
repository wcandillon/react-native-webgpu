export const DEBUG = process.env.DEBUG === "true";
export const REFERENCE = process.env.REFERENCE === "true";
export const NODE_WEBGPU = process.env.NODE_WEBGPU === "true";
// Serves both the WebSocket endpoint the example app connects to and the static
// fixtures under ./assets. Kept in sync with PORT in apps/example/src/useClient:
// both read E2E_PORT (inlined into the app bundle by Metro, read at run time
// here) and fall back to 4242, so CI can move the server off the port other
// projects on the same machine use.
export const TEST_SERVER_PORT = Number(process.env.E2E_PORT ?? 4242);
