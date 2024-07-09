#pragma once

#include "webgpu/webgpu_cpp.h"

#include "RNFJSIConverter.h"

namespace margelo {

template <> struct JSIConverter<wgpu::AddressMode> {
  static wgpu::AddressMode fromJSI(jsi::Runtime &runtime,
                                   const jsi::Value &arg) {
    auto str = arg.asString(runtime).utf8(runtime);
    if (str == "clamp-to-edge") {
      return wgpu::AddressMode::ClampToEdge;
    }
    if (str == "repeat") {
      return wgpu::AddressMode::Repeat;
    }
    if (str == "mirror-repeat") {
      return wgpu::AddressMode::MirrorRepeat;
    }
  }

  static jsi::Value toJSI(jsi::Runtime &runtime, wgpu::AddressMode arg) {
    // No conversions here
    return jsi::Value::null();
  }
};

template <> struct JSIConverter<wgpu::BlendFactor> {
  static wgpu::BlendFactor fromJSI(jsi::Runtime &runtime,
                                   const jsi::Value &arg) {
    auto str = arg.asString(runtime).utf8(runtime);
    if (str == "zero") {
      return wgpu::BlendFactor::Zero;
    }
    if (str == "one") {
      return wgpu::BlendFactor::One;
    }
    if (str == "src") {
      return wgpu::BlendFactor::Src;
    }
    if (str == "one-minus-src") {
      return wgpu::BlendFactor::OneMinusSrc;
    }
    if (str == "src-alpha") {
      return wgpu::BlendFactor::SrcAlpha;
    }
    if (str == "one-minus-src-alpha") {
      return wgpu::BlendFactor::OneMinusSrcAlpha;
    }
    if (str == "dst") {
      return wgpu::BlendFactor::Dst;
    }
    if (str == "one-minus-dst") {
      return wgpu::BlendFactor::OneMinusDst;
    }
    if (str == "dst-alpha") {
      return wgpu::BlendFactor::DstAlpha;
    }
    if (str == "one-minus-dst-alpha") {
      return wgpu::BlendFactor::OneMinusDstAlpha;
    }
    if (str == "src-alpha-saturated") {
      return wgpu::BlendFactor::SrcAlphaSaturated;
    }
    if (str == "constant") {
      return wgpu::BlendFactor::Constant;
    }
    if (str == "one-minus-constant") {
      return wgpu::BlendFactor::OneMinusConstant;
    }
  }

  static jsi::Value toJSI(jsi::Runtime &runtime, wgpu::BlendFactor arg) {
    // No conversions here
    return jsi::Value::null();
  }
};

template <> struct JSIConverter<wgpu::BlendOperation> {
  static wgpu::BlendOperation fromJSI(jsi::Runtime &runtime,
                                      const jsi::Value &arg) {
    auto str = arg.asString(runtime).utf8(runtime);
    if (str == "add") {
      return wgpu::BlendOperation::Add;
    }
    if (str == "subtract") {
      return wgpu::BlendOperation::Subtract;
    }
    if (str == "reverse-subtract") {
      return wgpu::BlendOperation::ReverseSubtract;
    }
    if (str == "min") {
      return wgpu::BlendOperation::Min;
    }
    if (str == "max") {
      return wgpu::BlendOperation::Max;
    }
  }

  static jsi::Value toJSI(jsi::Runtime &runtime, wgpu::BlendOperation arg) {
    // No conversions here
    return jsi::Value::null();
  }
};

template <> struct JSIConverter<wgpu::BufferBindingType> {
  static wgpu::BufferBindingType fromJSI(jsi::Runtime &runtime,
                                         const jsi::Value &arg) {
    auto str = arg.asString(runtime).utf8(runtime);
    if (str == "uniform") {
      return wgpu::BufferBindingType::Uniform;
    }
    if (str == "storage") {
      return wgpu::BufferBindingType::Storage;
    }
    if (str == "read-only-storage") {
      return wgpu::BufferBindingType::ReadOnlyStorage;
    }
  }

  static jsi::Value toJSI(jsi::Runtime &runtime, wgpu::BufferBindingType arg) {
    // No conversions here
    return jsi::Value::null();
  }
};

template <> struct JSIConverter<wgpu::BufferMapState> {
  static wgpu::BufferMapState fromJSI(jsi::Runtime &runtime,
                                      const jsi::Value &arg) {
    auto str = arg.asString(runtime).utf8(runtime);
    if (str == "unmapped") {
      return wgpu::BufferMapState::Unmapped;
    }
    if (str == "pending") {
      return wgpu::BufferMapState::Pending;
    }
    if (str == "mapped") {
      return wgpu::BufferMapState::Mapped;
    }
  }

  static jsi::Value toJSI(jsi::Runtime &runtime, wgpu::BufferMapState arg) {
    // No conversions here
    return jsi::Value::null();
  }
};

template <> struct JSIConverter<wgpu::CompareFunction> {
  static wgpu::CompareFunction fromJSI(jsi::Runtime &runtime,
                                       const jsi::Value &arg) {
    auto str = arg.asString(runtime).utf8(runtime);
    if (str == "never") {
      return wgpu::CompareFunction::Never;
    }
    if (str == "less") {
      return wgpu::CompareFunction::Less;
    }
    if (str == "equal") {
      return wgpu::CompareFunction::Equal;
    }
    if (str == "less-equal") {
      return wgpu::CompareFunction::LessEqual;
    }
    if (str == "greater") {
      return wgpu::CompareFunction::Greater;
    }
    if (str == "not-equal") {
      return wgpu::CompareFunction::NotEqual;
    }
    if (str == "greater-equal") {
      return wgpu::CompareFunction::GreaterEqual;
    }
    if (str == "always") {
      return wgpu::CompareFunction::Always;
    }
  }

  static jsi::Value toJSI(jsi::Runtime &runtime, wgpu::CompareFunction arg) {
    // No conversions here
    return jsi::Value::null();
  }
};

template <> struct JSIConverter<wgpu::CompilationMessageType> {
  static wgpu::CompilationMessageType fromJSI(jsi::Runtime &runtime,
                                              const jsi::Value &arg) {
    auto str = arg.asString(runtime).utf8(runtime);
    if (str == "error") {
      return wgpu::CompilationMessageType::Error;
    }
    if (str == "warning") {
      return wgpu::CompilationMessageType::Warning;
    }
    if (str == "info") {
      return wgpu::CompilationMessageType::Info;
    }
  }

  static jsi::Value toJSI(jsi::Runtime &runtime,
                          wgpu::CompilationMessageType arg) {
    // No conversions here
    return jsi::Value::null();
  }
};

template <> struct JSIConverter<wgpu::CullMode> {
  static wgpu::CullMode fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto str = arg.asString(runtime).utf8(runtime);
    if (str == "none") {
      return wgpu::CullMode::None;
    }
    if (str == "front") {
      return wgpu::CullMode::Front;
    }
    if (str == "back") {
      return wgpu::CullMode::Back;
    }
  }

