import React from "react";
import { Pressable, ScrollView, StyleSheet, Text, View } from "react-native";

import { FEATURES, type Modes } from "./features";

type Props = {
  modes: Modes;
  onCycle: (key: keyof Modes, optionsCount: number) => void;
};

export const EffectToolbar = ({ modes, onCycle }: Props) => {
  return (
    <View style={styles.toolbar} pointerEvents="box-none">
      <ScrollView
        horizontal
        showsHorizontalScrollIndicator={false}
        contentContainerStyle={styles.toolbarContent}
      >
        {FEATURES.map((f) => (
          <Pressable
            key={f.title}
            onPress={() => onCycle(f.key, f.labels.length)}
            style={({ pressed }) => [
              styles.button,
              pressed && styles.buttonPressed,
            ]}
          >
            <Text style={styles.buttonTitle}>{f.title}</Text>
            <Text style={styles.buttonValue}>{f.labels[modes[f.key]]}</Text>
          </Pressable>
        ))}
      </ScrollView>
    </View>
  );
};

const styles = StyleSheet.create({
  toolbar: {
    position: "absolute",
    left: 0,
    right: 0,
    top: 60,
  },
  toolbarContent: {
    paddingHorizontal: 12,
    gap: 8,
  },
  button: {
    backgroundColor: "rgba(0,0,0,0.55)",
    borderColor: "rgba(255,255,255,0.18)",
    borderWidth: 1,
    borderRadius: 14,
    paddingHorizontal: 12,
    paddingVertical: 8,
    minWidth: 84,
  },
  buttonPressed: {
    backgroundColor: "rgba(255,255,255,0.18)",
  },
  buttonTitle: {
    color: "rgba(255,255,255,0.65)",
    fontSize: 11,
    fontWeight: "500",
    letterSpacing: 0.4,
    textTransform: "uppercase",
  },
  buttonValue: {
    color: "white",
    fontSize: 15,
    fontWeight: "600",
    marginTop: 2,
  },
});
