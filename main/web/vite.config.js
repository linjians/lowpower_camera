import { defineConfig } from 'vite'
import htmlMinifierTerser from 'vite-plugin-html-minifier-terser';
import removeConsole from "vite-plugin-remove-console";

// https://vitejs.dev/config/
export default defineConfig({
  base: "./",
  server: {
    https: false,
    port: 8080,
    host: "0.0.0.0",
    proxy: {
      "/api/v1": {
        target: "http://192.168.1.1",
        changeOrigin: true,
        secure: false,
        pathRewrite: {
          "^/api/v1": "/api/v1"
        }
      }
    }
  },
  plugins: [
    removeConsole(),
    htmlMinifierTerser({
      removeAttributeQuotes: true, 
      collapseWhitespace: true,
      removeComments: true
    }),
  ],
  build: {
    assetsInlineLimit: 51200,
    target: 'esnext',
    cssTarget: 'chrome61',
    minify: 'esbuild',
    sourcemap: false,
    rollupOptions: {
      output: {
        entryFileNames: `assets/[name].js`,
        chunkFileNames: `assets/[name].js`,
        assetFileNames: `assets/[name].[ext]`,
      }
    }
  }
})
