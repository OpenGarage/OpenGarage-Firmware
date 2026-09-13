import assert from 'node:assert/strict';
import fs from 'node:fs';
import vm from 'node:vm';

const html = fs.readFileSync(new URL('../OpenGarage/html/sta_options.html', import.meta.url), 'utf8');
assert(html.indexOf("id='ckey'") < html.indexOf("id='btn_regenerate_id'"));
assert(html.indexOf("id='btn_regenerate_id'") < html.indexOf("id='dkey'"));
const start = html.indexOf("$('#btn_regenerate_id').click(");
const end = html.indexOf('\nfunction bc(', start);
assert(start >= 0 && end > start);
async function run(key, accepted, result) {
  let handler, request, message, disabled;
  const button = {prop: (_, value) => { disabled = value; }};
  const context = {
    $: selector => selector === '#btn_regenerate_id' ? {click: fn => { handler = fn; }} :
      selector === '#dkey' ? {val: () => key} : button,
    confirm: text => { assert.match(text, /restart.*WiFi/s); return accepted; },
    show_msg: text => { message = text; },
    fetch: async (url, options) => { request = {url, options}; return {json: async () => result}; }
  };
  vm.runInNewContext(html.slice(start, end), context);
  handler.call(button, {preventDefault() {}});
  await new Promise(resolve => setImmediate(resolve));
  return {request, message, disabled};
}
assert.equal((await run('', true)).request, undefined);
assert.equal((await run('test-key', false)).request, undefined);
const success = await run('test-key', true, {result: 1, message: 'Restarting'});
assert.equal(success.request.url, 'secplus2/regenerate-id');
assert.equal(success.request.options.method, 'POST');
assert.equal(success.request.options.body, 'test-key');
assert.equal(success.request.options.headers['Content-Type'], 'text/plain;charset=UTF-8');
assert.equal(success.request.options.headers['X-OG-Device-Key'], undefined);
const specialKey = ' spaces & + % = é ';
assert.equal((await run(specialKey, true, {result:1})).request.options.body, specialKey);
assert.equal(success.disabled, true);
assert.equal(success.message, 'Restarting');
assert.equal((await run('bad-key', true, {result: 2})).disabled, false);
assert.equal((await run('test-key', true, {result: 0, message: 'Busy'})).message, 'Busy');
console.log('Client-ID button: missing key, cancellation, POST, success and rejection checks passed');
