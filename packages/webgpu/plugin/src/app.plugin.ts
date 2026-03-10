import {
  ConfigPlugin,
  withAndroidManifest,
  withInfoPlist,
  AndroidConfig,
} from '@expo/config-plugins';

type WebGPUPluginOptions = {
  enableToggles?: string[];
  disableToggles?: string[];
};

const withWebGPUAndroid: ConfigPlugin<WebGPUPluginOptions> = (
  config,
  { enableToggles = [], disableToggles = [] } = {}
) => {
  return withAndroidManifest(config, (config) => {
    const app = AndroidConfig.Manifest.getMainApplicationOrThrow(
      config.modResults
    );
    if (!app['meta-data']) app['meta-data'] = [];

    if (enableToggles.length > 0) {
      app['meta-data'].push({
        $: {
          'android:name': 'com.webgpu.enable_toggles',
          'android:value': enableToggles.join(','),
        },
      });
    }
    if (disableToggles.length > 0) {
      app['meta-data'].push({
        $: {
          'android:name': 'com.webgpu.disable_toggles',
          'android:value': disableToggles.join(','),
        },
      });
    }
    return config;
  });
};

const withWebGPUIos: ConfigPlugin<WebGPUPluginOptions> = (
  config,
  { enableToggles = [], disableToggles = [] } = {}
) => {
  return withInfoPlist(config, (config) => {
    if (enableToggles.length > 0) {
      config.modResults['RNWebGPUEnableToggles'] = enableToggles;
    }
    if (disableToggles.length > 0) {
      config.modResults['RNWebGPUDisableToggles'] = disableToggles;
    }
    return config;
  });
};

const withWebGPU: ConfigPlugin<WebGPUPluginOptions> = (
  config,
  options = {}
) => {
  config = withWebGPUAndroid(config, options);
  config = withWebGPUIos(config, options);
  return config;
};

export default withWebGPU;
