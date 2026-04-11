import { defineConfig } from "vite";

export default defineConfig({
  build: {
    outDir: "dist",
    emptyOutDir: true,
    chunkSizeWarningLimit: 200,
    target: "esnext",
    rollupOptions: {
      output: {
        manualChunks(id: string) {
          if (
            id.includes("node_modules/lit") ||
            id.includes("node_modules/@lit")
          )
            return "vendor";
          if (
            id.includes("node_modules/marked") ||
            id.includes("node_modules/dompurify")
          )
            return "markdown";
          if (id.includes("node_modules/shiki")) return "syntax";
          if (id.includes("node_modules/katex")) return "math";
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
