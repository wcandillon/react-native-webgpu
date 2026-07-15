import { useEffect, useState } from "react";
import { Platform } from "react-native";

const ANDROID_WS_HOST = "10.0.2.2";
const IOS_WS_HOST = "localhost";
const HOST = Platform.OS === "android" ? ANDROID_WS_HOST : IOS_WS_HOST;
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
      ws.send(JSON.stringify({ OS: Platform.OS, arch: "paper" }));
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
