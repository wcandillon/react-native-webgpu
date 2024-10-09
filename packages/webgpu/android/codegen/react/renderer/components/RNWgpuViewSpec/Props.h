#pragma once

#include <react/renderer/components/view/ViewProps.h>
#include <react/renderer/core/PropsParserContext.h>
#include <react/renderer/core/propsConversions.h>

namespace facebook::react {

class WebGPUViewProps final : public ViewProps {
 public:
  WebGPUViewProps() = default;
  WebGPUViewProps(const PropsParserContext& context, const WebGPUViewProps &sourceProps, const RawProps &rawProps)
  : ViewProps(context, sourceProps, rawProps),
    contextId(convertRawProp(context, rawProps, "contextId", sourceProps.contextId, {0}))
  {}

#pragma mark - Props

  int contextId{0};
};

} // namespace facebook::react
