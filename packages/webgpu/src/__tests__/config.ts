export const DEBUG = process.env.DEBUG === "true";
export const REFERENCE = process.env.REFERENCE === "true";
export const NODE_WEBGPU = process.env.NODE_WEBGPU === "true";
// Serves both the WebSocket endpoint the example app connects to and the static
// fixtures under ./assets. Kept in sync with PORT in apps/example/src/useClient.
export const TEST_SERVER_PORT = 4242;
