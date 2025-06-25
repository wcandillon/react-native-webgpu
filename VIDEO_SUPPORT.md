# Video Support in React Native WebGPU

React Native WebGPU provides comprehensive support for using video as textures in your WebGPU applications. This enables powerful video processing, real-time effects, and seamless integration with 3D scenes.

## Quick Start

```tsx
import { VideoFrameTexture } from './components/VideoFrameTexture';

<VideoFrameTexture
  source={{ uri: "https://example.com/video.mp4" }}
  style={{ width: 300, height: 200 }}
  loop={true}
  autoPlay={true}
  onVideoReady={() => console.log("Video ready")}
/>
```

## Components

### VideoFrameTexture

The `VideoFrameTexture` component provides a complete solution for rendering video through WebGPU with real-time effects.

```tsx
interface VideoFrameTextureProps {
  source: { uri: string } | number;
  style?: { width: number; height: number };
  loop?: boolean;
  autoPlay?: boolean;
  onVideoReady?: () => void;
  onError?: (error: any) => void;
}
```

**Features:**
- Automatic video texture management
- Real-time shader effects
- GPU-optimized rendering
- Customizable video processing pipeline

### VideoTexture

Alternative video texture implementation for different use cases.

## Integration with Video Libraries

### React Native Video

For production use with actual video playback, integrate with `react-native-video`:

```bash
npm install react-native-video
```

```tsx
import Video, { VideoRef } from 'react-native-video';

const VideoWebGPUExample = () => {
  const videoRef = useRef<VideoRef>(null);
  const { device } = useDevice();
  
  const extractVideoFrame = async () => {
    // Platform-specific video frame extraction
    // This would need native implementation
  };
  
  const createVideoTexture = async (videoFrame: ImageData) => {
    const imageBitmap = await createImageBitmap(videoFrame);
    
    const texture = device.createTexture({
      size: [imageBitmap.width, imageBitmap.height, 1],
      format: "rgba8unorm",
      usage: GPUTextureUsage.TEXTURE_BINDING | GPUTextureUsage.COPY_DST,
    });
    
    device.queue.copyExternalImageToTexture(
      { source: imageBitmap },
      { texture },
      [imageBitmap.width, imageBitmap.height]
    );
    
    return texture;
  };
  
  return (
    <View>
      <Video ref={videoRef} source={{ uri: "video.mp4" }} />
      <Canvas ref={canvasRef} />
    </View>
  );
};
```

## Video Processing Shaders

### Basic Video Rendering

```wgsl
@group(0) @binding(0) var videoSampler: sampler;
@group(0) @binding(1) var videoTexture: texture_2d<f32>;

@fragment
fn fragmentMain(input: VertexOutput) -> @location(0) vec4<f32> {
  return textureSample(videoTexture, videoSampler, input.texCoord);
}
```

### Video Effects

#### Grayscale Conversion

```wgsl
@fragment
fn grayscaleMain(input: VertexOutput) -> @location(0) vec4<f32> {
  let color = textureSample(videoTexture, videoSampler, input.texCoord);
  let gray = dot(color.rgb, vec3(0.299, 0.587, 0.114));
  return vec4(vec3(gray), color.a);
}
```

#### Color Adjustment

```wgsl
@fragment
fn colorAdjustMain(input: VertexOutput) -> @location(0) vec4<f32> {
  let color = textureSample(videoTexture, videoSampler, input.texCoord);
  
  // Brightness and contrast
  let brightness = 0.1;
  let contrast = 1.2;
  
  let adjusted = (color.rgb - 0.5) * contrast + 0.5 + brightness;
  return vec4(clamp(adjusted, vec3(0.0), vec3(1.0)), color.a);
}
```

#### Film Grain Effect

```wgsl
@fragment
fn filmGrainMain(input: VertexOutput) -> @location(0) vec4<f32> {
  let color = textureSample(videoTexture, videoSampler, input.texCoord);
  
  // Generate pseudo-random grain
  let grain = sin(input.texCoord.x * 1000.0) * sin(input.texCoord.y * 1000.0) * 0.02;
  
  return vec4(color.rgb + grain, color.a);
}
```

## Advanced Usage

### Multi-Texture Video Processing

```tsx
const useMultiVideoTextures = (videoSources: string[]) => {
  const { device } = useDevice();
  const [textures, setTextures] = useState<GPUTexture[]>([]);
  
  useEffect(() => {
    const loadVideos = async () => {
      const loadedTextures = await Promise.all(
        videoSources.map(async (source) => {
          // Load video and create texture
          const videoTexture = await createVideoTexture(source);
          return videoTexture;
        })
      );
      setTextures(loadedTextures);
    };
    
    loadVideos();
  }, [videoSources, device]);
  
  return textures;
};
```

