#include "GPU.h"
#include <utility>
#include <webgpu/webgpu_cpp.h>
#include <android/native_window_jni.h>
#include <android/surface_texture.h>
#include <android/surface_texture_jni.h>

using namespace wgpu;

#include "Convertors.h"

namespace rnwgpu {

std::future<std::shared_ptr<GPUAdapter>>
GPU::requestAdapter(std::shared_ptr<GPURequestAdapterOptions> options) {
  return std::async(std::launch::async, [this, options]() {
    wgpu::RequestAdapterOptions aOptions;
    // TODO: enable, make std::shared_ptr<GPURequestAdapterOptions>  optional
    // Convertor conv;
    // conv(aOptions, options);
    wgpu::Adapter adapter = nullptr;
    _instance.RequestAdapter(
        &aOptions,
        [](WGPURequestAdapterStatus, WGPUAdapter cAdapter, const char *message,
           void *userdata) {
          if (message != nullptr) {
            fprintf(stderr, "%s", message);
            return;
          }
          *static_cast<wgpu::Adapter *>(userdata) =
              wgpu::Adapter::Acquire(cAdapter);
        },
        &adapter);
    // TODO: implement returning null jsi value
    if (!adapter) {
      throw std::runtime_error("Failed to request adapter");
    }

    return std::make_shared<GPUAdapter>(std::move(adapter), _async);
  });
}

// Async impl keeping here as a reference
// std::future<std::shared_ptr<GPUAdapter>>
// GPU::requestAdapter(std::shared_ptr<GPURequestAdapterOptions> options) {
//   return _async->runAsync([=](wgpu::Instance *instance) {
//     auto aOptions = options->getInstance();
//     wgpu::Adapter adapter = nullptr;
//     auto result = std::make_shared<GPUAdapter>(adapter, _async);
//     wgpu::RequestAdapterCallbackInfo callback;
//     callback.callback = [](WGPURequestAdapterStatus, WGPUAdapter cAdapter,
//                            const char *message, void *userdata) {
//       if (message != nullptr) {
//         fprintf(stderr, "%s", message);
//         return;
//       }
//       *static_cast<wgpu::Adapter *>(userdata) =
//           wgpu::Adapter::Acquire(cAdapter);
//     };
//     callback.mode = wgpu::CallbackMode::WaitAnyOnly;
//     callback.userdata = &(result->_instance);
//     auto future = _instance.RequestAdapter(aOptions, callback);
//     instance->WaitAny(future, UINT64_MAX);
//     return result;
//   });
// }

wgpu::TextureFormat GPU::getPreferredCanvasFormat() {
#if defined(__ANDROID__)
  return wgpu::TextureFormat::RGBA8Unorm;
#else
  return wgpu::TextureFormat::BGRA8Unorm;
#endif // defined(__ANDROID__)
}

void GPU::attachSurface(void *window) {
  runTriangleDemo(window, 200, 200);
}

void GPU::runTriangleDemo(void *window, int width, int height) {
  auto instance = CreateInstance(nullptr);
  SurfaceDescriptorFromAndroidNativeWindow androidSurfaceDesc = {};
  androidSurfaceDesc.window = window;

  // Set up the generic surface descriptor to use the platform-specific one
  SurfaceDescriptor surfaceDesc = {};
  surfaceDesc.nextInChain =
    reinterpret_cast<const ChainedStruct *>(&androidSurfaceDesc);
  Surface surface = instance.CreateSurface(&surfaceDesc);

  RequestAdapterOptions adapterOpts;

  wgpu::Adapter adapter = nullptr;
  instance.RequestAdapter(
    nullptr,
    [](WGPURequestAdapterStatus, WGPUAdapter cAdapter, const char *message,
       void *userdata) {
      if (message != nullptr) {
        fprintf(stderr, "%s", message);
        return;
      }
      *static_cast<wgpu::Adapter *>(userdata) =
        wgpu::Adapter::Acquire(cAdapter);
    },
    &adapter);

  wgpu::Device device = nullptr;
  adapter.RequestDevice(
    nullptr,
    [](WGPURequestDeviceStatus, WGPUDevice cDevice, const char *message,
       void *userdata) {
      if (message != nullptr) {
        fprintf(stderr, "%s", message);
        return;
      }
      *static_cast<wgpu::Device *>(userdata) = wgpu::Device::Acquire(cDevice);
    },
    &device);

  DeviceDescriptor deviceDesc;
  deviceDesc.label = "My Device";
  // deviceDesc.requiredFeaturesCount = 0;
  deviceDesc.requiredLimits = nullptr;
  deviceDesc.defaultQueue.label = "The default queue";

  Queue queue = device.GetQueue();

  SwapChainDescriptor swapChainDesc;
  swapChainDesc.width = width;
  swapChainDesc.height = height;
  swapChainDesc.usage = TextureUsage::RenderAttachment;
  swapChainDesc.format = TextureFormat::RGBA8Unorm;
  swapChainDesc.presentMode = PresentMode::Fifo;
  SwapChain swapChain = device.CreateSwapChain(surface, &swapChainDesc);

  const char *shaderSource = R"(
@vertex
fn vs_main(@builtin(vertex_index) in_vertex_index: u32) -> @builtin(position) vec4<f32> {
var p = vec2f(0.0, 0.0);
if (in_vertex_index == 0u) {
  p = vec2f(-0.5, -0.5);
} else if (in_vertex_index == 1u) {
  p = vec2f(0.5, -0.5);
} else {
  p = vec2f(0.0, 0.5);
}
return vec4f(p, 0.0, 1.0);
}

@fragment
fn fs_main() -> @location(0) vec4f {
  return vec4f(0.3, 0.6, 1.0, 1.0);
}
)";

