#include "RNFRuntimeState.h"

namespace margelo {

const facebook::jsi::UUID RNFRuntimeState::kRuntimeStateKey = facebook::jsi::UUID();

std::shared_ptr<RNFRuntimeState> RNFRuntimeState::get(facebook::jsi::Runtime& runtime) {
  auto existing = runtime.getRuntimeData(kRuntimeStateKey);
  if (existing) {
    return std::static_pointer_cast<RNFRuntimeState>(existing);
  }

  auto state = std::shared_ptr<RNFRuntimeState>(new RNFRuntimeState());
  runtime.setRuntimeData(kRuntimeStateKey, state);
  return state;
}

} // namespace margelo