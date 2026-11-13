import assert from 'node:assert/strict';
import fs from 'node:fs';
import vm from 'node:vm';
const html=fs.readFileSync(new URL('../OpenGarage/html/sta_ui.html',import.meta.url),'utf8');
const context=vm.createContext({});
const start=html.indexOf('function cloud_endpoint(');
const end=html.indexOf("$('btn_submit').addEventListener('click'",start);
assert(start>=0 && end>start);
vm.runInContext(html.slice(start,end),context);
const hosts=['', 'blynk.openthings.io','ws.cloud.openthings.io','custom.example'];
const expectedMigration=[
  [true,true,false,false], // Disabled: preserve custom/OTC endpoints.
  [true,true,true,true],   // Blynk: even a custom endpoint belongs to retired Blynk.
  [false,false,false,false] // Existing OTC: preserve the configuration.
];
for(const mode of [0,1,2]) {
  for(const [index,host] of hosts.entries()) {
    const actual=context.cloud_endpoint({cld:mode,bdmn:host,bprt:8080});
    const migrate=expectedMigration[mode][index];
    assert.equal(actual.host,migrate?'ws.cloud.openthings.io':host);
    assert.equal(actual.port,migrate?80:8080);
  }
}
for(const field of ['auth','iftt','apwd','mqpw']) {
  assert(context.secret_edit_error({[field]:'replacement'},{[field]:true},false));
  assert.equal(context.secret_edit_error({[field]:'replacement'},{},true),'');
  assert.equal(context.secret_edit_error({},{[field]:true},false),'');
}
assert(context.secret_edit_error({},{auth:true},true));
assert.equal(context.secret_edit_error({},{mqpw:true},true),'');
console.log('Cloud options: legacy/disabled/OTC/custom endpoints and secret edit conflicts passed');

// Execute the actual visibility function with a small DOM boundary fake.
const flags={cld:1};const nodes={otcm:{value:'0'}};const visible={};
const ui=vm.createContext({eval_cb:id=>flags[id],$:id=>nodes[id],toggleAll:(selector,on)=>{visible[selector]=!!on;}});
const visibility=html.match(/function update_cld\(\) \{[^\n]+\}/)[0];
vm.runInContext(visibility,ui);ui.update_cld();assert.equal(visible['.cld'],true);assert.equal(visible['.cloud_custom'],false);
nodes.otcm.value='1';ui.update_cld();assert.equal(visible['.cloud_custom'],true);
flags.cld=0;ui.update_cld();assert.equal(visible['.cloud_custom'],false);assert.equal(visible['.cld'],false);
assert(html.includes("'&nossl=' + eval_cb('nossl')"));
console.log('Default/custom rows and secure opt-out field wiring passed');
