import assert from 'node:assert/strict';
import fs from 'node:fs';
import vm from 'node:vm';
const html=fs.readFileSync(new URL('../OpenGarage/html/sta_options.html',import.meta.url),'utf8');
const context=vm.createContext({});
const start=html.indexOf('function cloud_endpoint(');
vm.runInContext(html.slice(start,html.indexOf("$('#btn_submit').click",start)),context);
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
