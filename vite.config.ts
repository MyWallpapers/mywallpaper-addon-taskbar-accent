import { readFileSync } from 'node:fs'
import { resolve } from 'node:path'
import { defineConfig, type Plugin } from 'vite'

function distributionEntry(): Plugin {
  return {
    name: 'mywallpaper-distribution-entry',
    generateBundle(_options, bundle) {
      const entry = bundle['assets/addon.js']
      if (!entry || entry.type !== 'chunk' || !entry.exports.includes('mount')) {
        this.error('The production add-on entry must export mount.')
      }
      const styles = Object.values(bundle)
        .filter((output) => output.type === 'asset' && output.fileName.endsWith('.css'))
        .map((output) => `    <link rel="stylesheet" href="./${output.fileName}">\n`)
        .join('')
      this.emitFile({
        type: 'asset',
        fileName: 'index.html',
        source: `<!doctype html>\n<html><head><meta charset="UTF-8">\n${styles}</head><body><script type="module" src="./assets/addon.js"></script></body></html>\n`,
      })
      this.emitFile({
        type: 'asset',
        fileName: 'THIRD_PARTY_NOTICES.md',
        source: readFileSync(resolve(process.cwd(), 'THIRD_PARTY_NOTICES.md'), 'utf8'),
      })
    },
  }
}

export default defineConfig(({ command }) => ({
  base: './',
  // Native preview output is intentionally served only by the development
  // server. Copying public/__mywallpaper into dist would publish local DLLs.
  publicDir: command === 'build' ? false : 'public',
  plugins: [distributionEntry()],
  build: {
    outDir: 'dist',
    emptyOutDir: true,
    rollupOptions: {
      preserveEntrySignatures: 'strict',
      input: 'src/main.ts',
      output: {
        entryFileNames: 'assets/addon.js',
        chunkFileNames: 'assets/chunk-[name]-[hash].js',
        assetFileNames: 'assets/[name]-[hash][extname]',
      },
    },
  },
  server: {
    host: '127.0.0.1',
    port: 5178,
    strictPort: true,
    cors: true,
  },
}))