  static jsi::Value toJSI(jsi::Runtime &runtime, wgpu::CullMode arg) {
    // No conversions here
    return jsi::Value::null();
  }
};

template <> struct JSIConverter<wgpu::DeviceLostReason> {
  static wgpu::DeviceLostReason fromJSI(jsi::Runtime &runtime,
                                        const jsi::Value &arg) {
    auto str = arg.asString(runtime).utf8(runtime);
    if (str == "unknown") {
      return wgpu::DeviceLostReason::Unknown;
    }
    if (str == "destroyed") {
      return wgpu::DeviceLostReason::Destroyed;
    }
  }

  static jsi::Value toJSI(jsi::Runtime &runtime, wgpu::DeviceLostReason arg) {
    // No conversions here
    return jsi::Value::null();
  }
};

template <> struct JSIConverter<wgpu::ErrorFilter> {
  static wgpu::ErrorFilter fromJSI(jsi::Runtime &runtime,
                                   const jsi::Value &arg) {
    auto str = arg.asString(runtime).utf8(runtime);
    if (str == "validation") {
      return wgpu::ErrorFilter::Validation;
    }
    if (str == "out-of-memory") {
      return wgpu::ErrorFilter::OutOfMemory;
    }
    if (str == "internal") {
      return wgpu::ErrorFilter::Internal;
    }
  }

  static jsi::Value toJSI(jsi::Runtime &runtime, wgpu::ErrorFilter arg) {
    // No conversions here
    return jsi::Value::null();
  }
};

