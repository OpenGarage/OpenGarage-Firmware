import assert from 'node:assert/strict';
import fs from 'node:fs';
import vm from 'node:vm';
const html=fs.readFileSync(new URL('../OpenGarage/html/sta_ui.html',import.meta.url),'utf8');
assert(!html.includes('lightflip_status') && !html.includes('lockflip_status'));
assert(!html.includes('jquery'), 'station UI must not load jQuery');
const start=html.indexOf('var switchPoll =');
const code=html.slice(start,html.indexOf("['light', 'lock'].forEach(name => $(name + 'flip').addEventListener('change'",start));
assert(start>=0 && code.length>0);
function harness() {
 const elements={},requests=[],timers=new Map(); let id=0;
 const element=()=>({checked:false,disabled:false,textContent:'',classList:{toggle(){}}});
 const $=key=>elements[key] ||= element();
 const context=vm.createContext({
  setTimeout:(fn,ms)=>{assert.equal(ms,4000);timers.set(++id,fn);return id;},
  clearTimeout:key=>timers.delete(key),
  do_action_command:(command,callback)=>requests.push({command,callback}),
  showMsg:(el,text)=>{el.textContent=text;},
  $, msg:$('msg')
 });
 vm.runInContext(code,context);
 const poll=(light,lock=0,valid=1)=>{context.switchPoll++;context.observe_switches({light,lock,light_lock_valid:valid},context.switchPoll);};
 // A user toggle: the native checkbox changes, then its change handler runs.
 const click=(name,value)=>{elements[name+'flip'].checked=value==='1';context.request_switch(name);};
 const value=name=>elements[name+'flip'].checked?'1':'0';
 poll(0);
 return {context,elements,requests,timers,poll,click,value};
}
let h=harness();
h.click('light','1');
assert.equal(h.requests.length,1);
assert.equal(h.requests[0].command,'light=toggle');
assert.equal(h.elements.lightflip.disabled,true);
assert.equal(h.elements.msg.textContent,'');
h.requests[0].callback(true); // HTTP acceptance alone cannot clear pending
assert(h.context.switchState.light.pending);
h.poll(0); // old state from a new poll must not flip selection back
assert.equal(h.value('light'),'1');
h.click('light','0');assert.equal(h.requests.length,1);
assert.equal(h.value('light'),'1');
h.poll(1);assert.equal(h.context.switchState.light.pending,null);
assert.equal(h.elements.lightflip.disabled,false);
assert.equal(h.timers.size,0);
assert.equal(h.elements.msg.textContent,'');
h.elements.msg.textContent='Unrelated device message';h.poll(1);
assert.equal(h.elements.msg.textContent,'Unrelated device message');

h=harness();h.click('light','1');h.click('lock','1');
assert.equal(h.elements.msg.textContent,'');
assert.equal(h.requests.length,2);h.poll(1,0);
assert.equal(h.context.switchState.light.pending,null);
assert(h.context.switchState.lock.pending);
assert.equal(h.elements.msg.textContent,'');
for(const timer of [...h.timers.values()])timer();
assert.equal(h.value('lock'),'0');
assert.equal(h.elements.lockflip.disabled,false);
assert.equal(h.requests.length,2); // timeout never retries
assert.match(h.elements.msg.textContent,/Lock: Not confirmed/);

h=harness();h.context.switchPoll=10;h.click('light','1');
h.context.observe_switches({light:1,lock:0},10); // in-flight pre-click poll
assert(h.context.switchState.light.pending);
h.context.observe_switches({light:1,lock:0,light_lock_valid:0},11);
assert(h.context.switchState.light.pending); // invalid state cannot confirm
for(const timer of [...h.timers.values()])timer();
assert.equal(h.elements.lightflip.disabled,true);
h.context.observe_switches({light:1,lock:0,light_lock_valid:1},12);
assert.equal(h.value('light'),'1');
h.context.observe_switches({light:0,lock:0},11); // out-of-order response ignored
assert.equal(h.value('light'),'1');

h=harness();h.click('light','1');const first=h.requests[0];
first.callback(false);assert.equal(h.context.switchState.light.pending,null);
assert.equal(h.value('light'),'0');
h.click('light','1');first.callback(false); // late old error cannot cancel new request
assert(h.context.switchState.light.pending);
h.click('lock','0'); // no change from the reported state: nothing is sent
assert.equal(h.requests.length,2);
console.log('Pending switches: hold, confirmation, duplicates, timeout, rejection, invalid/stale feedback, independent controls and no retries passed');
