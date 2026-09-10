import assert from 'node:assert/strict'
import { readFile } from 'node:fs/promises'
import ts from 'typescript'

const nativeSource = await readFile(new URL('../native/hooks/taskbar.wh.cpp', import.meta.url), 'utf8')
const hookId = nativeSource.match(/^\/\/\s+@id\s+(\S+)/m)?.[1]
assert.ok(hookId, 'the native module must declare its Windhawk identity')
const source = await readFile(new URL('../src/main.ts', import.meta.url), 'utf8')
const compiled = ts.transpileModule(source.replace("import './styles.css'", ''), {
  compilerOptions: { module: ts.ModuleKind.ESNext, target: ts.ScriptTarget.ES2022 },
}).outputText
const { mount } = await import(`data:text/javascript;base64,${Buffer.from(compiled).toString('base64')}`)
const card = { dataset: {}, title: '' }
const detail = { textContent: '' }
let listener
let unsubscribed = false
const events = []
const dispose = mount({
  layer: {
    root: { classList: { add() {} }, innerHTML: '', querySelector: (selector) => selector === '.status' ? card : detail },
    native: { hooks: { onStateChange(callback) { listener = callback; callback([]); return () => { unsubscribed = true } } } },
    bus: { emit: (topic, payload) => events.push({ topic, payload }) },
  },
  runtime: { instance: { canonical: true } },
})
assert.equal(card.dataset.state, 'disabled')
listener([{ hookId, state: 'active' }])
assert.equal(card.dataset.state, 'active', 'the UI must recognize the actual native module identity')
assert.equal(detail.textContent, 'Live · shared across every Windows taskbar')
assert.equal(events.at(-1).payload.state, 'active')
listener([{ hookId, state: 'degraded', cause: 'Engine stopped.', action: 'Retry.' }])
assert.equal(card.dataset.state, 'degraded')
assert.equal(detail.textContent, 'Engine stopped. Retry.')
listener([{ hookId: 'unrelated-module', state: 'active' }])
assert.equal(card.dataset.state, 'disabled', 'another module must not imply that Taskbar Accent is active')
dispose()
assert.equal(unsubscribed, true)
console.log('native status contract verified')
