const globalTeardown = () => {
  if (global.testClient) {
    global.testClient.close();
  }
  if (global.testServer) {
    global.testServer.close();
  }
  if (global.testFixtureServer) {
    global.testFixtureServer.close();
  }
};

// eslint-disable-next-line import/no-default-export
export default globalTeardown;
