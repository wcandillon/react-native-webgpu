#pragma once

#include <android/data_space.h>

#include <cstdint>

#include "PlatformContext.h"

namespace rnwgpu {

// The data spaces that predate the standard / transfer / range bit fields
// (HAL_DATASPACE_* in system/graphics.h, "legacy" since Android 6) as the
// field form the NDK header documents for them. Decoders still report them:
// a Pixel 8 on Android 17 tags every H.264 frame with the legacy BT.709
// value. The modern header no longer defines these constants.
inline int32_t canonicalDataSpace(int32_t dataSpace) {
  switch (dataSpace) {
  case 0x101: // HAL_DATASPACE_JFIF
    return STANDARD_BT601_625 | TRANSFER_SMPTE_170M | RANGE_FULL;
  case 0x102: // HAL_DATASPACE_BT601_625
    return STANDARD_BT601_625 | TRANSFER_SMPTE_170M | RANGE_LIMITED;
  case 0x103: // HAL_DATASPACE_BT601_525
    return STANDARD_BT601_525 | TRANSFER_SMPTE_170M | RANGE_LIMITED;
  case 0x104: // HAL_DATASPACE_BT709
    return STANDARD_BT709 | TRANSFER_SMPTE_170M | RANGE_LIMITED;
  case 0x200: // HAL_DATASPACE_SRGB_LINEAR
    return STANDARD_BT709 | TRANSFER_LINEAR | RANGE_FULL;
  case 0x201: // HAL_DATASPACE_SRGB
    return STANDARD_BT709 | TRANSFER_SRGB | RANGE_FULL;
  default:
    return dataSpace;
  }
}

// The color space a frame's data space declares (android/data_space.h, the
// value of Image.getDataSpace() or of the container's color info packed the
// same way). The standard gives the YCbCr matrix and the primaries; the
// transfer function and the range are what their names say. The fields a data
// space leaves unspecified stay unset, and ADATASPACE_UNKNOWN (0) sets nothing.
inline VideoFrameColorSpace colorSpaceFromDataSpace(int32_t dataSpace) {
  VideoFrameColorSpace colorSpace;
  dataSpace = canonicalDataSpace(dataSpace);
  switch (dataSpace & STANDARD_MASK) {
  case STANDARD_BT709:
    colorSpace.matrix = wgpu::ColorSpaceYCbCrMatrixDawn::Rec709;
    // BT.709 shares the sRGB primaries (Dawn's helper only knows them under
    // that name).
    colorSpace.primaries = wgpu::ColorSpacePrimariesDawn::SRGB;
    break;
  case STANDARD_BT601_625:
  case STANDARD_BT601_625_UNADJUSTED:
  case STANDARD_BT601_525:
  case STANDARD_BT601_525_UNADJUSTED:
    colorSpace.matrix = wgpu::ColorSpaceYCbCrMatrixDawn::Rec601;
    colorSpace.primaries = wgpu::ColorSpacePrimariesDawn::Rec601;
    break;
  case STANDARD_BT2020:
  case STANDARD_BT2020_CONSTANT_LUMINANCE:
    colorSpace.matrix = wgpu::ColorSpaceYCbCrMatrixDawn::Rec2020;
    colorSpace.primaries = wgpu::ColorSpacePrimariesDawn::Rec2020;
    break;
  case STANDARD_DCI_P3:
    colorSpace.matrix = wgpu::ColorSpaceYCbCrMatrixDawn::Rec709;
    colorSpace.primaries = wgpu::ColorSpacePrimariesDawn::DisplayP3;
    break;
  default:
    break;
  }
  switch (dataSpace & TRANSFER_MASK) {
  case TRANSFER_LINEAR:
    colorSpace.transfer = wgpu::ColorSpaceTransferDawn::Identity;
    break;
  case TRANSFER_SRGB:
  case TRANSFER_GAMMA2_2: // sRGB is the closest curve Dawn has
    colorSpace.transfer = wgpu::ColorSpaceTransferDawn::SRGB;
    break;
  case TRANSFER_SMPTE_170M:
    colorSpace.transfer = wgpu::ColorSpaceTransferDawn::SMPTE_170M;
    break;
  case TRANSFER_GAMMA2_6:
  case TRANSFER_GAMMA2_8: // BT.1886 (gamma 2.4) is the closest
    colorSpace.transfer = wgpu::ColorSpaceTransferDawn::BT_1886;
    break;
  case TRANSFER_ST2084:
    colorSpace.transfer = wgpu::ColorSpaceTransferDawn::PQ;
    break;
  case TRANSFER_HLG:
    colorSpace.transfer = wgpu::ColorSpaceTransferDawn::HLG;
    break;
  default:
    break;
  }
  switch (dataSpace & RANGE_MASK) {
  case RANGE_FULL:
  case RANGE_EXTENDED:
    colorSpace.range = wgpu::ColorSpaceYCbCrRangeDawn::Full;
    break;
  case RANGE_LIMITED:
    colorSpace.range = wgpu::ColorSpaceYCbCrRangeDawn::Narrow;
    break;
  default:
    break;
  }
  return colorSpace;
}

} // namespace rnwgpu