template <> struct JSIConverter<wgpu::FeatureName> {
  static wgpu::FeatureName fromJSI(jsi::Runtime &runtime,
                                   const jsi::Value &arg) {
    auto str = arg.asString(runtime).utf8(runtime);
    if (str == "depth-clip-control") {
      return wgpu::FeatureName::DepthClipControl;
    }
    if (str == "depth32float-stencil8") {
      return wgpu::FeatureName::Depth32FloatStencil8;
    }
    if (str == "texture-compression-bc") {
      return wgpu::FeatureName::TextureCompressionBC;
    }
    if (str == "texture-compression-etc2") {
      return wgpu::FeatureName::TextureCompressionETC2;
    }
    if (str == "texture-compression-astc") {
      return wgpu::FeatureName::TextureCompressionASTC;
    }
    if (str == "timestamp-query") {
      return wgpu::FeatureName::TimestampQuery;
    }
    if (str == "indirect-first-instance") {
      return wgpu::FeatureName::IndirectFirstInstance;
    }
    if (str == "shader-f16") {
      return wgpu::FeatureName::ShaderF16;
    }
    if (str == "rg11b10ufloat-renderable") {
      return wgpu::FeatureName::RG11B10UfloatRenderable;
    }
    if (str == "bgra8unorm-storage") {
      return wgpu::FeatureName::BGRA8UnormStorage;
    }
    if (str == "float32-filterable") {
      return wgpu::FeatureName::Float32Filterable;
    }
  }

  static jsi::Value toJSI(jsi::Runtime &runtime, wgpu::FeatureName arg) {
    // No conversions here
    return jsi::Value::null();
  }
};

template <> struct JSIConverter<wgpu::FilterMode> {
  static wgpu::FilterMode fromJSI(jsi::Runtime &runtime,
                                  const jsi::Value &arg) {
    auto str = arg.asString(runtime).utf8(runtime);
    if (str == "nearest") {
      return wgpu::FilterMode::Nearest;
    }
    if (str == "linear") {
      return wgpu::FilterMode::Linear;
    }
  }

  static jsi::Value toJSI(jsi::Runtime &runtime, wgpu::FilterMode arg) {
    // No conversions here
    return jsi::Value::null();
  }
};

template <> struct JSIConverter<wgpu::FrontFace> {
  static wgpu::FrontFace fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto str = arg.asString(runtime).utf8(runtime);
    if (str == "ccw") {
      return wgpu::FrontFace::CCW;
    }
    if (str == "cw") {
      return wgpu::FrontFace::CW;
    }
  }

  static jsi::Value toJSI(jsi::Runtime &runtime, wgpu::FrontFace arg) {
    // No conversions here
    return jsi::Value::null();
  }
};

template <> struct JSIConverter<wgpu::IndexFormat> {
  static wgpu::IndexFormat fromJSI(jsi::Runtime &runtime,
                                   const jsi::Value &arg) {
    auto str = arg.asString(runtime).utf8(runtime);
    if (str == "uint16") {
      return wgpu::IndexFormat::Uint16;
    }
    if (str == "uint32") {
      return wgpu::IndexFormat::Uint32;
    }
  }

  static jsi::Value toJSI(jsi::Runtime &runtime, wgpu::IndexFormat arg) {
    // No conversions here
    return jsi::Value::null();
  }
};

template <> struct JSIConverter<wgpu::LoadOp> {
  static wgpu::LoadOp fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto str = arg.asString(runtime).utf8(runtime);
    if (str == "load") {
      return wgpu::LoadOp::Load;
    }
    if (str == "clear") {
      return wgpu::LoadOp::Clear;
    }
  }

  static jsi::Value toJSI(jsi::Runtime &runtime, wgpu::LoadOp arg) {
    // No conversions here
    return jsi::Value::null();
  }
};

template <> struct JSIConverter<wgpu::MipmapFilterMode> {
  static wgpu::MipmapFilterMode fromJSI(jsi::Runtime &runtime,
                                        const jsi::Value &arg) {
    auto str = arg.asString(runtime).utf8(runtime);
    if (str == "nearest") {
      return wgpu::MipmapFilterMode::Nearest;
    }
    if (str == "linear") {
      return wgpu::MipmapFilterMode::Linear;
    }
  }

  static jsi::Value toJSI(jsi::Runtime &runtime, wgpu::MipmapFilterMode arg) {
    // No conversions here
    return jsi::Value::null();
  }
};

template <> struct JSIConverter<wgpu::PowerPreference> {
  static wgpu::PowerPreference fromJSI(jsi::Runtime &runtime,
                                       const jsi::Value &arg) {
    auto str = arg.asString(runtime).utf8(runtime);
    if (str == "low-power") {
      return wgpu::PowerPreference::LowPower;
    }
    if (str == "high-performance") {
      return wgpu::PowerPreference::HighPerformance;
    }
  }

  static jsi::Value toJSI(jsi::Runtime &runtime, wgpu::PowerPreference arg) {
    // No conversions here
    return jsi::Value::null();
  }
};

