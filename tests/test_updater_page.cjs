// No devices contacted. Execute the real page logic with controlled network/UI.
const fs=require('fs'),vm=require('vm'),assert=require('assert'),crypto=require('crypto');
const html=fs.readFileSync('OpenGarage/html/sta_update.html','utf8');
const apHtml=fs.readFileSync('OpenGarage/html/ap_update.html','utf8');
assert(apHtml.includes('<section id="online_section" hidden>'));
assert.equal(html,apHtml.replace('const apMode=true;','const apMode=false;').replace('id="online_section" hidden','id="online_section"'));
assert(!/<(?:script|link)[^>]+(?:src|href)=/i.test(html));
const script=html.match(/<script>([\s\S]*?)<\/script>/)[1].replace(/load_catalog\(\);\s*$/,'');
function harness(ap=false){
 const elements={};const el=id=>elements[id]??=( {value:'',files:[],disabled:false,hidden:false,textContent:'',showModal(){this.open=true;},close(){this.open=false;},addEventListener(){},replaceChildren(...x){this.options=x;this.value=x.length?'0':'';}} );
 const calls=[];let consent=true,fail=false,corrupt=false;
 const image=Buffer.alloc(2048,0x5a);const hash=crypto.createHash('sha256').update(image).digest('hex');
 const release={id:'1.2.6',version:126,targets:['og-esp8266-4m-v1'],descriptor:'/v1/releases/1.2.6/release.json',signature:'/v1/releases/1.2.6/release.sig'};
 const d={schema:1,product:'opengarage-stock',id:'1.2.6',version:126,targets:{'og-esp8266-4m-v1':{file:'/v1/releases/1.2.6/og.bin',size:image.length,min_flash:4194304,sha256:hash}}};
 const ctx=vm.createContext({document:{getElementById:el,createElement:()=>({})},window:{location:{hostname:'device'},addEventListener(){}},confirm:()=>consent,TextEncoder,TextDecoder,URL,Blob,FormData,Uint8Array,Uint32Array,console});
 vm.runInContext(ap?script.replace('const apMode=false;','const apMode=true;'):script,ctx);
 ctx.fakeRequest=async(url,o={})=>{calls.push({url,...o});
  if(url.endsWith('/info'))return JSON.stringify({product:'opengarage-stock',configured:true,version:126,target:'og-esp8266-4m-v1',base_url:'https://firmware.opengarage.io'});
  if(url.endsWith('manifest.json'))return JSON.stringify({schema:2,releases:[release]});
  if(url.endsWith('release.json'))return new TextEncoder().encode(JSON.stringify(d)).buffer;
  if(url.endsWith('release.sig'))return 'aa'.repeat(70);
  if(url.endsWith('og.bin')){const b=Buffer.from(image);if(corrupt)b[99]^=1;return b.buffer.slice(b.byteOffset,b.byteOffset+b.length);}
  if(url.endsWith('/prepare'))return JSON.stringify({result:1,upload_token:'test-token'});
  if(url.endsWith('/upload')){if(fail)throw Error('Update not confirmed');return JSON.stringify({result:1});}
  throw Error('unexpected request '+url);
 };
 vm.runInContext('request=fakeRequest',ctx);el('dkey').value='private-key';
 return {ctx,el,calls,image,d,release,setConsent:v=>consent=v,setFail:v=>fail=v,setCorrupt:v=>corrupt=v,run:s=>vm.runInContext(s,ctx)};
}
(async()=>{
 let t=harness();for(const n of [0,1,55,56,64,1000,595072]){const bytes=crypto.randomBytes(n);t.ctx.bytes=bytes;assert.equal(t.run('sha256_bytes(bytes)'),crypto.createHash('sha256').update(bytes).digest('hex'));}
 await t.run('load_catalog()');await t.run('install_release()');
 const posts=t.calls.filter(x=>x.method==='POST');assert.equal(posts.length,2);assert(posts[0].url.endsWith('/prepare'));assert.equal(posts[0].headers['X-OG-Allow-Downgrade'],'1');assert.equal(posts[1].headers['X-OG-Upload-Token'],'test-token');
 for(const c of t.calls){assert(!c.url.includes('private-key'));if(c.url.startsWith('https:'))assert.equal(c.headers,undefined);}
 assert.equal(t.run('locked'),true);assert(t.el('msg').textContent.includes('successful'));
 await t.run('install_release()');assert.equal(t.calls.filter(x=>x.method==='POST').length,2);
 t=harness();t.setCorrupt(true);await t.run('load_catalog()');await t.run('install_release()');assert.equal(t.calls.filter(x=>x.method==='POST').length,0);assert(t.el('msg').textContent.includes('checksum'));
 t=harness();t.setConsent(false);await t.run('load_catalog()');const before=t.calls.length;await t.run('install_release()');assert.equal(t.calls.length,before);
 t=harness();t.setFail(true);await t.run('load_catalog()');await t.run('install_release()');await t.run('install_release()');assert.equal(t.calls.filter(x=>x.url.endsWith('/upload')).length,1);assert.equal(t.run('locked'),true);
 t=harness();t.release.targets=['different'];await t.run('load_catalog()');assert.equal(t.run('releases.length'),0);assert(t.el('btn_install').disabled);
 t=harness();t.d.product='wrong';await t.run('load_catalog()');await t.run('install_release()');assert.equal(t.calls.filter(x=>x.method==='POST').length,0);
 // Explicit header-stage refusal permits correcting the key and retrying manually.
 for(const status of [401,409]){
  t=harness();t.el('file').files=[new File([t.image],'og.bin')];
  t.ctx.refusal=async()=>{const e=Error('refused');e.status=status;throw e;};
  t.run('request=refusal');await t.run('manual_update()');
  assert.equal(t.run('locked'),false);assert.equal(t.el('dkey').disabled,false);
  assert(t.el('msg').textContent.includes(status===401?'device key':'busy'));
  t.ctx.accepted=async()=>JSON.stringify({result:1});t.run('request=accepted');
  await t.run('manual_update()');assert(t.el('msg').textContent.includes('successful'));
 }
 t=harness(true);await t.run('load_catalog()');assert.equal(t.calls.length,1);assert(t.calls[0].url.endsWith('/info'));assert(t.el('btn_notes').disabled);
 t=harness();await t.run('load_catalog()');t.ctx.fakeNotes=async()=>JSON.stringify({body_text:'<script>not HTML</script> notes'});t.run('request=fakeNotes');await t.run('open_notes()');assert.equal(t.el('notes_body').textContent,'<script>not HTML</script> notes');assert(t.el('notes_dialog').open);t.run('close_notes()');assert(!t.el('notes_dialog').open);
 console.log('Updater page: SHA256, target filtering, corruption, cancellation, headers, no credential leakage, single upload/no retry PASS');
})().catch(e=>{console.error(e);process.exitCode=1;});
