import React, { useEffect, useState } from "react";
import { View, StyleSheet, Text, TouchableOpacity } from "react-native";
import { Canvas, useGPUContext, useDevice } from "react-native-wgpu";

/**
 * SimpleVideoExample - A minimal example showing video texture basics
 * 
 * This demonstrates:
 * - Creating a video-like texture with animated content
 * - Applying a simple shader effect
 * - Basic texture management
 */
export const SimpleVideoExample = () => {
  const { ref, context } = useGPUContext();
  const { device } = useDevice();
  const [isPlaying, setIsPlaying] = useState(false);
  const [pipeline, setPipeline] = useState<GPURenderPipeline | null>(null);
  const [texture, setTexture] = useState<GPUTexture | null>(null);

  // Initialize WebGPU pipeline
  useEffect(() => {
    if (!device || !context) return;

    const shader = device.createShaderModule({
      code: /* wgsl */ `
        @vertex
        fn vs(@builtin(vertex_index) vertexIndex: u32) -> @builtin(position) vec4<f32> {
          let pos = array(
            vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(-1.0, 1.0),
            vec2(-1.0, 1.0), vec2(1.0, -1.0), vec2(1.0, 1.0)
          );
          return vec4(pos[vertexIndex], 0.0, 1.0);
        }

        @group(0) @binding(0) var videoTexture: texture_2d<f32>;
        @group(0) @binding(1) var videoSampler: sampler;

        @fragment
        fn fs(@builtin(position) pos: vec4<f32>) -> @location(0) vec4<f32> {
          let uv = pos.xy / 512.0; // Assuming 512x512 canvas
          let color = textureSample(videoTexture, videoSampler, uv);
          
          // Add a simple effect - color tint
          return vec4(color.rgb * vec3(1.2, 0.8, 1.0), color.a);
        }
      `,
    });

    const newPipeline = device.createRenderPipeline({
      layout: "auto",
      vertex: { module: shader, entryPoint: "vs" },
      fragment: {
        module: shader,
        entryPoint: "fs",
        targets: [{ format: navigator.gpu.getPreferredCanvasFormat() }],
      },
    });

    setPipeline(newPipeline);
  }, [device, context]);

  // Create/update video texture
  const updateVideoTexture = () => {
    if (!device) return;

    const size = 256;
    const data = new Uint8Array(size * size * 4);
    const time = Date.now() * 0.001;

    // Generate animated "video" content
    for (let y = 0; y < size; y++) {
      for (let x = 0; x < size; x++) {
        const i = (y * size + x) * 4;
        const u = x / size;
        const v = y / size;
        
        // Create moving patterns
        const wave1 = Math.sin(u * 8 + time * 2) * 0.5 + 0.5;
        const wave2 = Math.sin(v * 6 + time * 1.5) * 0.5 + 0.5;
        const radial = Math.sin(Math.sqrt((u-0.5)**2 + (v-0.5)**2) * 10 - time * 3) * 0.5 + 0.5;
        
        data[i] = wave1 * 255;     // R
        data[i+1] = wave2 * 255;   // G  
        data[i+2] = radial * 255;  // B
        data[i+3] = 255;           // A
      }
    }

    // Create or update texture
    if (!texture) {
      const newTexture = device.createTexture({
        size: [size, size],
        format: "rgba8unorm",
        usage: GPUTextureUsage.TEXTURE_BINDING | GPUTextureUsage.COPY_DST,
      });
      setTexture(newTexture);
      
      device.queue.writeTexture(
        { texture: newTexture },
        data,
        { bytesPerRow: size * 4 },
        [size, size]
      );
    } else {
      device.queue.writeTexture(
        { texture },
        data,
        { bytesPerRow: size * 4 },
        [size, size]
      );
    }
  };

  // Render loop
  useEffect(() => {
    if (!isPlaying || !pipeline || !device || !context) return;

    let animationId: number;
    
    const render = () => {
      updateVideoTexture();
      
      if (!texture) {
        animationId = requestAnimationFrame(render);
        return;
      }

      const sampler = device.createSampler({
        magFilter: "linear",
        minFilter: "linear",
      });

      const bindGroup = device.createBindGroup({
        layout: pipeline.getBindGroupLayout(0),
        entries: [
          { binding: 0, resource: texture.createView() },
          { binding: 1, resource: sampler },
        ],
      });

      const encoder = device.createCommandEncoder();
      const pass = encoder.beginRenderPass({
        colorAttachments: [{
          view: context.getCurrentTexture().createView(),
          clearValue: [0, 0, 0, 1],
          loadOp: "clear",
          storeOp: "store",
        }],
      });

      pass.setPipeline(pipeline);
      pass.setBindGroup(0, bindGroup);
      pass.draw(6);
      pass.end();

      device.queue.submit([encoder.finish()]);
      context.present();

      animationId = requestAnimationFrame(render);
    };

    render();

    return () => {
      if (animationId) {
        cancelAnimationFrame(animationId);
      }
    };
  }, [isPlaying, pipeline, device, context, texture]);

  return (
    <View style={styles.container}>
      <Text style={styles.title}>Simple Video Texture Example</Text>
      
      <Canvas ref={ref} style={styles.canvas} />
      
      <TouchableOpacity
        style={[styles.button, isPlaying && styles.buttonActive]}
        onPress={() => setIsPlaying(!isPlaying)}
      >
        <Text style={styles.buttonText}>
          {isPlaying ? "Pause" : "Play"} Video
        </Text>
      </TouchableOpacity>
      
      <Text style={styles.description}>
        This example creates an animated texture that simulates video content
        and applies a color tint effect using WebGPU shaders.
      </Text>
    </View>
  );
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
    padding: 20,
    backgroundColor: "#f5f5f5",
  },
  title: {
    fontSize: 20,
    fontWeight: "bold",
    textAlign: "center",
    marginBottom: 20,
  },
  canvas: {
    width: "100%",
    height: 300,
    backgroundColor: "#000",
    borderRadius: 8,
    marginBottom: 20,
  },
  button: {
    backgroundColor: "#007AFF",
    padding: 15,
    borderRadius: 8,
    alignItems: "center",
    marginBottom: 15,
  },
  buttonActive: {
    backgroundColor: "#FF6B6B",
  },
  buttonText: {
    color: "white",
    fontSize: 16,
    fontWeight: "bold",
  },
  description: {
    fontSize: 14,
    color: "#666",
    textAlign: "center",
    lineHeight: 20,
  },
});