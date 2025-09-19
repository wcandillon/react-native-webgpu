const globalTeardown = () => {
  if (global.testClient) {
    global.testClient.close();
  }
  if (global.testServer) {
    global.testServer.close();
  }
};

module.exports = globalTeardown;