  ShaderModuleDescriptor shaderDesc;
  // shaderDesc.hintCount = 0;
  // shaderDesc.hints = nullptr;
  //  Use the extension mechanism to load a WGSL shader source code
  ShaderModuleWGSLDescriptor shaderCodeDesc;
  // Set the chained struct's header
  // Connect the chain
  shaderDesc.nextInChain = &shaderCodeDesc;

  // Setup the actual payload of the shader code descriptor
  shaderCodeDesc.code = shaderSource;

  auto shaderModule = device.CreateShaderModule(&shaderDesc);
  RenderPipelineDescriptor pipelineDesc;

  // Vertex fetch
  // (We don't use any input buffer so far)
  pipelineDesc.vertex.bufferCount = 0;
  pipelineDesc.vertex.buffers = nullptr;

  // Vertex shader
  pipelineDesc.vertex.module = shaderModule;
  pipelineDesc.vertex.entryPoint = "vs_main";
  pipelineDesc.vertex.constantCount = 0;
  pipelineDesc.vertex.constants = nullptr;

  // Primitive assembly and rasterization
  // Each sequence of 3 vertices is considered as a triangle
  pipelineDesc.primitive.topology = PrimitiveTopology::TriangleList;
  // We'll see later how to specify the order in which vertices should
  // be connected. When not specified, vertices are considered
  // sequentially.
  pipelineDesc.primitive.stripIndexFormat = IndexFormat::Undefined;
  // The face orientation is defined by assuming that when looking
  // from the front of the face, its corner vertices are enumerated
  // in the counter-clockwise (CCW) order.
  pipelineDesc.primitive.frontFace = FrontFace::CCW;
  // But the face orientation does not matter much because we do not
  // cull (i.e. "hide") the faces pointing away from us (which is often
  // used for optimization).
  pipelineDesc.primitive.cullMode = CullMode::None;

  // Fragment shader
  FragmentState fragmentState;
  pipelineDesc.fragment = &fragmentState;
  fragmentState.module = shaderModule;
  fragmentState.entryPoint = "fs_main";
  fragmentState.constantCount = 0;
  fragmentState.constants = nullptr;

