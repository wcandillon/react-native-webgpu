/* eslint-disable no-eval */
/* eslint-disable @typescript-eslint/no-explicit-any */
import React, { useEffect } from "react";
import { Text, View } from "react-native";

import { useClient } from "./useClient";

export const CI = process.env.CI === "true";

export const Tests = () => {
  const [client, hostname] = useClient();
  useEffect(() => {
    if (client !== null) {
      client.onmessage = (e) => {
        const tree: any = JSON.parse(e.data);
        if (tree.code) {
          client.send(
            JSON.stringify(
              eval(
                `(function Main() {
                  return (${tree.code})(this.ctx);
                })`,
              ).call({
                ctx: tree.ctx,
              }),
            ),
          );
        }
      };
      return () => {
        client.close();
      };
    }
    return;
  }, [client]);
  return (
    <View style={{ flex: 1, backgroundColor: "white" }}>
      <Text style={{ color: "black" }}>
        {client === null
          ? `⚪️ Connecting to ${hostname}. Use yarn e2e to run tests.`
          : "🟢 Waiting for the server to send tests"}
      </Text>
    </View>
  );
};
