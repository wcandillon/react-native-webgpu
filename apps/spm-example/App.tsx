/**
 * Minimal app used to validate react-native-webgpu linked via Swift Package
 * Manager (see packages/webgpu/Package.swift) instead of CocoaPods on iOS.
 * Renders a plain animated clear color: enough to exercise adapter/device
 * request, context configuration, command submission, and present() end to
 * end without pulling in the full example app's dependencies.
 *
 * @format
 */

import { useEffect, useRef } from 'react';
import { StyleSheet, View } from 'react-native';
import { Canvas, type CanvasRef } from 'react-native-webgpu';

function Scene() {
  const ref = useRef<CanvasRef>(null);

  useEffect(() => {
    let stopped = false;
    let frame: number;

    const start = async () => {
      const context = ref.current?.getContext('webgpu');
      if (!context) {
        frame = requestAnimationFrame(start);
        return;
      }
      const adapter = await navigator.gpu.requestAdapter();
      const device = await adapter?.requestDevice();
      if (!device) {
        return;
      }
      const format = navigator.gpu.getPreferredCanvasFormat();
      context.configure({ device, format, alphaMode: 'opaque' });

      const render = (t: number) => {
        if (stopped) {
          return;
        }
        const hue = (t / 1000) % 1;
        const encoder = device.createCommandEncoder();
        const pass = encoder.beginRenderPass({
          colorAttachments: [
            {
              view: context.getCurrentTexture().createView(),
              loadOp: 'clear',
              storeOp: 'store',
              clearValue: { r: hue, g: 1 - hue, b: 0.5, a: 1 },
            },
          ],
        });
        pass.end();
        device.queue.submit([encoder.finish()]);
        context.present();
        frame = requestAnimationFrame(render);
      };
      frame = requestAnimationFrame(render);
    };

    frame = requestAnimationFrame(start);
    return () => {
      stopped = true;
      cancelAnimationFrame(frame);
    };
  }, []);

  return <Canvas ref={ref} style={StyleSheet.absoluteFill} />;
}

function App() {
  return (
    <View style={styles.container}>
      <Scene />
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
  },
});

export default App;
