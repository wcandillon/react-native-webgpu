#include "GPUShaderModule.h"

#include <utility>

namespace rnwgpu {

std::future<std::shared_ptr<GPUCompilationInfo>>
GPUShaderModule::getCompilationInfo() {
  return _async->runAsync([=](wgpu::Instance *instance) {
    wgpu::CompilationInfo compilationInfo;
    auto result = std::make_shared<GPUCompilationInfo>();
    auto future = _instance.GetCompilationInfo(
        wgpu::CallbackMode::WaitAnyOnly,
        [&result](wgpu::CompilationInfoRequestStatus status,
                  wgpu::CompilationInfo const *compilationInfo) {
          if (status == wgpu::CompilationInfoRequestStatus::Success &&
              compilationInfo) {
            for (size_t i = 0; i < compilationInfo->messageCount; ++i) {
              const auto &wgpuMessage = compilationInfo->messages[i];
              GPUCompilationMessage message;
              message.message = wgpuMessage.message ? wgpuMessage.message : "";
              message.type = wgpuMessage.type;
              message.lineNum = wgpuMessage.lineNum;
              message.linePos = wgpuMessage.linePos;
              message.offset = wgpuMessage.offset;
              message.length = wgpuMessage.length;
              message.utf16LinePos = wgpuMessage.utf16LinePos;
              message.utf16Offset = wgpuMessage.utf16Offset;
              message.utf16Length = wgpuMessage.utf16Length;
              result->_messages.push_back(std::move(message));
            }
          }
        });
    instance->WaitAny(future, UINT64_MAX);
    return result;
  });
}

} // namespace rnwgpu
