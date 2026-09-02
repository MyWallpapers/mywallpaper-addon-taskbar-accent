import assert from 'node:assert/strict'
import { access, readdir, readFile } from 'node:fs/promises'
import { constants } from 'node:fs'
import { resolve } from 'node:path'

async function exists(path) {
  try {
    await access(path, constants.F_OK)
    return true
  } catch {
    return false
  }
}

const entryPath = resolve('dist/assets/addon.js')
const entry = await readFile(entryPath, 'utf8')
assert.ok(entry.length > 0, 'the production module entry must exist and not be empty')
assert.match(entry, /\bmount\b/u, 'the production entry must contain the mount export')
assert.equal(await exists(resolve('dist/index.html')), false, 'the Canvas runtime must not depend on an obsolete HTML loader')

const assets = await readdir(resolve('dist/assets'))
for (const stylesheet of assets.filter((fileName) => fileName.endsWith('.css'))) {
  assert.ok(
    entry.includes(JSON.stringify('./' + stylesheet)),
    'the production entry must load emitted stylesheet ' + stylesheet,
  )
}

const notices = await readFile(resolve('dist/THIRD_PARTY_NOTICES.md'), 'utf8')
assert.match(notices, /Windows 11 Taskbar Styler 1\.7/u, 'the distributed bundle must retain GPL attribution')

console.log('dist entry verified')