template <> struct JSIConverter<wgpu::PrimitiveTopology> {
  static wgpu::PrimitiveTopology fromJSI(jsi::Runtime &runtime,
                                         const jsi::Value &arg) {
    auto str = arg.asString(runtime).utf8(runtime);
    if (str == "point-list") {
      return wgpu::PrimitiveTopology::PointList;
    }
    if (str == "line-list") {
      return wgpu::PrimitiveTopology::LineList;
    }
    if (str == "line-strip") {
      return wgpu::PrimitiveTopology::LineStrip;
    }
    if (str == "triangle-list") {
      return wgpu::PrimitiveTopology::TriangleList;
    }
    if (str == "triangle-strip") {
      return wgpu::PrimitiveTopology::TriangleStrip;
    }
  }

  static jsi::Value toJSI(jsi::Runtime &runtime, wgpu::PrimitiveTopology arg) {
    // No conversions here
    return jsi::Value::null();
  }
};

template <> struct JSIConverter<wgpu::QueryType> {
  static wgpu::QueryType fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto str = arg.asString(runtime).utf8(runtime);
    if (str == "occlusion") {
      return wgpu::QueryType::Occlusion;
    }
    if (str == "timestamp") {
      return wgpu::QueryType::Timestamp;
    }
  }

  static jsi::Value toJSI(jsi::Runtime &runtime, wgpu::QueryType arg) {
    // No conversions here
    return jsi::Value::null();
  }
};

template <> struct JSIConverter<wgpu::SamplerBindingType> {
  static wgpu::SamplerBindingType fromJSI(jsi::Runtime &runtime,
                                          const jsi::Value &arg) {
    auto str = arg.asString(runtime).utf8(runtime);
    if (str == "filtering") {
      return wgpu::SamplerBindingType::Filtering;
    }
    if (str == "non-filtering") {
      return wgpu::SamplerBindingType::NonFiltering;
    }
    if (str == "comparison") {
      return wgpu::SamplerBindingType::Comparison;
    }
  }

  static jsi::Value toJSI(jsi::Runtime &runtime, wgpu::SamplerBindingType arg) {
    // No conversions here
    return jsi::Value::null();
  }
};

template <> struct JSIConverter<wgpu::StencilOperation> {
  static wgpu::StencilOperation fromJSI(jsi::Runtime &runtime,
                                        const jsi::Value &arg) {
    auto str = arg.asString(runtime).utf8(runtime);
    if (str == "zero") {
      return wgpu::StencilOperation::Zero;
    }
    if (str == "keep") {
      return wgpu::StencilOperation::Keep;
    }
    if (str == "replace") {
      return wgpu::StencilOperation::Replace;
    }
    if (str == "invert") {
      return wgpu::StencilOperation::Invert;
    }
    if (str == "increment-clamp") {
      return wgpu::StencilOperation::IncrementClamp;
    }
    if (str == "decrement-clamp") {
      return wgpu::StencilOperation::DecrementClamp;
    }
    if (str == "increment-wrap") {
      return wgpu::StencilOperation::IncrementWrap;
    }
    if (str == "decrement-wrap") {
      return wgpu::StencilOperation::DecrementWrap;
    }
  }

  static jsi::Value toJSI(jsi::Runtime &runtime, wgpu::StencilOperation arg) {
    // No conversions here
    return jsi::Value::null();
  }
};

template <> struct JSIConverter<wgpu::StorageTextureAccess> {
  static wgpu::StorageTextureAccess fromJSI(jsi::Runtime &runtime,
                                            const jsi::Value &arg) {
    auto str = arg.asString(runtime).utf8(runtime);
    if (str == "write-only") {
      return wgpu::StorageTextureAccess::WriteOnly;
    }
    if (str == "read-only") {
      return wgpu::StorageTextureAccess::ReadOnly;
    }
    if (str == "read-write") {
      return wgpu::StorageTextureAccess::ReadWrite;
    }
  }

  static jsi::Value toJSI(jsi::Runtime &runtime,
                          wgpu::StorageTextureAccess arg) {
    // No conversions here
    return jsi::Value::null();
  }
};

template <> struct JSIConverter<wgpu::StoreOp> {
  static wgpu::StoreOp fromJSI(jsi::Runtime &runtime, const jsi::Value &arg) {
    auto str = arg.asString(runtime).utf8(runtime);
    if (str == "store") {
      return wgpu::StoreOp::Store;
    }
    if (str == "discard") {
      return wgpu::StoreOp::Discard;
    }
  }

  static jsi::Value toJSI(jsi::Runtime &runtime, wgpu::StoreOp arg) {
    // No conversions here
    return jsi::Value::null();
  }
};

template <> struct JSIConverter<wgpu::TextureAspect> {
  static wgpu::TextureAspect fromJSI(jsi::Runtime &runtime,
                                     const jsi::Value &arg) {
    auto str = arg.asString(runtime).utf8(runtime);
    if (str == "all") {
      return wgpu::TextureAspect::All;
    }
    if (str == "stencil-only") {
      return wgpu::TextureAspect::StencilOnly;
    }
    if (str == "depth-only") {
      return wgpu::TextureAspect::DepthOnly;
    }
  }

  static jsi::Value toJSI(jsi::Runtime &runtime, wgpu::TextureAspect arg) {
    // No conversions here
    return jsi::Value::null();
  }
};

