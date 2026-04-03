import { defineConfig } from "vite";

export default defineConfig({
  build: {
    outDir: "dist",
    emptyOutDir: true,
    chunkSizeWarningLimit: 200,
    target: "esnext",
    rollupOptions: {
      output: {
        manualChunks: {
          vendor: [
            "lit",
            "lit/decorators.js",
            "lit/directives/class-map.js",
            "lit/directives/style-map.js",
          ],
          markdown: ["marked", "dompurify"],
          syntax: ["shiki/core", "shiki/engine/oniguruma"],
          math: ["katex"],
        },
      },
    },
  },
  server: {
    proxy: {
      "/ws": {
        target: "http://localhost:3000",
        ws: true,
      },
    },
  },
});
