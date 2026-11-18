import fs from 'node:fs';
import vm from 'node:vm';
import assert from 'node:assert/strict';
const html=fs.readFileSync('OpenGarage/html/ap_home.html','utf8');
const script=html.replace('<!--#include _cloud_endpoint.js-->', fs.readFileSync('OpenGarage/html/_cloud_endpoint.js','utf8')).match(/<script>([\s\S]*?)<\/script>/)[1];
const elements=new Map();
function element(id){if(!elements.has(id))elements.set(id,{value:'',checked:false,disabled:false,hidden:true,innerHTML:'',addEventListener(event,fn){this[event]=fn;}});return elements.get(id);}
const requests=[];
const context={document:{getElementById:element,querySelector:()=>({value:'0'}),querySelectorAll:()=>[element('dkey'),element('ssid')]},setTimeout(){},clearTimeout(){},setInterval(){return 1;},clearInterval(){},console,
 XMLHttpRequest:class {constructor(){requests.push(this);} open(method,url){this.method=method;this.url=url;}send(){}},window:{},encodeURIComponent};
vm.createContext(context);vm.runInContext(script,context);
function reply(req,data){req.readyState=4;req.status=200;req.responseText=JSON.stringify(data);req.onreadystatechange();}
context.setupReady=true;context.keyRequired=true;element('ssid').value='network';
context.sf();assert.equal(requests.length,0); // no key, no request
const key='old +&%é key';element('dkey').value=key;
context.sf();assert.equal(new URL('http://og/'+requests[0].url).searchParams.get('dkey'),key);
reply(requests[0],{result:2});assert.equal(element('butt').disabled,false);assert.equal(element('dkey').disabled,false);
assert.match(element('msg').innerHTML,/existing device key/);
context.sf();reply(requests[1],{result:1});
element('dkey').value='edited while connecting';context.tryConnect();
assert.equal(new URL('http://og/'+requests[2].url).searchParams.get('dkey'),key); // saved request key
reply(requests[2],{result:2});assert.equal(element('dkey').disabled,false);assert.equal(element('butt').disabled,false);
assert.equal(element('key_section').hidden,false);
// WiFi names must never be interpreted as HTML on a page collecting credentials.
assert.ok(!script.includes('cell1.innerHTML'));
assert.ok(script.includes('document.createTextNode(jd.ssids[i])'));
assert.ok(script.includes('radio.value=jd.ssids[i]'));
assert.ok(script.includes('showKeyState(jd.key_required === 1)'));
const fetches=[];let detectReply={result:2};
context.confirm=()=>true;context.AbortController=AbortController;
context.fetch=async(url,opts)=>{fetches.push({url,opts});return {json:async()=>detectReply};};
async function detect(){element('btn_autodetect').click.call(element('btn_autodetect'));await new Promise(resolve=>setImmediate(resolve));}
context.keyRequired=true;element('dkey').value='';await detect();assert.equal(fetches.length,0);
element('dkey').value=key;await detect();
assert.equal(new URL('http://og/'+fetches[0].url).searchParams.get('dkey'),key);
assert.equal(fetches[0].opts.cache,'no-store');assert.equal(element('btn_autodetect').disabled,false);
assert.match(element('msg').innerHTML,/existing device key/);
detectReply={result:48};await detect();assert.match(element('msg').innerHTML,/Device busy/);
context.keyRequired=false;element('dkey').value='';detectReply={version:0};await detect();
assert.equal(new URL('http://og/'+fetches.at(-1).url).searchParams.get('dkey'),'');
assert.match(element('msg').innerHTML,/No supported protocol/);
console.log('PASS: Detect authorization/busy/first-use, AP key prompt, encoded requests, polling snapshot, unauthorized retry and text-only SSIDs');

context.showKeyState(false);assert.equal(element('butt').disabled,true);
let saves=0;context.fetch=async(url,opts)=>{assert.equal(url,'setup_key');assert.equal(opts.method,'POST');assert.equal(opts.body,'a brand new key');++saves;return {json:async()=>({result:1})};};
element('new_key').value='short';await element('save_key').click.call(element('save_key'));assert.equal(saves,0);
element('new_key').value=element('confirm_key').value='a brand new key';
await element('save_key').click.call(element('save_key'));assert.equal(saves,1);assert.equal(context.setupReady,true);assert.equal(element('dkey').value,'a brand new key');
assert.equal(element('new_key').value,'');
context.crypto={getRandomValues(bytes){bytes.fill(0);return bytes;}};
element('generate_key').click();assert.equal(element('new_key').value.length,20);assert.ok(context.validNewKey(element('new_key').value));
assert.equal(element('new_key').value,element('confirm_key').value);
console.log('PASS: first-use gate, validation, POST key save and browser generation');

context.showKeyState(false);element('new_key').value=element('confirm_key').value='a brand new key';
context.fetch=async()=>{throw Error('lost reply');};await element('save_key').click.call(element('save_key'));
assert.equal(context.setupReady,false);assert.equal(element('new_key').value,'a brand new key');assert.equal(element('save_key').disabled,false);
assert.match(element('msg').innerHTML,/retry the same key/);
assert.ok(!script.includes('Math.random'));
console.log('PASS: uncertain save retains chosen key and permits safe retry');
context.fetch=async()=>({json:async()=>({result:2})});
await element('save_key').click.call(element('save_key'));
assert.equal(context.setupReady,true);assert.equal(element('key_section').hidden,false);
assert.match(element('msg').innerHTML,/already been set/);assert.match(element('msg').innerHTML,/factory-reset/);

// Cloud-off never sends stale Custom controls to setup.
context.setupReady=true;context.keyRequired=true;element('dkey').value=key;element('otc').checked=false;element('otcm').value='1';
element('bdmn').value='http://stale hidden value';element('bprt').value='invalid';
context.sf();const off=new URL('http://og/'+requests.at(-1).url).searchParams;
for(const name of ['cld','auth','otcm','nossl','bdmn','bprt'])assert.equal(off.has(name),false,name);
console.log('PASS: cloud-off setup omits stale Custom controls');
