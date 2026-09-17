import { useEffect, useState } from "react";
import { Platform } from "react-native";

// Both platforms reach the test server on localhost: the iOS Simulator shares
// the host network, and on Android the jest globalSetup runs
// "adb reverse tcp:4242 tcp:4242" (as the RN CLI does for Metro on 8081), which
// covers the emulator and a physical device alike. Only a device that cannot be
// reached over adb needs this machine's LAN IP here instead.
const HOST = "localhost";
const PORT = 4242;

type UseClient = [client: WebSocket | null, hostname: string];
export const useClient = (): UseClient => {
  const [client, setClient] = useState<WebSocket | null>(null);
  const [retry, setRetry] = useState<number>(0);

  useEffect(() => {
    const url = `ws://${HOST}:${PORT}`;
    let it: ReturnType<typeof setTimeout>;
    let disposed = false;
    const ws = new WebSocket(url);
    const scheduleRetry = () => {
      if (disposed) {
        return;
      }
      it = setTimeout(() => {
        // incrementing retry to rerun the effect
        setRetry((r) => r + 1);
      }, 500);
    };
    ws.onopen = () => {
      setClient(ws);
      // The host is reported back so the test server can hand out fixture URLs
      // this device can actually reach (see fixtureUrl in setup.ts).
      ws.send(JSON.stringify({ OS: Platform.OS, arch: "paper", host: HOST }));
    };
    // Reconnect on every close, not only on error: the test server closes the
    // socket cleanly at the end of each jest run, and without a retry here the
    // app would need a manual reload before the next run.
    ws.onclose = () => {
      setClient(null);
      scheduleRetry();
    };
    ws.onerror = () => {
      // Triggers onclose, which schedules the retry.
      ws.close();
    };
    return () => {
      disposed = true;
      clearTimeout(it);
      ws.close();
    };
  }, [retry]);
  return [client, HOST];
};
