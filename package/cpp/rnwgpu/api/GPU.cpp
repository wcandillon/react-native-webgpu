#include "GPU.h"
#include <utility>
#include <webgpu/webgpu_cpp.h>
#include <android/native_window_jni.h>
#include <android/surface_texture.h>
#include <android/surface_texture_jni.h>
#include <RNWebGPUManager.h>

using namespace wgpu;

#include "Convertors.h"

namespace rnwgpu {

std::future<std::variant<std::nullptr_t, std::shared_ptr<GPUAdapter>>>
GPU::requestAdapter(
    std::optional<std::shared_ptr<GPURequestAdapterOptions>> options) {
  return std::async(
      std::launch::async,
      [this,
       options]() -> std::variant<std::nullptr_t, std::shared_ptr<GPUAdapter>> {
        wgpu::RequestAdapterOptions aOptions;
        Convertor conv;
        if (!conv(aOptions, options)) {
          throw std::runtime_error("Failed to convert GPUDeviceDescriptor");
        }
        wgpu::Adapter adapter = nullptr;
        _instance.RequestAdapter(
            &aOptions,
            [](WGPURequestAdapterStatus, WGPUAdapter cAdapter,
               const char *message, void *userdata) {
              if (message != nullptr) {
                fprintf(stderr, "%s", message);
                return;
              }
              *static_cast<wgpu::Adapter *>(userdata) =
                  wgpu::Adapter::Acquire(cAdapter);
            },
            &adapter);
        if (!adapter) {
          return nullptr;
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
  RequestAdapterOptions adapterOpts;
  wgpu::Adapter adapter = nullptr;
  _instance.RequestAdapter(
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
  Queue queue = device.GetQueue();

  SwapChainDescriptor swapChainDesc;
  swapChainDesc.width = width;
  swapChainDesc.height = height;
  swapChainDesc.usage = TextureUsage::RenderAttachment;
  swapChainDesc.format = TextureFormat::RGBA8Unorm;
  swapChainDesc.presentMode = PresentMode::Fifo;


//    SurfaceDescriptorFromAndroidNativeWindow androidSurfaceDesc = {};
//    androidSurfaceDesc.window = rnwgpu::RNWebGPUManager::window;
//    SurfaceDescriptor surfaceDesc = {};
//    surfaceDesc.nextInChain = reinterpret_cast<const ChainedStruct *>(&androidSurfaceDesc);
//    Surface surface = _instance.CreateSurface(&surfaceDesc);
//    SwapChain swapChain = device.CreateSwapChain(surface, &swapChainDesc);

    auto surface = rnwgpu::RNWebGPUManager::surface;
    SwapChain swapChain = device.CreateSwapChain(*surface, &swapChainDesc);

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
  ShaderModuleWGSLDescriptor shaderCodeDesc;
  shaderDesc.nextInChain = &shaderCodeDesc;
  shaderCodeDesc.code = shaderSource;

  auto shaderModule = device.CreateShaderModule(&shaderDesc);
  RenderPipelineDescriptor pipelineDesc;

  // Vertex shader
  pipelineDesc.vertex.module = shaderModule;
  pipelineDesc.vertex.entryPoint = "vs_main";

  // Fragment shader
  FragmentState fragmentState;
  pipelineDesc.fragment = &fragmentState;
  fragmentState.module = shaderModule;
  fragmentState.entryPoint = "fs_main";

  ColorTargetState colorTarget;
  colorTarget.format = TextureFormat::RGBA8Unorm;
  colorTarget.writeMask = ColorWriteMask::All;
  fragmentState.targetCount = 1;
  fragmentState.targets = &colorTarget;

  RenderPipeline pipeline = device.CreateRenderPipeline(&pipelineDesc);

//  auto texture = RNWebGPUManager::swapChain->GetCurrentTexture();
  auto texture = swapChain.GetCurrentTexture();
  TextureViewDescriptor textureViewDescriptor;
  TextureView nextTexture = texture.CreateView(&textureViewDescriptor);

  CommandEncoderDescriptor commandEncoderDesc;
  commandEncoderDesc.label = "Command Encoder";
  CommandEncoder encoder = device.CreateCommandEncoder(&commandEncoderDesc);

  RenderPassDescriptor renderPassDesc;

  RenderPassColorAttachment renderPassColorAttachment;
  renderPassColorAttachment.view = nextTexture;
  renderPassColorAttachment.loadOp = LoadOp::Clear;
  renderPassColorAttachment.storeOp = StoreOp::Store;
  renderPassColorAttachment.depthSlice = UINT32_MAX;
  renderPassColorAttachment.clearValue = Color{0.0, 1.0, 1.0, 1.0};
  renderPassDesc.colorAttachmentCount = 1;
  renderPassDesc.colorAttachments = &renderPassColorAttachment;

  RenderPassEncoder renderPass = encoder.BeginRenderPass(&renderPassDesc);

  renderPass.SetPipeline(pipeline);
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
    _instance.ProcessEvents();
  }
//  RNWebGPUManager::swapChain->Present();
  swapChain.Present();
}

} // namespace rnwgpu
