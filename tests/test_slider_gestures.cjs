// Real-widget regression: all commands are mocked, assets served on loopback.
// Usage: node tests/test_slider_gestures.cjs jquery-1.9.1.min.js jquery.mobile-1.3.1.js
const fs=require('fs'),http=require('http'),os=require('os'),path=require('path');
const {execFile}=require('child_process');
const source=fs.readFileSync(path.join(__dirname,'../OpenGarage/html/sta_home.html'),'utf8');
const logic=source.slice(source.indexOf('var switchPoll='),source.indexOf("$('#dkey').on('input change'",source.indexOf('var switchPoll=')));
if(!process.argv[2]||!process.argv[3])throw Error('Supply the exact jQuery and jQuery Mobile asset files');
const assets={'/jquery.js':fs.readFileSync(process.argv[2]),'/mobile.js':fs.readFileSync(process.argv[3])};
const body=`<div data-role="page"><div data-role="content"><label id="msg"></label><div class="lightlock-ctrl"><select id="lightflip" data-role="slider"><option value="0">Off</option><option value="1">On</option></select><select id="lockflip" data-role="slider"><option value="0">Unlocked</option><option value="1">Locked</option></select></div></div></div>`;
const script=`var requests=[];function do_action_command(c,callback){requests.push({c:c,callback:callback});}
${logic}
bind_switch_gestures();
$(function(){setTimeout(function(){try{
 function check(ok,message){if(!ok)throw Error(message);}
 $('#lightflip,#lockflip').slider();
 var w=$('#lightflip').data('mobile-slider');
 w.slider.css({width:'160px',height:'32px',display:'block'});
 function poll(value,valid){observe_switches({light:value,lock:0,light_lock_valid:valid===undefined?1:valid},++switchPoll);}
 function mouse(type,value,which){
  var x=w.slider.offset().left+(value?w.slider.width()-1:1);
  (type==='vmousedown'?w.slider:$(document)).trigger($.Event(type,{which:which===undefined?1:which,pageX:x,pageY:10}));
 }
 function reset(){
  if(switchState.light.pending)finish_switch('light',switchState.light.pending,'');
  cancel_switch_gesture('light');poll(0);
 }
 poll(0);
 mouse('vmousedown',1);
 check(requests.length===0 && w.dragging && !w.options.disabled,'mousedown must not dispatch or disable');
 poll(0);check(w.dragging && !w.options.disabled,'poll must not disable active gesture');
 mouse('vmouseup',1);
 check(requests.length===1 && !w.dragging && w.options.disabled,'release must dispatch once after drag cleanup');
 mouse('vmousedown',0);mouse('vmouseup',0);
 check(requests.length===1 && !w.dragging,'pending click must not dispatch or leave drag state');
 poll(1);mouse('vmousemove',0,0);
 check(requests.length===1 && !w.dragging && !w.options.disabled,'hover after confirmation must not dispatch');
 reset();var n=requests.length;
 mouse('vmousedown',0);mouse('vmousemove',1);poll(0);
 check(requests.length===n && String($('#lightflip').val())==='1','drag/poll must retain selection without dispatch');
 mouse('vmouseup',1);check(requests.length===n+1 && !w.dragging,'drag release must dispatch once');
 finish_switch('light',switchState.light.pending,'Not confirmed');mouse('vmousemove',0,0);
 check(requests.length===n+1 && !w.dragging,'hover after timeout must not dispatch');
 reset();n=requests.length;
 mouse('vmousedown',1,0);mouse('vmouseup',1,0);
 check(requests.length===n+1 && !w.dragging,'touch-style virtual gesture must dispatch once');
 reset();n=requests.length;
 w.handle.trigger($.Event('keydown',{which:39,keyCode:39}));
 check(requests.length===n && w._keySliding && !w.options.disabled,'keydown must not dispatch or disable');
 poll(0);
 w.handle.trigger($.Event('keyup',{which:39,keyCode:39}));
 check(requests.length===n+1 && !w._keySliding,'keyup must dispatch after key cleanup');
 poll(1);mouse('vmousemove',0,0);w.handle.trigger($.Event('keyup',{which:39,keyCode:39}));
 check(requests.length===n+1,'hover or unmatched keyup must not dispatch');
 reset();n=requests.length;
 $('#lightflip').val(1).trigger('change');poll(0);
 check(requests.length===n,'programmatic change must not dispatch');
 mouse('vmousedown',1);$(document).trigger('touchcancel');mouse('vmousemove',0,0);mouse('vmouseup',0);
 check(requests.length===n && !w.dragging,'touch cancellation must not dispatch');
 mouse('vmousedown',1);$(window).trigger('blur');mouse('vmousemove',0,0);mouse('vmouseup',0);
 check(requests.length===n && !w.dragging,'lost focus must cancel without dispatch');
 mouse('vmousedown',1);poll(0,0);
 check(!w.options.disabled,'validity loss must not disable mid-gesture');
 mouse('vmouseup',1);
 check(requests.length===n && !w.dragging && w.options.disabled,'invalid state on release must not dispatch');
 document.body.insertAdjacentHTML('beforeend','<pre id="result">PASS: real jQuery Mobile mouse/drag/touch-style/key gestures, pending clicks, polling, timeout, hover, cancellation and no synthetic commands</pre>');
}catch(e){document.body.insertAdjacentHTML('beforeend','<pre id="result">FAIL: '+e.message+'</pre>');}},200);});`;
const html='<!doctype html><html><head><script src="/jquery.js"></script><script src="/mobile.js"></script></head><body>'+body+'<script>'+script+'</script></body></html>';
const server=http.createServer((req,res)=>{
 res.setHeader('Content-Type',assets[req.url]?'application/javascript':'text/html');
 res.end(assets[req.url]|| (req.url==='/'?html:''));
});
server.listen(0,'127.0.0.1',()=>{
 const profile=fs.mkdtempSync(path.join(os.tmpdir(),'og-slider-gestures-'));
 execFile('/opt/google/chrome/chrome',['--headless=new','--disable-gpu','--disable-background-networking','--no-first-run','--no-default-browser-check','--disable-extensions','--user-data-dir='+profile,'--virtual-time-budget=2000','--dump-dom','http://127.0.0.1:'+server.address().port+'/'],{timeout:30000,maxBuffer:2000000},(err,stdout)=>{
  const result=[...stdout.matchAll(/<pre id="result">([\s\S]*?)<\/pre>/g)].pop()?.[1];
  console.log(result||String(err||'No result'));
  if(err||!result?.startsWith('PASS:'))process.exitCode=1;
  server.closeAllConnections();server.close();
 });
});
