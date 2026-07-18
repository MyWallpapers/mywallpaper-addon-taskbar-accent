import assert from 'node:assert/strict'
import { readFile } from 'node:fs/promises'
import { resolve } from 'node:path'
import { pathToFileURL } from 'node:url'

const htmlPath = resolve('dist/index.html')
const html = await readFile(htmlPath, 'utf8')
const entryPath = html.match(/<script[^>]+type="module"[^>]+src="([^"]+)"/u)?.[1]
assert.ok(entryPath, 'dist/index.html must reference one module entry')
const entry = await import(new URL(entryPath, pathToFileURL(htmlPath)).href)
assert.equal(typeof entry.mount, 'function', 'the production entry must export a callable mount')

const notices = await readFile(resolve('dist/THIRD_PARTY_NOTICES.md'), 'utf8')
assert.match(notices, /Windows 11 Taskbar Styler 1\.7/u, 'the distributed bundle must retain GPL attribution')
