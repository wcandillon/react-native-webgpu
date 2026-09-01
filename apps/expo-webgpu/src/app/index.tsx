import { Link } from "expo-router";
import { Pressable, StyleSheet, Text, View } from "react-native";

const examples = [
  {
    href: "/raw",
    title: "WebGPU triangle",
    description: "Render WGSL directly with the WebGPU API.",
  },
  {
    href: "/three",
    title: "Three.js",
    description: "Create and animate an imperative Three.js scene.",
  },
  {
    href: "/fiber",
    title: "React Three Fiber",
    description: "Describe a Three.js WebGPU scene with React components.",
  },
] as const;

export default function Home() {
  return (
    <View style={styles.container}>
      <Text style={styles.eyebrow}>REACT NATIVE WEBGPU</Text>
      <Text style={styles.title}>Choose an example</Text>
      <Text style={styles.subtitle}>
        Start at the WebGPU API, use Three.js directly, or compose the scene
        with React Three Fiber.
      </Text>

      <View style={styles.list}>
        {examples.map((example) => (
          <Link key={example.href} href={example.href} asChild>
            <Pressable style={styles.card}>
              <Text style={styles.cardTitle}>{example.title}</Text>
              <Text style={styles.cardDescription}>{example.description}</Text>
              <Text style={styles.open}>Open example →</Text>
            </Pressable>
          </Link>
        ))}
      </View>
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    padding: 24,
    backgroundColor: "#070b14",
  },
  eyebrow: {
    marginTop: 24,
    color: "#60a5fa",
    fontSize: 12,
    fontWeight: "700",
    letterSpacing: 1.5,
  },
  title: {
    marginTop: 8,
    color: "#f8fafc",
    fontSize: 32,
    fontWeight: "700",
  },
  subtitle: {
    marginTop: 10,
    color: "#94a3b8",
    fontSize: 16,
    lineHeight: 24,
  },
  list: {
    marginTop: 28,
    gap: 14,
  },
  card: {
    padding: 20,
    borderWidth: 1,
    borderColor: "#24324a",
    borderRadius: 16,
    backgroundColor: "#111827",
  },
  cardTitle: {
    color: "#f8fafc",
    fontSize: 20,
    fontWeight: "600",
  },
  cardDescription: {
    marginTop: 6,
    color: "#94a3b8",
    fontSize: 15,
    lineHeight: 21,
  },
  open: {
    marginTop: 16,
    color: "#60a5fa",
    fontSize: 14,
    fontWeight: "600",
  },
});
