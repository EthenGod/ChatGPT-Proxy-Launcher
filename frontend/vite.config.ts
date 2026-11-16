import { defineConfig } from 'vitest/config'
import react from '@vitejs/plugin-react'
import tailwindcss from '@tailwindcss/vite'

export default defineConfig({
  plugins: [react(), tailwindcss()],
  clearScreen: false,
  build: { target: 'es2022', outDir: process.env.CHATGPT_UI_DIST ?? 'dist', emptyOutDir: false },
  server: { fs: { allow: ['..'] } },
  test: { include: ['../tests/frontend/**/*.test.ts'] },
})
