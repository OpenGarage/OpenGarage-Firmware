import assert from 'node:assert/strict';
import fs from 'node:fs';
import vm from 'node:vm';

const html = fs.readFileSync(new URL('../OpenGarage/html/sta_ui.html', import.meta.url), 'utf8');
const source = fs.readFileSync(new URL('../OpenGarage/main.cpp', import.meta.url), 'utf8');
const optionsJson = source.slice(source.indexOf('void sta_options_fill_json('), source.indexOf('void on_sta_options('));
assert.match(optionsJson, /F\(",.*s2id/);
assert.doesNotMatch(optionsJson, /F\(",.*secplus2_client_id/);
assert.match(source, /otf->on\("\/s2id_gen", on_regenerate_secplus2_id, OTF::HTTP_POST\)/);
assert.doesNotMatch(source, /otf->on\("\/secplus2\/regenerate-id"/);
assert.match(html, /jd\.s2id \|\| 'Unavailable'/);
assert(!html.includes('jquery'), 'station UI must not load jQuery');
// Row order: Security+ version, Panel Emulate (1.0), Client ID (2.0), then the door threshold.
assert(html.indexOf('data-row="secv"') < html.indexOf('data-row="pem"'));
assert(html.indexOf('data-row="pem"') < html.indexOf('data-row="cid"'));
assert(html.indexOf('data-row="cid"') < html.indexOf('id="dth"'));
assert.match(html, /<span data-row="cid" hidden><button[^>]*id="btn_regenerate_id"/);
const visibility = html.match(/function update_security_details\(\) \{[\s\S]*?\n\}/)[0];
for (const capable of [false,true]) for (const saved of [false,true]) {
  for (const selected of ['0','1','2',undefined]) {
    const rows = {}; const button = {disabled: null};
    vm.runInNewContext(visibility+';update_security_details();', {
      security_capable:capable, saved_secplus2:saved,
      checked_val: name => { assert.equal(name, 'secv'); return selected; },
      show_row: (name, show) => { rows[name] = show; },
      $: id => { assert.equal(id, 'btn_regenerate_id'); return button; }
    });
    assert.equal(rows.pem, capable && selected==='1');
    assert.equal(rows.cid, capable && selected==='2');
    assert.equal(button.disabled, !saved);
  }
}
assert.match(html, /saved_secplus2 = !!jd\.has_swrx && jd\.secv === 2;/);
// Panel Emulate: exactly one radio of the pair is selected from the saved value.
const pemLine = html.match(/cbt\('pem' \+ \(jd\.pem === 1 \? 1 : 0\)\);/);
assert(pemLine);
for (const pem of [0,1,undefined]) {
  let selected;
  vm.runInNewContext(pemLine[0], {jd:{pem}, cbt: id => { selected = id; }});
  assert.equal(selected, pem===1 ? 'pem1' : 'pem0');
}
assert.match(html, /<input type="radio" name="pem" id="pem0" value="0" checked>/); // Auto before /jo arrives
const start = html.indexOf("$('btn_regenerate_id').addEventListener('click',");
const end = html.indexOf('\nvar comm;', start);
assert(start >= 0 && end > start);
async function run(key, accepted, result) {
  let handler, request, message;
  const button = {disabled: false, addEventListener: (type, fn) => { assert.equal(type, 'click'); handler = fn; }};
  const context = {
    $: id => { assert.equal(id, 'btn_regenerate_id'); return button; },
    okey: {value: key},
    confirm: text => { assert.match(text, /restart.*WiFi/s); return accepted; },
    show_msg: text => { message = text; },
    fetch: async (url, options) => { request = {url, options}; return {json: async () => result}; }
  };
  vm.runInNewContext(html.slice(start, end), context);
  handler.call(button, {preventDefault() {}});
  await new Promise(resolve => setImmediate(resolve));
  return {request, message, disabled: button.disabled};
}
assert.equal((await run('', true)).request, undefined);
assert.equal((await run('test-key', false)).request, undefined);
const success = await run('test-key', true, {result: 1, message: 'Restarting'});
assert.equal(success.request.url, 's2id_gen');
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
console.log('Security+ options: protocol visibility, one selected panel setting, and client-ID POST checks passed');
