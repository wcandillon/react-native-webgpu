"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const config_plugins_1 = require("@expo/config-plugins");
const withWebGPUAndroid = (config, { enableToggles = [], disableToggles = [] } = {}) => {
    return (0, config_plugins_1.withAndroidManifest)(config, (config) => {
        const app = config_plugins_1.AndroidConfig.Manifest.getMainApplicationOrThrow(config.modResults);
        if (!app['meta-data'])
            app['meta-data'] = [];
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
const withWebGPUIos = (config, { enableToggles = [], disableToggles = [] } = {}) => {
    return (0, config_plugins_1.withInfoPlist)(config, (config) => {
        if (enableToggles.length > 0) {
            config.modResults['RNWebGPUEnableToggles'] = enableToggles;
        }
        if (disableToggles.length > 0) {
            config.modResults['RNWebGPUDisableToggles'] = disableToggles;
        }
        return config;
    });
};
const withWebGPU = (config, options = {}) => {
    config = withWebGPUAndroid(config, options);
    config = withWebGPUIos(config, options);
    return config;
};
exports.default = withWebGPU;
