import { defineConfig } from 'vitest/config'
import { normalizePath } from 'vite'
import react from '@vitejs/plugin-react'
import tailwindcss from '@tailwindcss/vite'
import { fileURLToPath } from 'node:url'
import registry from '../locales/languages.json' with { type: 'json' }

const localeModule = 'virtual:setup-locales'
const servedDirectories = ['.', '../locales']
if (process.env.VITEST === 'true') servedDirectories.push('../tests/frontend')

export default defineConfig({
  plugins: [react(), tailwindcss(), {
    name: 'registered-setup-locales',
    resolveId(id) { if (id === localeModule) return '\0' + localeModule },
    load(id) {
      if (id !== '\0' + localeModule) return
      // Import only published catalogs. Drafts and other local JSON stay out of the executable.
      const imports = registry.languages.map(({ tag }, index) => {
        const path = normalizePath(fileURLToPath(new URL(`../locales/${tag}.json`, import.meta.url)))
        return `import catalog${index} from ${JSON.stringify(path)}`
      })
      const names = registry.languages.map((_, index) => `catalog${index}`)
      return [...imports, `export default [${names.join(', ')}]`].join('\n')
    },
  }],
  clearScreen: false,
  resolve: {
    dedupe: ['react', 'react-dom'],
    alias: Object.fromEntries(['@tauri-apps/api', 'react', 'react-dom', '@testing-library/react', '@testing-library/user-event', '@testing-library/jest-dom']
      .map(name => [name, fileURLToPath(new URL(`./node_modules/${name}`, import.meta.url))])),
  },
  build: { target: 'es2022', outDir: process.env.CHATGPT_UI_DIST ?? 'dist', emptyOutDir: false,
    rollupOptions: { input: process.env.CHATGPT_UI_PROTOTYPE === '1'
      ? { main: 'index.html', prototype: 'prototype.html' } : { main: 'index.html' } } },
  server: { fs: { allow: servedDirectories.map(dir => fileURLToPath(new URL(dir, import.meta.url))) } },
  test: { include: ['../tests/frontend/**/*.test.{ts,tsx}'] },
})
