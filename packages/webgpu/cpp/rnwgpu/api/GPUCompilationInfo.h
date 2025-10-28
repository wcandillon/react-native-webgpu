#pragma once

#include <string>
#include <vector>

#include "Unions.h"

#include "RNFHybridObject.h"

#include "webgpu/webgpu_cpp.h"

namespace rnwgpu {

namespace m = margelo;

struct GPUCompilationMessage {
  std::string message;
  wgpu::CompilationMessageType type;
  uint64_t lineNum;
  uint64_t linePos;
  uint64_t offset;
  uint64_t length;
};

class GPUCompilationInfo : public m::HybridObject {
public:
  GPUCompilationInfo() : HybridObject("GPUCompilationInfo") {}

public:
  std::string getBrand() { return _name; }

  std::vector<GPUCompilationMessage> getMessages() { return _messages; }

  void loadHybridMethods() override {
    registerHybridGetter("__brand", &GPUCompilationInfo::getBrand, this);
    registerHybridGetter("messages", &GPUCompilationInfo::getMessages, this);
  }

private:
  std::vector<GPUCompilationMessage> _messages;
  friend class GPUShaderModule;
};

} // namespace rnwgpu

namespace margelo {
template <> struct JSIConverter<std::vector<rnwgpu::GPUCompilationMessage>> {
  static std::vector<rnwgpu::GPUCompilationMessage>
  fromJSI(jsi::Runtime &runtime, const jsi::Value &arg, bool outOfBounds) {
    throw std::runtime_error("Invalid GPUCompilationMessage::fromJSI()");
  }
  static jsi::Value toJSI(jsi::Runtime &runtime,
                          std::vector<rnwgpu::GPUCompilationMessage> arg) {
    jsi::Array result = jsi::Array(runtime, arg.size());
    for (size_t i = 0; i < arg.size(); i++) {
      const auto &message = arg[i];
      jsi::Object messageObj(runtime);
      messageObj.setProperty(
          runtime, "message",
          jsi::String::createFromUtf8(runtime, message.message));
      std::string typeStr;
      EnumMapper::convertEnumToJSUnion(message.type, &typeStr);
      messageObj.setProperty(runtime, "type",
                             jsi::String::createFromUtf8(runtime, typeStr));
      messageObj.setProperty(runtime, "lineNum",
                             static_cast<double>(message.lineNum));
      messageObj.setProperty(runtime, "linePos",
                             static_cast<double>(message.linePos));
      messageObj.setProperty(runtime, "offset",
                             static_cast<double>(message.offset));
      messageObj.setProperty(runtime, "length",
                             static_cast<double>(message.length));
      result.setValueAtIndex(runtime, i, messageObj);
    }
    return result;
  }
};

} // namespace margelo
