import { Stack } from "expo-router/stack";

export { ErrorBoundary } from "expo-router";

export default function Layout() {
  return (
    <Stack>
      <Stack.Screen name="index" options={{ title: "WebGPU examples" }} />
      <Stack.Screen name="raw" options={{ title: "WebGPU triangle" }} />
      <Stack.Screen name="three" options={{ title: "Three.js" }} />
      <Stack.Screen name="fiber" options={{ title: "React Three Fiber" }} />
    </Stack>
  );
}
