// Browser tests with every request intercepted; no controller or Internet access.
// NODE_PATH=/path/to/node_modules BROWSER_PATH=/path/to/chrome node tests/test_updater_notes.cjs
const assert=require('node:assert/strict'),fs=require('node:fs'),zlib=require('node:zlib');
const {chromium}=require('playwright');
let html=fs.readFileSync('OpenGarage/html/sta_update.html','utf8');
if(process.env.UPDATER_GENERATED==='1'){
 const header=fs.readFileSync('OpenGarage/htmls.h','utf8');
 const array=header.match(/const uint8_t sta_update_html_gz\[\] PROGMEM = \{([\s\S]*?)\};/)[1];
 html=zlib.gunzipSync(Buffer.from(array.match(/0x[0-9a-f]{2}/g).map(n=>parseInt(n,16)))).toString();
}
(async()=>{
 const browser=await chromium.launch({headless:true,executablePath:process.env.BROWSER_PATH});
 try{
  const page=await browser.newPage(),unexpected=[],errors=[];
  let data={body_html:'<h2>Changes</h2><ul><li><strong>Improved</strong> updater with <code>code</code>.</li></ul><p><a href="https://example.com/notes">Details</a></p>',body_text:'Changes'};
  page.on('pageerror',e=>errors.push(e.message));
  await page.route('**/*',route=>{
   const url=route.request().url();const json=d=>route.fulfill({contentType:'application/json',body:JSON.stringify(d)});
   if(url==='http://updater.test/')return route.fulfill({contentType:'text/html',body:html});
   if(url==='http://updater.test:8080/update/info')return json({product:'opengarage-stock',configured:true,version:126,target:'og-esp8266-4m-v1',base_url:'https://catalog.test'});
   if(url==='https://catalog.test/v1/manifest.json')return json({schema:2,releases:[{id:'1.2.6',version:126,targets:['og-esp8266-4m-v1'],descriptor:'/v1/releases/1.2.6/release.json',signature:'/v1/releases/1.2.6/release.sig'}]});
   if(url.startsWith('https://api.github.com/')){assert.equal(route.request().headers().accept,'application/vnd.github.full+json');return json(data);}
   unexpected.push(url);return route.abort();
  });
  await page.goto('http://updater.test/');await page.locator('#btn_notes').click();
  const notes=page.locator('#notes_body');await notes.locator('h2').waitFor();
  assert.equal(await notes.locator('li strong').textContent(),'Improved');
  assert.equal(await notes.locator('a').getAttribute('rel'),'noopener noreferrer');
  const render=d=>page.evaluate(d=>render_release_notes(document.getElementById('notes_body'),d),d);
  await render({body_text:'Fallback',body_html:'<script>window.pwned=1</script><img src="https://evil.test/image" alt="Image"><iframe src="https://evil.test/frame"></iframe><form><input></form><svg onload="window.pwned=1"></svg><p onclick="window.pwned=1" style="color:red">Safe <a href="javascript:alert(1)">bad</a><a href="http://evil.test/">http</a><a href="https://user:pass@evil.test/">credentials</a></p>'});
  assert.equal(await notes.locator('script,img,iframe,form,input,svg,a,[style],[onclick]').count(),0);
  assert.equal(await page.evaluate(()=>window.pwned),undefined);
  assert((await notes.textContent()).includes('Safe'));
  for(const body_html of ['<p>'+ 'x'.repeat(65537)+'</p>', '<div>'.repeat(40)+'deep'+'</div>'.repeat(40),'<span>x</span>'.repeat(4200)]){
   await render({body_text:'Fallback',body_html});assert.equal(await notes.textContent(),'Fallback');
  }
  await render({body:'**raw fallback**'});assert.equal(await notes.textContent(),'**raw fallback**');
  for(const width of [390,1280]){
   await page.setViewportSize({width,height:900});
   await render({body_html:'<h2>Changes</h2><ol start="3"><li>First</li></ol><blockquote>Note</blockquote><pre><code>'+ 'long '.repeat(100)+'</code></pre><table><tr><th>Feature</th><td>'+ 'wide'.repeat(200)+'</td></tr></table>'});
   assert.equal(await notes.locator('ol').getAttribute('start'),'3');
   assert.equal(await notes.locator('table').count(),1);
   assert(await notes.evaluate(e=>e.scrollWidth<=e.clientWidth));
   assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth));
   if(process.env.UPDATER_SCREENSHOTS)await page.screenshot({path:process.env.UPDATER_SCREENSHOTS+'/og-notes-'+width+'.png'});
  }
  await page.locator('#btn_notes_close').click();assert(!(await page.locator('#notes_dialog').isVisible()));
  assert.deepEqual(unexpected,[]);assert.deepEqual(errors,[]);
  console.log('PASS: release-note formatting, plain fallback, limits, safe links, inert resources and mobile/desktop layout');
 }finally{await browser.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});
