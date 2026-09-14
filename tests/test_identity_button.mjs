import assert from 'node:assert/strict';
import fs from 'node:fs';
import vm from 'node:vm';

const html = fs.readFileSync(new URL('../OpenGarage/html/sta_options.html', import.meta.url), 'utf8');
const source = fs.readFileSync(new URL('../OpenGarage/main.cpp', import.meta.url), 'utf8');
const optionsJson = source.slice(source.indexOf('void sta_options_fill_json('), source.indexOf('void on_sta_options('));
assert.match(optionsJson, /F\(",.*s2id/);
assert.doesNotMatch(optionsJson, /F\(",.*secplus2_client_id/);
assert.match(source, /otf->on\("\/s2id_gen", on_regenerate_secplus2_id, OTF::HTTP_POST\)/);
assert.doesNotMatch(source, /otf->on\("\/secplus2\/regenerate-id"/);
assert.match(html, /jd\.s2id \|\| 'Unavailable'/);
assert(html.indexOf("id='secv'") < html.indexOf("id='tr_panel_emu'"));
assert(html.indexOf("id='tr_panel_emu'") < html.indexOf("id='tr_client_id'"));
assert(html.indexOf("id='tr_client_id'") < html.indexOf("id='dth'"));
assert.match(html, /id='tr_client_id' style='display:none;'[\s\S]*?id='btn_regenerate_id'/);
const visibility = html.match(/function update_security_details\(\)\{[\s\S]*?\n\}/)[0];
for (const capable of [false,true]) for (const saved of [false,true]) {
  for (const selected of ['0','1','2',undefined]) {
    let panelVisible, clientVisible, buttonState;
    vm.runInNewContext(visibility+';update_security_details();', {
      security_capable:capable, saved_secplus2:saved,
      $: selector => selector==='#tr_panel_emu' ? {toggle:v=>{panelVisible=v;}} :
        selector==='#tr_client_id' ? {toggle:v=>{clientVisible=v;}} :
        selector==='#btn_regenerate_id' ? {button:v=>{buttonState=v;}} : {val:()=>selected}
    });
    assert.equal(panelVisible,capable && selected==='1');
    assert.equal(clientVisible,capable && selected==='2');
    assert.equal(buttonState,saved ? 'enable' : 'disable');
  }
}
assert.match(html, /saved_secplus2=!!jd\.has_swrx && jd\.secv===2;/);
const radioStart = html.indexOf("$('input[name=\"pem\"]').prop('checked', false);");
const radioEnd = html.indexOf('if(jd.has_swrx)', radioStart);
assert(radioStart>=0 && radioEnd>radioStart);
for (const pem of [0,1,undefined]) {
  const checked = {pem0:true,pem1:false}; // Auto starts checked in the HTML.
  vm.runInNewContext(html.slice(radioStart,radioEnd), {
    jd:{pem},
    $: selector => selector==='input[name="pem"]' ? {
      prop: (_,value) => {checked.pem0=value; checked.pem1=value;}, checkboxradio:()=>{}
    } : {prop:(_,value)=>{checked[selector.slice(1)]=value;}}
  });
  assert.deepEqual(checked,pem===1 ? {pem0:false,pem1:true} : {pem0:true,pem1:false});
}
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