template <> struct JSIConverter<wgpu::TextureDimension> {
  static wgpu::TextureDimension fromJSI(jsi::Runtime &runtime,
                                        const jsi::Value &arg) {
    auto str = arg.asString(runtime).utf8(runtime);
    if (str == "1d") {
      return wgpu::TextureDimension::e1D;
    }
    if (str == "2d") {
      return wgpu::TextureDimension::e2D;
    }
    if (str == "3d") {
      return wgpu::TextureDimension::e3D;
    }
  }

  static jsi::Value toJSI(jsi::Runtime &runtime, wgpu::TextureDimension arg) {
    // No conversions here
    return jsi::Value::null();
  }
};

template <> struct JSIConverter<wgpu::TextureFormat> {
  static wgpu::TextureFormat fromJSI(jsi::Runtime &runtime,
                                     const jsi::Value &arg) {
    auto str = arg.asString(runtime).utf8(runtime);
    if (str == "depth32float-stencil8") {
      return wgpu::TextureFormat::Depth32FloatStencil8;
    }
    if (str == "r8unorm") {
      return wgpu::TextureFormat::R8Unorm;
    }
    if (str == "r8snorm") {
      return wgpu::TextureFormat::R8Snorm;
    }
    if (str == "r8uint") {
      return wgpu::TextureFormat::R8Uint;
    }
    if (str == "r8sint") {
      return wgpu::TextureFormat::R8Sint;
    }
    if (str == "r16uint") {
      return wgpu::TextureFormat::R16Uint;
    }
    if (str == "r16sint") {
      return wgpu::TextureFormat::R16Sint;
    }
    if (str == "r16float") {
      return wgpu::TextureFormat::R16Float;
    }
    if (str == "rg8unorm") {
      return wgpu::TextureFormat::RG8Unorm;
    }
    if (str == "rg8snorm") {
      return wgpu::TextureFormat::RG8Snorm;
    }
    if (str == "rg8uint") {
      return wgpu::TextureFormat::RG8Uint;
    }
    if (str == "rg8sint") {
      return wgpu::TextureFormat::RG8Sint;
    }
    if (str == "r32uint") {
      return wgpu::TextureFormat::R32Uint;
    }
    if (str == "r32sint") {
      return wgpu::TextureFormat::R32Sint;
    }
    if (str == "r32float") {
      return wgpu::TextureFormat::R32Float;
    }
    if (str == "rg16uint") {
      return wgpu::TextureFormat::RG16Uint;
    }
    if (str == "rg16sint") {
      return wgpu::TextureFormat::RG16Sint;
    }
    if (str == "rg16float") {
      return wgpu::TextureFormat::RG16Float;
    }
    if (str == "rgba8unorm") {
      return wgpu::TextureFormat::RGBA8Unorm;
    }
    if (str == "rgba8unorm-srgb") {
      return wgpu::TextureFormat::RGBA8UnormSrgb;
    }
    if (str == "rgba8snorm") {
      return wgpu::TextureFormat::RGBA8Snorm;
    }
    if (str == "rgba8uint") {
      return wgpu::TextureFormat::RGBA8Uint;
    }
    if (str == "rgba8sint") {
      return wgpu::TextureFormat::RGBA8Sint;
    }
    if (str == "bgra8unorm") {
      return wgpu::TextureFormat::BGRA8Unorm;
    }
    if (str == "bgra8unorm-srgb") {
      return wgpu::TextureFormat::BGRA8UnormSrgb;
    }
    if (str == "rgb9e5ufloat") {
      return wgpu::TextureFormat::RGB9E5Ufloat;
    }
    if (str == "rgb10a2uint") {
      return wgpu::TextureFormat::RGB10A2Uint;
    }
    if (str == "rgb10a2unorm") {
      return wgpu::TextureFormat::RGB10A2Unorm;
    }
    if (str == "rg11b10ufloat") {
      return wgpu::TextureFormat::RG11B10Ufloat;
    }
    if (str == "rg32uint") {
      return wgpu::TextureFormat::RG32Uint;
    }
    if (str == "rg32sint") {
      return wgpu::TextureFormat::RG32Sint;
    }
    if (str == "rg32float") {
      return wgpu::TextureFormat::RG32Float;
    }
    if (str == "rgba16uint") {
      return wgpu::TextureFormat::RGBA16Uint;
    }
    if (str == "rgba16sint") {
      return wgpu::TextureFormat::RGBA16Sint;
    }
    if (str == "rgba16float") {
      return wgpu::TextureFormat::RGBA16Float;
    }
    if (str == "rgba32uint") {
      return wgpu::TextureFormat::RGBA32Uint;
    }
    if (str == "rgba32sint") {
      return wgpu::TextureFormat::RGBA32Sint;
    }
    if (str == "rgba32float") {
      return wgpu::TextureFormat::RGBA32Float;
    }
    if (str == "stencil8") {
      return wgpu::TextureFormat::Stencil8;
    }
    if (str == "depth16unorm") {
      return wgpu::TextureFormat::Depth16Unorm;
    }
    if (str == "depth24plus") {
      return wgpu::TextureFormat::Depth24Plus;
    }
    if (str == "depth24plus-stencil8") {
      return wgpu::TextureFormat::Depth24PlusStencil8;
    }
    if (str == "depth32float") {
      return wgpu::TextureFormat::Depth32Float;
    }
    if (str == "bc1-rgba-unorm") {
      return wgpu::TextureFormat::BC1RGBAUnorm;
    }
    if (str == "bc1-rgba-unorm-srgb") {
      return wgpu::TextureFormat::BC1RGBAUnormSrgb;
    }
    if (str == "bc2-rgba-unorm") {
      return wgpu::TextureFormat::BC2RGBAUnorm;
    }
    if (str == "bc2-rgba-unorm-srgb") {
      return wgpu::TextureFormat::BC2RGBAUnormSrgb;
    }
    if (str == "bc3-rgba-unorm") {
      return wgpu::TextureFormat::BC3RGBAUnorm;
    }
    if (str == "bc3-rgba-unorm-srgb") {
      return wgpu::TextureFormat::BC3RGBAUnormSrgb;
    }
    if (str == "bc4-r-unorm") {
      return wgpu::TextureFormat::BC4RUnorm;
    }
    if (str == "bc4-r-snorm") {
      return wgpu::TextureFormat::BC4RSnorm;
    }
    if (str == "bc5-rg-unorm") {
      return wgpu::TextureFormat::BC5RGUnorm;
    }
    if (str == "bc5-rg-snorm") {
      return wgpu::TextureFormat::BC5RGSnorm;
    }
    if (str == "bc6h-rgb-ufloat") {
      return wgpu::TextureFormat::BC6HRGBUfloat;
    }
    if (str == "bc6h-rgb-float") {
      return wgpu::TextureFormat::BC6HRGBFloat;
    }
    if (str == "bc7-rgba-unorm") {
      return wgpu::TextureFormat::BC7RGBAUnorm;
    }
    if (str == "bc7-rgba-unorm-srgb") {
      return wgpu::TextureFormat::BC7RGBAUnormSrgb;
    }
    if (str == "etc2-rgb8unorm") {
      return wgpu::TextureFormat::ETC2RGB8Unorm;
    }
    if (str == "etc2-rgb8unorm-srgb") {
      return wgpu::TextureFormat::ETC2RGB8UnormSrgb;
    }
    if (str == "etc2-rgb8a1unorm") {
      return wgpu::TextureFormat::ETC2RGB8A1Unorm;
    }
    if (str == "etc2-rgb8a1unorm-srgb") {
      return wgpu::TextureFormat::ETC2RGB8A1UnormSrgb;
    }
    if (str == "etc2-rgba8unorm") {
      return wgpu::TextureFormat::ETC2RGBA8Unorm;
    }
    if (str == "etc2-rgba8unorm-srgb") {
      return wgpu::TextureFormat::ETC2RGBA8UnormSrgb;
    }
    if (str == "eac-r11unorm") {
      return wgpu::TextureFormat::EACR11Unorm;
    }
    if (str == "eac-r11snorm") {
      return wgpu::TextureFormat::EACR11Snorm;
    }
    if (str == "eac-rg11unorm") {
      return wgpu::TextureFormat::EACRG11Unorm;
    }
    if (str == "eac-rg11snorm") {
      return wgpu::TextureFormat::EACRG11Snorm;
    }
    if (str == "astc-4x4-unorm") {
      return wgpu::TextureFormat::ASTC4x4Unorm;
    }
    if (str == "astc-4x4-unorm-srgb") {
      return wgpu::TextureFormat::ASTC4x4UnormSrgb;
    }
    if (str == "astc-5x4-unorm") {
      return wgpu::TextureFormat::ASTC5x4Unorm;
    }
    if (str == "astc-5x4-unorm-srgb") {
      return wgpu::TextureFormat::ASTC5x4UnormSrgb;
    }
    if (str == "astc-5x5-unorm") {
      return wgpu::TextureFormat::ASTC5x5Unorm;
    }
    if (str == "astc-5x5-unorm-srgb") {
      return wgpu::TextureFormat::ASTC5x5UnormSrgb;
    }
    if (str == "astc-6x5-unorm") {
      return wgpu::TextureFormat::ASTC6x5Unorm;
    }
    if (str == "astc-6x5-unorm-srgb") {
      return wgpu::TextureFormat::ASTC6x5UnormSrgb;
    }
    if (str == "astc-6x6-unorm") {
      return wgpu::TextureFormat::ASTC6x6Unorm;
    }
    if (str == "astc-6x6-unorm-srgb") {
      return wgpu::TextureFormat::ASTC6x6UnormSrgb;
    }
    if (str == "astc-8x5-unorm") {
      return wgpu::TextureFormat::ASTC8x5Unorm;
    }
    if (str == "astc-8x5-unorm-srgb") {
      return wgpu::TextureFormat::ASTC8x5UnormSrgb;
    }
    if (str == "astc-8x6-unorm") {
      return wgpu::TextureFormat::ASTC8x6Unorm;
    }
    if (str == "astc-8x6-unorm-srgb") {
      return wgpu::TextureFormat::ASTC8x6UnormSrgb;
    }
    if (str == "astc-8x8-unorm") {
      return wgpu::TextureFormat::ASTC8x8Unorm;
    }
    if (str == "astc-8x8-unorm-srgb") {
      return wgpu::TextureFormat::ASTC8x8UnormSrgb;
    }
    if (str == "astc-10x5-unorm") {
      return wgpu::TextureFormat::ASTC10x5Unorm;
    }
    if (str == "astc-10x5-unorm-srgb") {
      return wgpu::TextureFormat::ASTC10x5UnormSrgb;
    }
    if (str == "astc-10x6-unorm") {
      return wgpu::TextureFormat::ASTC10x6Unorm;
    }
    if (str == "astc-10x6-unorm-srgb") {
      return wgpu::TextureFormat::ASTC10x6UnormSrgb;
    }
    if (str == "astc-10x8-unorm") {
      return wgpu::TextureFormat::ASTC10x8Unorm;
    }
    if (str == "astc-10x8-unorm-srgb") {
      return wgpu::TextureFormat::ASTC10x8UnormSrgb;
    }
    if (str == "astc-10x10-unorm") {
      return wgpu::TextureFormat::ASTC10x10Unorm;
    }
    if (str == "astc-10x10-unorm-srgb") {
      return wgpu::TextureFormat::ASTC10x10UnormSrgb;
    }
    if (str == "astc-12x10-unorm") {
      return wgpu::TextureFormat::ASTC12x10Unorm;
    }
    if (str == "astc-12x10-unorm-srgb") {
      return wgpu::TextureFormat::ASTC12x10UnormSrgb;
    }
    if (str == "astc-12x12-unorm") {
      return wgpu::TextureFormat::ASTC12x12Unorm;
    }
    if (str == "astc-12x12-unorm-srgb") {
      return wgpu::TextureFormat::ASTC12x12UnormSrgb;
    }
  }

  static jsi::Value toJSI(jsi::Runtime &runtime, wgpu::TextureFormat arg) {
    // No conversions here
    return jsi::Value::null();
  }
};