### Video Texture in 3D Scenes

```tsx
// Use video texture on 3D geometry
const VideoMesh = ({ videoTexture }: { videoTexture: GPUTexture }) => {
  const { device } = useDevice();
  
  const bindGroup = device.createBindGroup({
    layout: pipeline.getBindGroupLayout(0),
    entries: [
      { binding: 0, resource: sampler },
      { binding: 1, resource: videoTexture.createView() },
    ],
  });
  
  // Render 3D mesh with video texture
  // ...
};
```

### Real-time Video Analysis

```tsx
const VideoAnalyzer = ({ videoTexture }: { videoTexture: GPUTexture }) => {
  const { device } = useDevice();
  
  // Compute shader for video analysis
  const computeShader = device.createShaderModule({
    code: /* wgsl */ `
      @group(0) @binding(0) var inputTexture: texture_2d<f32>;
      @group(0) @binding(1) var<storage, read_write> analysisData: array<f32>;
      
      @compute @workgroup_size(8, 8)
      fn analyze(@builtin(global_invocation_id) id: vec3<u32>) {
        let pixel = textureLoad(inputTexture, vec2<i32>(id.xy), 0);
        let luminance = dot(pixel.rgb, vec3(0.299, 0.587, 0.114));
        
        // Store analysis data
        let index = id.y * 256u + id.x;
        analysisData[index] = luminance;
      }
    `,
  });
  
  // Run analysis on video frames
  // ...
};
```

## Performance Optimization

### Texture Updates

```tsx
const optimizeVideoTextureUpdates = (
  device: GPUDevice,
  texture: GPUTexture,
  videoElement: HTMLVideoElement
) => {
  // Only update when video has new frame
  if (videoElement.currentTime !== lastUpdateTime) {
    // Update texture with new frame
    device.queue.copyExternalImageToTexture(
      { source: videoElement },
      { texture },
      [videoElement.videoWidth, videoElement.videoHeight]
    );
    lastUpdateTime = videoElement.currentTime;
  }
};
```

### Memory Management

```tsx
const VideoTexturePool = {
  textures: new Map<string, GPUTexture>(),
  
  getTexture(id: string, size: [number, number], device: GPUDevice) {
    if (!this.textures.has(id)) {
      const texture = device.createTexture({
        size: [...size, 1],
        format: "rgba8unorm",
        usage: GPUTextureUsage.TEXTURE_BINDING | GPUTextureUsage.COPY_DST,
      });
      this.textures.set(id, texture);
    }
    return this.textures.get(id)!;
  },
  
  cleanup() {
    this.textures.forEach(texture => texture.destroy());
    this.textures.clear();
  }
};
```

## Platform Considerations

### iOS

```tsx
// iOS-specific video frame extraction
const extractVideoFrameIOS = async (videoRef: VideoRef) => {
  // Use AVPlayer's video output for frame extraction
  // Requires native iOS implementation
};
```

### Android

```tsx
// Android-specific video frame extraction  
const extractVideoFrameAndroid = async (videoRef: VideoRef) => {
  // Use MediaPlayer or ExoPlayer for frame extraction
  // Requires native Android implementation
};
```

## Testing

The library includes comprehensive tests for video functionality:

```bash
# Run video-specific tests
npm test -- --testNamePattern="Video Support"
```

Test coverage includes:
- Video texture pipeline creation
- Texture update cycles
- Memory management
- Error handling
- Performance benchmarks

## Examples

See the example app for complete demonstrations:

1. **Basic Video Texture** - Simple video rendering
2. **Video with Effects** - Real-time shader effects on video
3. **Multi-Video Processing** - Multiple video sources
4. **3D Video Integration** - Video textures in 3D scenes
5. **Video Analysis** - Real-time video analysis with compute shaders

## Limitations and Future Work

### Current Limitations

- Video frame extraction requires platform-specific native implementation
- Limited to formats supported by `createImageBitmap`
- Performance depends on video resolution and update frequency

### Planned Features

- Native video frame extraction for iOS and Android
- Hardware-accelerated video decoding integration
- Advanced video processing effects library
- Video streaming support
- Multi-layer video compositing

## Contributing

When contributing video-related features:

1. Ensure cross-platform compatibility
2. Add comprehensive tests
3. Document performance characteristics
4. Include example usage
5. Consider memory management implications

## Support

For video-specific issues:

1. Check the example app implementations
2. Review the test cases for expected behavior
3. Consult the WebGPU specification for texture limitations
4. Consider platform-specific video constraints

---

*This documentation covers the current video support implementation and provides guidance for extending video functionality in React Native WebGPU applications.*