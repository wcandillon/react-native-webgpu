//
// Created by Marc Rousavy on 22.02.24.
//
#include "RNFWorkletRuntimeRegistry.h"

namespace margelo {

std::set<jsi::Runtime*> RNFWorkletRuntimeRegistry::registry_{};
std::mutex RNFWorkletRuntimeRegistry::mutex_{};

} // namespace margelo
