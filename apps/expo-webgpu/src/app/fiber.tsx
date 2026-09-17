import React, { Suspense } from "react";
import { ActivityIndicator } from "react-native";

const Fiber = React.lazy(() => import("@/components/fiber"));

export default function ReactThreeFiber() {
  return (
    <Suspense fallback={<ActivityIndicator animating />}>
      <Fiber />
    </Suspense>
  );
}