template <> struct JSIConverter<wgpu::TextureSampleType> {
  static wgpu::TextureSampleType fromJSI(jsi::Runtime &runtime,
                                         const jsi::Value &arg) {
    auto str = arg.asString(runtime).utf8(runtime);
    if (str == "float") {
      return wgpu::TextureSampleType::Float;
    }
    if (str == "unfilterable-float") {
      return wgpu::TextureSampleType::UnfilterableFloat;
    }
    if (str == "depth") {
      return wgpu::TextureSampleType::Depth;
    }
    if (str == "sint") {
      return wgpu::TextureSampleType::Sint;
    }
    if (str == "uint") {
      return wgpu::TextureSampleType::Uint;
    }
  }

  static jsi::Value toJSI(jsi::Runtime &runtime, wgpu::TextureSampleType arg) {
    // No conversions here
    return jsi::Value::null();
  }
};

template <> struct JSIConverter<wgpu::TextureViewDimension> {
  static wgpu::TextureViewDimension fromJSI(jsi::Runtime &runtime,
                                            const jsi::Value &arg) {
    auto str = arg.asString(runtime).utf8(runtime);
    if (str == "1d") {
      return wgpu::TextureViewDimension::e1D;
    }
    if (str == "2d") {
      return wgpu::TextureViewDimension::e2D;
    }
    if (str == "3d") {
      return wgpu::TextureViewDimension::e3D;
    }
    if (str == "2d-array") {
      return wgpu::TextureViewDimension::e2DArray;
    }
    if (str == "cube") {
      return wgpu::TextureViewDimension::Cube;
    }
    if (str == "cube-array") {
      return wgpu::TextureViewDimension::CubeArray;
    }
  }

  static jsi::Value toJSI(jsi::Runtime &runtime,
                          wgpu::TextureViewDimension arg) {
    // No conversions here
    return jsi::Value::null();
  }
};

