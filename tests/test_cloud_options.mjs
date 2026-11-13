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

// Execute the actual homepage label branch, including strict-time waiting.
const labelStart=html.indexOf("$('lbl_cld').textContent = jd.cld");
const labelEnd=html.indexOf('    dev_name = jd.name;',labelStart);
const labels={textContent:''};const labelContext=vm.createContext({$:()=>labels,jd:{}});
function cloudLabel(jd){labelContext.jd=jd;vm.runInContext(html.slice(labelStart,labelEnd),labelContext);return labels.textContent;}
assert.match(cloudLabel({cld:2,clds:1,cloud_secure:1,cloud_tls_state:1,time_valid:0}),/waiting for time/);
assert.match(cloudLabel({cld:2,clds:1,cloud_secure:1,cloud_tls_state:2,time_valid:0}),/secure connection unavailable/);
assert.match(cloudLabel({cld:2,clds:3,cloud_secure:1,time_valid:1}),/connected/);
assert(!cloudLabel({cld:2,clds:1,cloud_secure:0,time_valid:0}).includes('waiting for time'));
console.log('PASS: homepage exposes strict-time waiting and secure-resource failure');
