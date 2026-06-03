import type { ReactNode } from "react";
import clsx from "clsx";
import Link from "@docusaurus/Link";
import useDocusaurusContext from "@docusaurus/useDocusaurusContext";
import Layout from "@theme/Layout";

import styles from "./index.module.css";

interface Feature {
  emoji: string;
  title: string;
  description: string;
  to: string;
}

const FEATURES: Feature[] = [
  {
    emoji: "🧊",
    title: "Three.js",
    description:
      "Run Three.js r168+ natively through its WebGPURenderer, with react-three-fiber support.",
    to: "/docs/integrations/three-js",
  },
  {
    emoji: "🧠",
    title: "TensorFlow.js",
    description:
      "GPU-accelerated inference on device using the @tensorflow/tfjs-backend-webgpu backend.",
    to: "/docs/integrations/tensorflow",
  },
  {
    emoji: "📸",
    title: "Vision Camera",
    description:
      "Zero-copy camera frames imported as external textures and processed in WGSL on the worklet runtime.",
    to: "/docs/integrations/vision-camera",
  },
  {
    emoji: "🔷",
    title: "TypeGPU",
    description:
      "Type-safe WGSL authored in TypeScript with reactive GPU state via @typegpu/react.",
    to: "/docs/integrations/typegpu",
  },
  {
    emoji: "🪞",
    title: "Symmetric with the Web",
    description:
      "The same WebGPU API you already know. Synchronous context access, identical pixel-ratio handling.",
    to: "/docs/differences/frame-scheduling",
  },
  {
    emoji: "🎞️",
    title: "Reanimated",
    description:
      "Drive rendering from the UI thread by passing GPU objects straight into worklets.",
    to: "/docs/differences/reanimated",
  },
];

function Hero() {
  const { siteConfig } = useDocusaurusContext();
  return (
    <header className={clsx("hero", styles.heroBanner)}>
      <div className="container">
        <h1 className={styles.heroTitle}>{siteConfig.title}</h1>
        <p className={styles.heroSubtitle}>{siteConfig.tagline}</p>
        <div className={styles.buttons}>
          <Link
            className="button button--secondary button--lg"
            to="/docs/intro"
          >
            Get Started
          </Link>
          <Link
            className="button button--outline button--secondary button--lg"
            to="/docs/installation"
          >
            Installation
          </Link>
        </div>
      </div>
    </header>
  );
}

function FeatureCard({ emoji, title, description, to }: Feature) {
  return (
    <Link to={to} className={styles.card}>
      <div className={styles.cardTitle}>
        <span className={styles.cardEmoji}>{emoji}</span>
        {title}
      </div>
      <p>{description}</p>
    </Link>
  );
}

export default function Home(): ReactNode {
  return (
    <Layout
      title="React Native WebGPU"
      description="WebGPU for React Native, powered by Dawn. Three.js, TensorFlow.js, Vision Camera and TypeGPU integrations."
    >
      <Hero />
      <main>
        <section className={styles.features}>
          {FEATURES.map((feature) => (
            <FeatureCard key={feature.title} {...feature} />
          ))}
        </section>
      </main>
    </Layout>
  );
}