template <> struct JSIConverter<wgpu::VertexFormat> {
  static wgpu::VertexFormat fromJSI(jsi::Runtime &runtime,
                                    const jsi::Value &arg) {
    auto str = arg.asString(runtime).utf8(runtime);
    if (str == "uint32") {
      return wgpu::VertexFormat::Uint32;
    }
    if (str == "uint8x2") {
      return wgpu::VertexFormat::Uint8x2;
    }
    if (str == "uint8x4") {
      return wgpu::VertexFormat::Uint8x4;
    }
    if (str == "sint8x2") {
      return wgpu::VertexFormat::Sint8x2;
    }
    if (str == "sint8x4") {
      return wgpu::VertexFormat::Sint8x4;
    }
    if (str == "unorm8x2") {
      return wgpu::VertexFormat::Unorm8x2;
    }
    if (str == "unorm8x4") {
      return wgpu::VertexFormat::Unorm8x4;
    }
    if (str == "snorm8x2") {
      return wgpu::VertexFormat::Snorm8x2;
    }
    if (str == "snorm8x4") {
      return wgpu::VertexFormat::Snorm8x4;
    }
    if (str == "uint16x2") {
      return wgpu::VertexFormat::Uint16x2;
    }
    if (str == "uint16x4") {
      return wgpu::VertexFormat::Uint16x4;
    }
    if (str == "sint16x2") {
      return wgpu::VertexFormat::Sint16x2;
    }
    if (str == "sint16x4") {
      return wgpu::VertexFormat::Sint16x4;
    }
    if (str == "unorm16x2") {
      return wgpu::VertexFormat::Unorm16x2;
    }
    if (str == "unorm16x4") {
      return wgpu::VertexFormat::Unorm16x4;
    }
    if (str == "snorm16x2") {
      return wgpu::VertexFormat::Snorm16x2;
    }
    if (str == "snorm16x4") {
      return wgpu::VertexFormat::Snorm16x4;
    }
    if (str == "float16x2") {
      return wgpu::VertexFormat::Float16x2;
    }
    if (str == "float16x4") {
      return wgpu::VertexFormat::Float16x4;
    }
    if (str == "float32") {
      return wgpu::VertexFormat::Float32;
    }
    if (str == "float32x2") {
      return wgpu::VertexFormat::Float32x2;
    }
    if (str == "float32x3") {
      return wgpu::VertexFormat::Float32x3;
    }
    if (str == "float32x4") {
      return wgpu::VertexFormat::Float32x4;
    }
    if (str == "uint32x2") {
      return wgpu::VertexFormat::Uint32x2;
    }
    if (str == "uint32x3") {
      return wgpu::VertexFormat::Uint32x3;
    }
    if (str == "uint32x4") {
      return wgpu::VertexFormat::Uint32x4;
    }
    if (str == "sint32") {
      return wgpu::VertexFormat::Sint32;
    }
    if (str == "sint32x2") {
      return wgpu::VertexFormat::Sint32x2;
    }
    if (str == "sint32x3") {
      return wgpu::VertexFormat::Sint32x3;
    }
    if (str == "sint32x4") {
      return wgpu::VertexFormat::Sint32x4;
    }
    if (str == "unorm10-10-10-2") {
      return wgpu::VertexFormat::Unorm10_10_10_2;
    }
  }

  static jsi::Value toJSI(jsi::Runtime &runtime, wgpu::VertexFormat arg) {
    // No conversions here
    return jsi::Value::null();
  }
};

template <> struct JSIConverter<wgpu::VertexStepMode> {
  static wgpu::VertexStepMode fromJSI(jsi::Runtime &runtime,
                                      const jsi::Value &arg) {
    auto str = arg.asString(runtime).utf8(runtime);
    if (str == "vertex") {
      return wgpu::VertexStepMode::Vertex;
    }
    if (str == "instance") {
      return wgpu::VertexStepMode::Instance;
    }
  }

  static jsi::Value toJSI(jsi::Runtime &runtime, wgpu::VertexStepMode arg) {
    // No conversions here
    return jsi::Value::null();
  }
};

} // namespace margelo
