import { useEffect, useState, useRef } from "react";
import { View, StyleSheet, Dimensions } from "react-native";
import { Canvas, useGPUContext, useDevice } from "react-native-wgpu";

interface VideoTextureProps {
  source: { uri: string } | number;
  style?: {
    width: number;
    height: number;
  };
}

interface VideoTextureState {
  pipeline: GPURenderPipeline;
  sampler: GPUSampler;
  texture: GPUTexture | null;
}

export const VideoTexture = ({ source, style }: VideoTextureProps) => {
  const [state, setState] = useState<VideoTextureState | null>(null);
  const { ref: canvasRef, context } = useGPUContext();
  const { device } = useDevice();
  const [videoLoaded, setVideoLoaded] = useState(false);
  const animationFrameRef = useRef<number>();

  // Prevent unused variable warning - in real implementation this would load video
  console.log({ source });

  const defaultStyle = {
    width: Dimensions.get("window").width,
    height: Dimensions.get("window").width * (9 / 16), // 16:9 aspect ratio
  };
  const finalStyle = { ...defaultStyle, ...style };

  // Initialize WebGPU pipeline
  useEffect(() => {
    if (!device || !context) {
      return;
    }

    const presentationFormat = navigator.gpu.getPreferredCanvasFormat();
    
    // Create shader module for video texture rendering
    const shader = device.createShaderModule({
      code: /* wgsl */ `
        struct VertexOutput {
          @builtin(position) position: vec4<f32>,
          @location(0) texCoord: vec2<f32>,
        }

        @vertex
        fn vertexMain(@builtin(vertex_index) vertexIndex: u32) -> VertexOutput {
          // Full-screen quad vertices
          let pos = array(
            vec2(-1.0, -1.0), // bottom-left
            vec2( 1.0, -1.0), // bottom-right
            vec2(-1.0,  1.0), // top-left
            vec2( 1.0,  1.0), // top-right
          );
          
          let texCoord = array(
            vec2(0.0, 1.0), // bottom-left
            vec2(1.0, 1.0), // bottom-right
            vec2(0.0, 0.0), // top-left
            vec2(1.0, 0.0), // top-right
          );

          var output: VertexOutput;
          output.position = vec4(pos[vertexIndex], 0.0, 1.0);
          output.texCoord = texCoord[vertexIndex];
          return output;
        }

        @group(0) @binding(0) var videoSampler: sampler;
        @group(0) @binding(1) var videoTexture: texture_2d<f32>;

        @fragment
        fn fragmentMain(input: VertexOutput) -> @location(0) vec4<f32> {
          return textureSample(videoTexture, videoSampler, input.texCoord);
        }
      `,
    });

    // Create render pipeline
    const pipeline = device.createRenderPipeline({
      layout: "auto",
      vertex: {
        module: shader,
        entryPoint: "vertexMain",
      },
      fragment: {
        module: shader,
        entryPoint: "fragmentMain",
        targets: [{ format: presentationFormat }],
      },
      primitive: {
        topology: "triangle-strip",
      },
    });

    // Create sampler
    const sampler = device.createSampler({
      magFilter: "linear",
      minFilter: "linear",
      addressModeU: "clamp-to-edge",
      addressModeV: "clamp-to-edge",
    });

    setState({ pipeline, sampler, texture: null });
  }, [device, context]);

  // Create texture from video frame
  const createVideoTexture = async () => {
    if (!device) {
      return null;
    }

    try {
      // In a real implementation, this would:
      // 1. Get current video frame from the native player
      // 2. Convert it to an ImageBitmap or similar format
      // 3. Create a texture and copy the image data
      
      // For now, create a placeholder texture
      const textureSize = 512;
      const texture = device.createTexture({
        size: [textureSize, textureSize],
        format: "rgba8unorm",
        usage: GPUTextureUsage.TEXTURE_BINDING | GPUTextureUsage.COPY_DST,
      });

      return texture;
    } catch (error) {
      console.warn("Failed to create video texture:", error);
      return null;
    }
  };

  // Animation loop for video texture updates
  const animate = async () => {
    if (!state || !context || !device) {
      return;
    }

    // Update video texture if needed
    const videoTexture = state.texture || await createVideoTexture();
    if (!videoTexture) {
      animationFrameRef.current = requestAnimationFrame(animate);
      return;
    }

    // Create bind group
    const bindGroup = device.createBindGroup({
      layout: state.pipeline.getBindGroupLayout(0),
      entries: [
        { binding: 0, resource: state.sampler },
        { binding: 1, resource: videoTexture.createView() },
      ],
    });

    // Render
    const commandEncoder = device.createCommandEncoder();
    const textureView = context.getCurrentTexture().createView();

    const renderPass = commandEncoder.beginRenderPass({
      colorAttachments: [
        {
          view: textureView,
          clearValue: { r: 0.0, g: 0.0, b: 0.0, a: 1.0 },
          loadOp: "clear",
          storeOp: "store",
        },
      ],
    });

    renderPass.setPipeline(state.pipeline);
    renderPass.setBindGroup(0, bindGroup);
    renderPass.draw(4); // Draw full-screen quad
    renderPass.end();

    device.queue.submit([commandEncoder.finish()]);
    context.present();

    // Update state with current texture
    if (!state.texture) {
      setState(prev => prev ? { ...prev, texture: videoTexture } : null);
    }

    animationFrameRef.current = requestAnimationFrame(animate);
  };

  // Start animation loop when video is loaded and pipeline is ready
  useEffect(() => {
    if (state) {
      // Simulate video loading after a short delay
      setTimeout(() => {
        setVideoLoaded(true);
      }, 100);
    }
  }, [state]);

  useEffect(() => {
    if (videoLoaded && state) {
      animate();
    }

    return () => {
      if (animationFrameRef.current) {
        cancelAnimationFrame(animationFrameRef.current);
      }
    };
  }, [videoLoaded, state]);

  return (
    <View style={[styles.container, finalStyle]}>
      <Canvas ref={canvasRef} style={styles.canvas} />
      {!videoLoaded && (
        <View style={styles.loadingOverlay}>
          {/* You could add a loading indicator here */}
        </View>
      )}
    </View>
  );
};

const styles = StyleSheet.create({
  container: {
    position: "relative",
  },
  canvas: {
    width: "100%",
    height: "100%",
  },
  loadingOverlay: {
    position: "absolute",
    top: 0,
    left: 0,
    right: 0,
    bottom: 0,
    backgroundColor: "rgba(0,0,0,0.5)",
    justifyContent: "center",
    alignItems: "center",
  },
});