  // Configure blend state
  BlendState blendState;
  // Usual alpha blending for the color:
  blendState.color.srcFactor = BlendFactor::SrcAlpha;
  blendState.color.dstFactor = BlendFactor::OneMinusSrcAlpha;
  blendState.color.operation = BlendOperation::Add;
  // We leave the target alpha untouched:
  blendState.alpha.srcFactor = BlendFactor::Zero;
  blendState.alpha.dstFactor = BlendFactor::One;
  blendState.alpha.operation = BlendOperation::Add;

  ColorTargetState colorTarget;
  colorTarget.format = TextureFormat::RGBA8Unorm;
  colorTarget.blend = &blendState;
  colorTarget.writeMask = ColorWriteMask::All; // We could write to only some
  // of the color channels.

  // We have only one target because our render pass has only one output
  // color attachment.
  fragmentState.targetCount = 1;
  fragmentState.targets = &colorTarget;

  // Depth and stencil tests are not used here
  pipelineDesc.depthStencil = nullptr;

  // Multi-sampling
  // Samples per pixel
  pipelineDesc.multisample.count = 1;
  // Default value for the mask, meaning "all bits on"
  pipelineDesc.multisample.mask = ~0u;
  // Default value as well (irrelevant for count = 1 anyways)
  pipelineDesc.multisample.alphaToCoverageEnabled = false;

  // Pipeline layout
  pipelineDesc.layout = nullptr;

  RenderPipeline pipeline = device.CreateRenderPipeline(&pipelineDesc);

  TextureView nextTexture = swapChain.GetCurrentTextureView();

  CommandEncoderDescriptor commandEncoderDesc;
  commandEncoderDesc.label = "Command Encoder";
  CommandEncoder encoder = device.CreateCommandEncoder(&commandEncoderDesc);

  RenderPassDescriptor renderPassDesc;

  RenderPassColorAttachment renderPassColorAttachment;
  renderPassColorAttachment.view = nextTexture;
  renderPassColorAttachment.resolveTarget = nullptr;
  renderPassColorAttachment.loadOp = LoadOp::Clear;
  renderPassColorAttachment.storeOp = StoreOp::Store;
  renderPassColorAttachment.depthSlice = UINT32_MAX;
  renderPassColorAttachment.clearValue = Color{0.0, 1.0, 1.0, 1.0};
  renderPassDesc.colorAttachmentCount = 1;
  renderPassDesc.colorAttachments = &renderPassColorAttachment;

  renderPassDesc.depthStencilAttachment = nullptr;
  // renderPassDesc.timestampWriteCount = 0;
  renderPassDesc.timestampWrites = nullptr;
  RenderPassEncoder renderPass = encoder.BeginRenderPass(&renderPassDesc);

  // In its overall outline, drawing a triangle is as simple as this:
  // Select which render pipeline to use
  renderPass.SetPipeline(pipeline);
  // Draw 1 instance of a 3-vertices shape
  renderPass.Draw(3, 1, 0, 0);

  renderPass.End();

  CommandBufferDescriptor cmdBufferDescriptor;
  cmdBufferDescriptor.label = "Command buffer";
  CommandBuffer command = encoder.Finish(&cmdBufferDescriptor);
  std::vector<CommandBuffer> commands;
  commands.push_back(command);
  queue.Submit(commands.size(), commands.data());
  bool done = false;
  queue.OnSubmittedWorkDone(
    [](WGPUQueueWorkDoneStatus status, void *userdata) {
      auto done = static_cast<bool *>(userdata);
      *done = true;
    },
    &done);
  while (!done) {
    instance.ProcessEvents();
  }
  swapChain.Present();

  // surface.release();
  // glfwDestroyWindow(window);
  // glfwTerminate();
}

} // namespace rnwgpu
