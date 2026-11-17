import { defineConfig } from 'vitest/config'
import react from '@vitejs/plugin-react'
import tailwindcss from '@tailwindcss/vite'
import { fileURLToPath } from 'node:url'

export default defineConfig({
  plugins: [react(), tailwindcss()],
  clearScreen: false,
  resolve: {
    dedupe: ['react', 'react-dom'],
    alias: Object.fromEntries(['@tauri-apps/api', 'react', 'react-dom', '@testing-library/react', '@testing-library/user-event', '@testing-library/jest-dom']
      .map(name => [name, fileURLToPath(new URL(`./node_modules/${name}`, import.meta.url))])),
  },
  build: { target: 'es2022', outDir: process.env.CHATGPT_UI_DIST ?? 'dist', emptyOutDir: false,
    rollupOptions: { input: process.env.CHATGPT_UI_PROTOTYPE === '1'
      ? { main: 'index.html', prototype: 'prototype.html' } : { main: 'index.html' } } },
  server: { fs: { allow: ['..'] } },
  test: { include: ['../tests/frontend/**/*.test.{ts,tsx}'] },
})
