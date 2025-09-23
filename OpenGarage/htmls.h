const char ap_home_html[] PROGMEM = R"OG(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>OpenGarage • Wi-Fi Setup</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
:root{
--bg: #f6f7fb; --fg: #0f172a; --muted: #586380;
--card: #ffffff; --border: #e2e8f0;
--accent: #2563eb; --accent-weak:#3b82f6;
--success:#0f9d58; --warn:#d97706; --error:#d14343;
--shadow: 0 8px 30px rgba(2,8,23,.12);
--radius: 16px; --radius-sm: 10px;
}
@media (prefers-color-scheme: dark){
:root{
--bg:#0b1220; --fg:#e6eaff; --muted:#a3acc7;
--card:#0f172a; --border:#1e293b;
--accent:#60a5fa; --accent-weak:#93c5fd;
--success:#34d399; --warn:#fbbf24; --error:#f87171;
--shadow: 0 8px 30px rgba(0,0,0,.45);
}
}
*{box-sizing:border-box}
html,body{margin:0; padding:0; background:var(--bg); color:var(--fg); font:16px/1.45 system-ui, -apple-system, Segoe UI, Roboto, "Helvetica Neue", Arial, "Noto Sans", "Apple Color Emoji","Segoe UI Emoji"}
a{color:var(--accent)}
.wrap{min-height:100dvh; display:grid; place-items:center; padding:24px}
.card{width:100%; max-width:740px; background:var(--card); border:1px solid var(--border); border-radius:var(--radius); box-shadow:var(--shadow); overflow:hidden}
.card-header{padding:20px 22px; border-bottom:1px solid var(--border); display:flex; align-items:center; gap:12px}
h1{font-size:20px; margin:0}
.card-body{padding:18px 22px 22px}
.grid{display:grid; gap:14px}
.two-col{display:grid; gap:10px; grid-template-columns:180px 1fr}
@media (max-width:560px){ .two-col{grid-template-columns:1fr} }
label{font-weight:600}
input[type=text], input[type=password]{
width:100%; height:42px; border:1px solid var(--border); border-radius:10px; padding:0 12px;
background:transparent; color:inherit; font-size:15px; outline:none;
}
input[type=text]:focus, input[type=password]:focus{border-color:var(--accent); box-shadow:0 0 0 4px color-mix(in oklab, var(--accent) 20%, transparent)}
.hint{color:var(--muted); font-size:13px}
.section{padding:14px 0; border-top:1px dashed var(--border)}
.section:first-of-type{border-top:none}
fieldset{border:none; padding:0; margin:0}
legend{font-weight:700; margin-bottom:8px}
.radio-row{display:grid; gap:10px}
.radio{display:flex; gap:10px; align-items:center}
input[type=radio]{inline-size:18px; block-size:18px}
/* Table */
.table-wrap{border:1px solid var(--border); border-radius:var(--radius-sm); overflow:auto}
table{width:100%; border-collapse:collapse; font-size:15px}
thead th{position:sticky; top:0; background:color-mix(in oklab, var(--card) 85%, var(--bg)); text-align:left; padding:10px 12px; border-bottom:1px solid var(--border)}
tbody td{padding:10px 12px; border-bottom:1px solid var(--border)}
tbody tr:nth-child(even){background:color-mix(in oklab, var(--card) 96%, var(--bg))}
tbody tr:hover{background:color-mix(in oklab, var(--card) 90%, var(--bg))}
.num{font-variant-numeric:tabular-nums; font-family:ui-monospace, SFMono-Regular, Menlo, Consolas, "Liberation Mono", monospace}
.clickable{cursor:pointer}
/* Actions */
.actions{display:flex; gap:10px; align-items:center; flex-wrap:wrap}
button{
appearance:none; border:1px solid var(--accent); background:var(--accent); color:white;
padding:10px 16px; border-radius:12px; font-weight:700; font-size:15px; cursor:pointer; transition:transform .02s ease, filter .15s ease
}
button[disabled]{opacity:.6; cursor:not-allowed; filter:grayscale(.2)}
button:active{transform:translateY(1px)}
.msg{min-height:24px; font-weight:600}
.msg.success{color:var(--success)}
.msg.warn{color:var(--warn)}
.msg.error{color:var(--error)}
.msg.neutral{color:var(--muted)}
/* Reveal animation for cloud block */
#tb_cld_wrap{display:grid; grid-template-rows:0fr; transition:grid-template-rows .25s ease}
#tb_cld_wrap.open{grid-template-rows:1fr}
#tb_cld{overflow:hidden}
/* Tiny spinner */
.spinner{inline-size:14px; block-size:14px; border:2px solid color-mix(in oklab, var(--accent) 20%, transparent); border-top-color:var(--accent); border-radius:50%; display:inline-block; animation:spin 1s linear infinite; vertical-align:-2px}
@keyframes spin{to{transform:rotate(1turn)}}
/* Focus visible */
:focus-visible{outline:3px solid color-mix(in oklab, var(--accent) 45%, transparent); outline-offset:3px; border-radius:8px}
/* Security+ radio chips */
.secplus-group{display:flex; gap:10px; flex-wrap:wrap; align-items:center; margin-top: 8px;}
.secplus-option{position:relative; display:inline-block}
.secplus-option input{position:absolute; opacity:0; pointer-events:none}
/* Base chip look */
.secplus-option label {
display:inline-flex; align-items:center; gap:8px;
padding:8px 14px; border-radius:9999px;
font-weight:700; cursor:pointer;
border:2px solid var(--border);
background:var(--card); color:var(--fg);
transition:background .15s ease, color .15s ease, border-color .15s ease;
}
/* Focus ring */
.secplus-option input:focus + label {
outline:3px solid color-mix(in oklab, var(--accent) 45%, transparent);
outline-offset:2px;
}
/* Hover previews */
.secplus-option label.bg-yellow:hover {
background:color-mix(in oklab, #eeee15 20%, var(--card));
}
.secplus-option label.bg-purple:hover {
background:color-mix(in oklab, #a855f7 20%, var(--card));
color:#4b5563; /* gray-600 so text stays readable on light tint */
}
.secplus-option label.bg-green:hover {
background:color-mix(in oklab, #22c55e 20%, var(--card));
color:#14532d; /* dark green for contrast */
}
/* Checked states */
.secplus-option input:checked + label.bg-yellow {
background:#eeee15; color:#111827; border-color:#d97706;
}
.secplus-option input:checked + label.bg-purple {
background:#a855f7; color:#ffffff; border-color:#6b21a8;
}
.secplus-option input:checked + label.bg-green {
background:#22c55e; color:#ffffff; border-color:#166534;
}
</style>
</head>
<body>
<div class="wrap">
<div class="card" role="region" aria-labelledby="title">
<div class="card-header">
<h1 id="title">OpenGarage Wi-Fi Config</h1>
</div>
<div class="card-body">
<div class="section">
<div class="hint">Select your network below or enter it manually.</div>
<div class="table-wrap" aria-live="polite">
<table id="rd">
<thead>
<tr><th>SSID</th><th>Strength</th><th>Power</th></tr>
</thead>
<tbody>
<tr><td colspan="3"><span class="spinner"></span> Scanning Wi-Fi…</td></tr>
</tbody>
</table>
</div>
</div>
<div class="section">
<div class="grid two-col">
<label for="ssid">Wi-Fi SSID</label>
<input type="text" id="ssid" autocomplete="off">
<label for="pass">Wi-Fi Password</label>
<input type="password" id="pass" autocomplete="off">
<label for="host">Host Name <span class="hint">(optional)</span></label>
<input type="text" id="host" maxlength="32" placeholder="e.g., opengarage">
</div>
</div>
<!-- Security+ section (hidden by default; shown when has_swrx==1) -->
<div class="section" id="secplus_section" hidden>
<fieldset>
<legend>Security+ version</legend>
<div class="hint">Choose the version based on your Learn button color.</div>
<div class="secplus-group" role="radiogroup" aria-label="Security+ version">
<div class="secplus-option">
<input type="radio" name="secplus" id="secplus_20" value="2">
<label for="secplus_20" class="bg-yellow">2.0</label>
</div>
<div class="secplus-option">
<input type="radio" name="secplus" id="secplus_10" value="1">
<label for="secplus_10" class="bg-purple">1.0</label>
</div>
<div class="secplus-option">
<input type="radio" name="secplus" id="secplus_none" value="0" checked>
<label for="secplus_none" class="bg-green">None</label>
</div>
</div>
</fieldset>
</div>
<div class="section">
<fieldset>
<legend>Enable Cloud Connection?</legend>
<div class="radio-row">
<label class="radio">
<input type="radio" id="none" name="token" checked onclick="toggle_cld()">
No, I’ll configure this later.
</label>
<label class="radio">
<input type="radio" id="blynk" name="token" onclick="toggle_cld()">
Use Blynk Token
</label>
<label class="radio">
<input type="radio" id="otc" name="token" onclick="toggle_cld()">
Use OpenThings Cloud (OTC) Token
</label>
</div>
</fieldset>
<div id="tb_cld_wrap" class="">
<div id="tb_cld">
<div class="grid two-col" style="margin-top:10px">
<div></div><div class="hint">Enter your cloud credentials</div>
<label for="auth">Token</label>
<input type="text" id="auth">
<label for="bdmn">Server</label>
<input type="text" id="bdmn">
<label for="bprt">Port</label>
<input type="text" id="bprt">
</div>
</div>
</div>
</div>
<div class="section actions">
<p id="msg" class="msg neutral" role="status" aria-live="polite"></p>
<button type="button" id="butt" onclick="sf()">Submit</button>
</div>
</div>
</div>
</div>
<script>
function id(s){return document.getElementById(s);}
function sel(i){id('ssid').value=id('rd'+i).value;}
function eval_cb(n){return id(n).checked;}
function dis_config(x){let a = document.querySelectorAll('#host, #ssid, #pass, #tb_cld input');for(let e of a){e.disabled = x;}}
function show_msg(s, level){ const msg = id('msg'); msg.textContent = s || ''; msg.className = 'msg ' + (level || 'neutral'); }
/* Cloud block open/close + defaults */
function toggle_cld(){
const wrap = id('tb_cld_wrap');
wrap.classList.remove('open');
if(eval_cb('blynk')) { wrap.classList.add('open'); id('bdmn').value='blynk.openthings.io'; id('bprt').value='8080'; }
if(eval_cb('otc'))   { wrap.classList.add('open'); id('bdmn').value='ws.cloud.openthings.io'; id('bprt').value='80'; }
}
var tci;
function tryConnect(){
var xhr=new XMLHttpRequest();
xhr.onreadystatechange=function() {
if(xhr.readyState==4 && xhr.status==200) {
var jd=JSON.parse(xhr.responseText);
if(jd.ip==0) return;
var ip=''+(jd.ip%256)+'.'+((jd.ip/256>>0)%256)+'.'+(((jd.ip/256>>0)/256>>0)%256)+'.'+(((jd.ip/256>>0)/256>>0)/256>>0);
show_msg('Connected! Device IP: '+ip+' — rebooting… Switch back to the above WiFi, then tap the button below to redirect.', 'success');
var b=id('butt'); b.innerHTML='Go to '+ip; b.disabled=false;
b.onclick=function rd(){window.open('http://'+ip, '_self');}
clearInterval(tci);
}
}
xhr.open('GET', 'jt', true);
xhr.send();
}
function sf(){
show_msg('', 'neutral');
if(!id('ssid').value) {show_msg('Wi-Fi SSID cannot be empty!', 'error');return;}
var xhr=new XMLHttpRequest();
xhr.onreadystatechange=function() {
if(xhr.readyState==4 && xhr.status==200) {
var jd=JSON.parse(xhr.responseText);
if(jd.result==1) {
id('butt').innerHTML='<span class="spinner"></span> Connecting…';
show_msg('Connecting, please wait…', 'neutral');
tci=setInterval(tryConnect, 2000);
return;
}
show_msg('Error code: '+jd.result+', item: '+jd.item, 'error');
id('butt').innerHTML='Submit';
dis_config(false);
}
};
// Read Security+ selection (default 0 if none selected/section hidden)
var secplusSel = document.querySelector('input[name="secplus"]:checked');
var secplusVal = secplusSel ? secplusSel.value : '0';
var comm='cc?ssid='+encodeURIComponent(id('ssid').value)
+'&pass='+encodeURIComponent(id('pass').value)
+'&host='+encodeURIComponent(id('host').value)
+'&secplus='+encodeURIComponent(secplusVal);
if(eval_cb('otc')||eval_cb('blynk')){
if(id('auth').value.length<32) {show_msg('Cloud token is too short!', 'error');return;}
comm+='&cld='+(eval_cb('blynk')?'blynk':'otc');
comm+='&auth='+encodeURIComponent(id('auth').value);
comm+='&bdmn='+id('bdmn').value;
comm+='&bprt='+id('bprt').value;
}
xhr.open('GET', comm, true);
xhr.send();
id('butt').disabled=true;
dis_config(true);
}
/* Load SSIDs once and detect has_swrx for Security+ UI */
function loadSSIDs(){
const tbody = document.querySelector('#rd tbody');
if(!tbody) return;
tbody.innerHTML = '<tr><td colspan="3"><span class="spinner"></span> Scanning Wi-Fi…</td></tr>';
var xhr=new XMLHttpRequest();
xhr.onreadystatechange=function() {
if(xhr.readyState==4 && xhr.status==200) {
const jd=JSON.parse(xhr.responseText);
// Show/hide Security+ group based on has_swrx
if (typeof jd.has_swrx !== 'undefined' && Number(jd.has_swrx) === 1) {
id('secplus_section').hidden = false;
} else {
id('secplus_section').hidden = true;
}
// Render SSIDs
tbody.innerHTML = '';
for(let i=0;i<jd.ssids.length;i++) {
const s = jd.ssids[i], rssi = jd.rssis[i];
const strength = rssi>-71?'OK':(rssi>-81?'Weak':'Poor');
const tr = document.createElement('tr');
tr.className = 'clickable';
tr.innerHTML = `
<td>
<label style="display:flex; gap:8px; align-items:center; cursor:pointer">
<input name="ssids" id="rd${i}" type="radio" value="${s}">
<span>${s}</span>
</label>
</td>
<td>${strength}</td>
<td class="num">(${rssi} dBm)</td>
`;
tr.addEventListener('click', ()=>{ id('rd'+i).checked=true; sel(i); });
tbody.appendChild(tr);
}
if(!jd.ssids.length){
tbody.innerHTML = '<tr><td colspan="3">No networks found.</td></tr>';
}
}
}
xhr.open('GET','js',true);
xhr.send();
}
/* Kick off an initial scan a moment after load */
setTimeout(loadSSIDs, 400);
</script>
</body>
</html>
)OG";
const char ap_update_html[] PROGMEM = R"OG(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>OpenGarage • Firmware Update</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
:root{
--bg:#f6f7fb; --fg:#0f172a; --muted:#586380;
--card:#fff; --border:#e2e8f0;
--accent:#2563eb; --accent-weak:#3b82f6;
--success:#0f9d58; --warn:#d97706; --error:#d14343;
--shadow:0 8px 30px rgba(2,8,23,.12);
--radius:16px; --radius-sm:10px;
}
@media (prefers-color-scheme: dark){
:root{
--bg:#0b1220; --fg:#e6eaff; --muted:#a3acc7;
--card:#0f172a; --border:#1e293b;
--accent:#60a5fa; --accent-weak:#93c5fd;
--success:#34d399; --warn:#fbbf24; --error:#f87171;
--shadow:0 8px 30px rgba(0,0,0,.45);
}
}
*{box-sizing:border-box}
body{
margin:0; padding:0; background:var(--bg); color:var(--fg);
font:16px/1.45 system-ui, -apple-system, Segoe UI, Roboto, "Helvetica Neue", Arial, "Noto Sans";
}
.wrap{min-height:100dvh; display:grid; place-items:center; padding:24px}
.card{
width:100%; max-width:740px; background:var(--card);
border:1px solid var(--border); border-radius:var(--radius);
box-shadow:var(--shadow); overflow:hidden;
}
.card-header{padding:20px 22px; border-bottom:1px solid var(--border)}
.card-header h1{font-size:20px; margin:0}
.card-body{padding:18px 22px 22px}
.section{padding:14px 0; border-top:1px dashed var(--border)}
.section:first-of-type{border-top:none}
.actions{display:flex; gap:10px; align-items:center; flex-wrap:wrap}
input[type=file]{
font-size:15px; border:1px solid var(--border);
padding:6px; border-radius:10px; background:transparent; color:inherit;
}
button{
appearance:none; border:1px solid var(--accent); background:var(--accent); color:white;
padding:10px 16px; border-radius:12px; font-weight:700; font-size:15px;
cursor:pointer; transition:transform .02s ease, filter .15s ease;
}
button[disabled]{opacity:.6; cursor:not-allowed; filter:grayscale(.2)}
button:active{transform:translateY(1px)}
.msg{min-height:24px; font-weight:600}
.msg.success{color:var(--success)}
.msg.error{color:var(--error)}
.msg.neutral{color:var(--muted)}
.spinner{
inline-size:14px; block-size:14px;
border:2px solid color-mix(in oklab,var(--accent) 20%, transparent);
border-top-color:var(--accent);
border-radius:50%; display:inline-block;
animation:spin 1s linear infinite; vertical-align:-2px;
}
@keyframes spin{to{transform:rotate(1turn)}}
</style>
</head>
<body>
<div class="wrap">
<div class="card" role="region" aria-labelledby="title">
<div class="card-header">
<h1 id="title">Firmware Update</h1>
</div>
<div class="card-body">
<form id="updateForm" enctype="multipart/form-data" method="post">
<div class="section">
<label for="file">Choose firmware (.bin) file:</label><br>
<input type="file" id="file" name="file" accept=".bin">
</div>
<div class="section actions">
<p id="msg" class="msg neutral" role="status" aria-live="polite"></p>
<button type="submit" id="btn">Upload</button>
</div>
</form>
</div>
</div>
</div>
<script>
const form = document.getElementById('updateForm');
const msg  = document.getElementById('msg');
const btn  = document.getElementById('btn');
function showMsg(text, level="neutral"){
msg.textContent = text || "";
msg.className = "msg " + level;
}
form.addEventListener('submit', e=>{
e.preventDefault();
const file = document.getElementById('file').files[0];
if(!file){ showMsg("Please choose a .bin file first","error"); return; }
btn.disabled = true;
btn.innerHTML = '<span class="spinner"></span> Uploading…';
showMsg("Uploading firmware, please wait…","neutral");
const xhr = new XMLHttpRequest();
xhr.open("POST", "/update", true);
xhr.upload.onprogress = evt=>{
if(evt.lengthComputable){
const pct = Math.round((evt.loaded/evt.total)*100);
showMsg("Uploading: "+pct+"%","neutral");
}
};
xhr.onload = ()=>{
if(xhr.status===200){
showMsg("Update successful! Rebooting…","success");
btn.innerHTML="Done";
} else {
showMsg("Update failed (status "+xhr.status+")","error");
btn.innerHTML="Upload"; btn.disabled=false;
}
};
xhr.onerror = ()=>{
showMsg("Upload error","error");
btn.innerHTML="Upload"; btn.disabled=false;
};
const formData = new FormData();
formData.append("file", file);
xhr.send(formData);
});
</script>
</body>
</html>
)OG";
const char sta_home_html[] PROGMEM = R"OG(<head><title>OpenGarage</title><meta name='viewport' content='width=device-width, initial-scale=1'><link rel='stylesheet' href='https://code.jquery.com/mobile/1.3.1/jquery.mobile-1.3.1.min.css' type='text/css'><script src='https://code.jquery.com/jquery-1.9.1.min.js' type='text/javascript'></script><script src='https://code.jquery.com/mobile/1.3.1/jquery.mobile-1.3.1.min.js' type='text/javascript'></script></head>
<body>
<style> table, th, td {border: 0px solid black;padding: 6px; border-collapse: collapse; } .img {width: inherit; height: inherit; position: absolute; top: inherit; left: inherit;} .img[src=""] {display:none;} 
.lightlock-ctrl div.ui-slider-switch {width:160px;}
</style>
<div data-role='page' id='page_home'><div data-role='header'><h3 id='head_name'>OG</h3></div>
<div data-role='content'><div data-role='fieldcontain'>
<table><tr><td><b>Door:<br></td><td><label id='lbl_status'>-</label></td>
<td rowspan='2'><div id='pic' style='width:112px;height:64px;'><img id='car_pic' src='' class='img'><img id='door_pic' src='' class='img'><img id='arrow_pic' src='' class='img'></div></td></tr>
<tr><td><b><label id='lbl_vstatus1'>Vehicle:</label></b></td><td><label id='lbl_vstatus'>-</label></td></tr>
<tr><td><b>Distance:</b></td><td><label id='lbl_dist'>-</label></td><td></td></tr>
<tr><td><b>Read Cnt.:</b></td><td><label id='lbl_beat'>-</label></td><td></td></tr>
<tr id='tbl_sn2' style='display:none'><td><b>Switch Sn.:</b></td><td><label id='lbl_sn2'>-</label></td><td></td></tr>
<tr id='tbl_obstruct' style='display:none'><td><b>Obstructed:</b></td><td><label id='lbl_obstruct'>-</label></td><td></td></tr>
<tr><td><b>WiFi Signal:</b></td><td colspan='2'><label id='lbl_rssi'>-</label></td></tr>
<tr><td><b>Cloud:</b></td><td colspan='2'><label id='lbl_cld'>-</label></td></tr>
<tr id='tbl_th'><td><b>T/H sensor:</b></td><td colspan='2'><label id='lbl_th'>-</label></td></tr>
<tr><td><b>Device Key:</b></td><td colspan='2' ><input type='password' size=20 maxlength=64 name='dkey' id='dkey'></td></tr>
<tr><td colspan=3><label id='msg'></label></td></tr>
</table>
<div class="lightlock-ctrl" style='display:none'>
<label for="lightflip" class="ui-hidden-accessible"></label>
<select name="lightflip" id="lightflip" data-theme="e" data-role="slider">
<option value=0>Light Off</option>
<option value=1>Light On</option>
</select>
<label for="lockflip" class="ui-hidden-accessible"></label>
<select name="lockflip" id="lockflip" data-theme="e" data-role="slider">
<option value=0>Lock Inactive</option>
<option value=1>Lock Active</option>
</select>
</div>
<span style='display:block;height:5px'></span>
<div data-role='controlgroup' data-type='horizontal'>
<button data-theme='b' id='btn_click'>Door</button>
<button data-theme='b' id='btn_opts'>Options</button>
<button data-theme='b' id='btn_log'>View Log</button>
</div>
<span style='display:block;height:5px'></span>
<div data-role='controlgroup' data-type='horizontal'>
<button data-theme='c' id='btn_rbt'>Reboot</button>
<button data-theme='c' id='btn_rap'>Reset WiFi</button>
<button data-theme='c' id='btn_cll'>Clear Log</button>
</div>
</div>
</div>
<div data-role='footer' data-theme='c'>
<p>&nbsp; OpenGarage Firmware v<label id='fwv'>-</label><div data-role='controlgroup' data-type='horizontal'><a href='update' target='_top' data-role='button' data-inline=true data-mini=true>Firmware Update</a><a href='https://github.com/OpenGarage/OpenGarage-Firmware/tree/master/docs' target='_blank' data-role='button' data-inline=true data-mini=true>User Manual</a></p></div>
</div>
</div>
<script>
var si;
function clear_msg() {$('#msg').text('');}
function show_msg(s,t,c) { $('#msg').text(s).css('color',c); if(t>0) setTimeout(clear_msg, t); }
$('#btn_opts').click(function(e){window.open('vo', '_top');});
$('#btn_log').click(function(e){window.open('vl', '_top');});
$('#btn_cll').click(function(e){
if(confirm('Clear log data?')){
var comm = 'clearlog?dkey='+encodeURIComponent($('#dkey').val());
clear_msg();
$.getJSON(comm, function(jd) {
if(jd.result!=1) show_msg('Check device key and try again.',2000,'red');
else { show_msg('Log data cleared',2000,'green'); }
});
}
});
$('#btn_rbt').click(function(e){
if(confirm('Reboot the device now?')){
var comm = 'cc?reboot=1&dkey='+encodeURIComponent($('#dkey').val());
clear_msg();
$.getJSON(comm, function(jd) {
if(jd.result!=1) show_msg('Check device key and try again.',2000,'red');
else {
show_msg('Rebooting. Please wait...',0,'green');
clearInterval(si);
setTimeout(function(){location.reload(true);}, 10000);
}
});
}
});
$('#btn_rap').click(function(e){
if(confirm('Reset the device to AP mode?')){
var comm = 'cc?apmode=1&dkey='+encodeURIComponent($('#dkey').val());
clear_msg();
$.getJSON(comm, function(jd) {
if(jd.result!=1) show_msg('Check device key and try again.',2000,'red');
else {
clearInterval(si);
$('#msg').html('Device is now in AP mode. Log on<br>to SSID OG_xxxxxx, then <br> click <a href="http://192.168.4.1">http://192.168.4.1</a><br>to configure.').css('color','green');
}
});
}
});
function do_action_command(comm) {
var comm = 'cc?'+comm+'&dkey='+encodeURIComponent($('#dkey').val());
$.getJSON(comm)
.done(function( jd ) {
if(jd.result!=1) {
show_msg('Check device key and try again.',2000,'red');
}else{clear_msg();};
})
.fail(function( jqxhr, textStatus, error ) {
var err = error;
$('#msg').text('Request Failed: ' + err).css('color','red');
});
}
$('#lightflip').on('change', function(e) {
do_action_command('light=toggle');
});
$('#lockflip').on('change', function(e) {
do_action_command('lock=toggle');
});
$('#btn_click').click(function(e) {
do_action_command('click=1');
});
$(document).ready(function() { show(); si=setInterval('show()', 5000); });
function show() {
$.ajax({
url:'jc',
dataType:'JSON',
timeout:5000,
success:function(jd){
$('#fwv').text((jd.fwv/100>>0)+'.'+(jd.fwv/10%10>>0)+'.'+(jd.fwv%10>>0));
$('#lbl_dist').text(jd.dist +' (cm)').css('color', jd.dist==450?'red':'black');
let status_text;
let status_color;
const door_img = document.getElementById("door_pic");
const arrow_img = document.getElementById("arrow_pic");
let base = "https://raw.githubusercontent.com/OpenGarage/OpenGarage-Firmware/refs/heads/dev/1.2.4/icons/parts/";
switch (jd.door) {
case 0: // Closed
status_text = "CLOSED";
status_color = "green";
door_img.src = base+"closed.svg"
arrow_img.src = ""
break;
case 1: // Open
status_text = "OPENED";
status_color = "red";
door_img.src = base+"open.svg"
arrow_img.src = ""
break;
case 3: // Stopped
status_text = "STOPPED";
status_color = "red";
door_img.src = base+"partial.svg"
arrow_img.src = ""
break;
case 4: // Closing
status_text = "CLOSING";
status_color = "green";
door_img.src = base+"partial.svg"
arrow_img.src = base+"arrow_down.svg"
break;
case 5: // Opening
status_text = "OPENING";
status_color = "red";
door_img.src = base+"partial.svg"
arrow_img.src = base+"arrow_up.svg"
break;
case 2: // UNKNOWN / UNSET
default:
status_text = "UNKNOWN";
status_color = "red";
break;
}
$('#lbl_status').text(status_text).css('color', status_color);
if(typeof(jd.light)=='undefined'||typeof(jd.lock)=='undefined') {
$('.lightlock-ctrl').hide();
}else{
$('.lightlock-ctrl').show();
$('#lightflip').val(jd.light).slider('refresh');
$('#lockflip').val(jd.lock).slider('refresh');
}
if (jd.vehicle >=2){
$('#lbl_vstatus1').hide();
$('#lbl_vstatus').text('');
}else{
$('#lbl_vstatus1').show()
$('#lbl_vstatus').text(jd.vehicle == 1 ?'Present':(jd.vehicle == 0 ?'Absent':'Unavailable'));
}
const car_img = document.getElementById("car_pic");
if(jd.vehicle < 3 && jd.vehicle > 0) {
car_img.src = base+"car.svg";
car_img.style.filter=jd.vehicle == 1 ? 'brightness(0)' : '';
}else{
car_img.src = "";
}
if(typeof(jd.sn2)!='undefined') {$('#tbl_sn2').show(); $('#lbl_sn2').text(jd.sn2?'High':'Low');}
else {$('#tbl_sn2').hide();}
$('#lbl_beat').text(jd.rcnt).css('color','black');
$('#lbl_rssi').text((jd.rssi>-71?'Good':(jd.rssi>-81?'Weak':'Poor')) +' ('+ jd.rssi +' dBm)');
if(jd.cld==0) $('#lbl_cld').text('None');
else if(jd.cld==1) {$('#lbl_cld').text('Blynk '+['(disconnected)','(connected)'][jd.clds]);}
else {$('#lbl_cld').text('OTC '+['(not enabled)','(connecting...)','(disconnected)','(connected)'][jd.clds]);}
$('#head_name').text(jd.name);
$('#btn_click').html(jd.door?'Close Door':'Open Door').button('refresh');
if(typeof(jd.temp)!='undefined') {$('#tbl_th').show(); $('#lbl_th').text(jd.temp.toFixed(1)+String.fromCharCode(176)+'C / '+(jd.temp*1.8+32).toFixed(1)+String.fromCharCode(176)+'F (H:'+jd.humid.toFixed(1)+'%)');}
else {$('#tbl_th').hide();}
if(typeof(jd.obstruct)!='undefined') {$('#tbl_obstruct').show(); $('#lbl_obstruct').text(jd.obstruct?'YES':'no').css('color',jd.obstruct?'red':'black');} else {$('#tbl_obstruct').hide();}
},
error:function(){
$('#lbl_beat').text('(offline)').css('color','red');
$('#lbl_cld').text('disconnected');
}
});
}
</script>
</body>
)OG";
const char sta_logs_html[] PROGMEM = R"OG(<head>
<title>OpenGarage</title>
<meta name='viewport' content='width=device-width, initial-scale=1'>
<link rel='stylesheet' href='//code.jquery.com/mobile/1.3.1/jquery.mobile-1.3.1.min.css' type='text/css'>
<script src='//code.jquery.com/jquery-1.9.1.min.js' type='text/javascript'></script>
<script src='//code.jquery.com/mobile/1.3.1/jquery.mobile-1.3.1.min.js' type='text/javascript'></script>
</head>
<body>
<div data-role='page' id='page_log'>
<div data-role='header'><h3><label id='lbl_name'></label> Log</h3></div>
<div data-role='content'>
<div data-role='timegroup'>
<table>
<tr><td>Current Time:</td><td><strong><label id='lbl_time' /></strong></td></tr>
<tr><td>Start Time:</td><td><strong><label id='lbl_start_time' /></strong></td></tr>
<tr><td>Up Time:</td><td><strong><label id='lbl_up_time' /></strong></td></tr>
</table>
</div>
<p>Below are the most recent <strong><label id='lbl_nr' /></strong> records</p>
<div data-role="controlgroup" data-type="horizontal">
<button data-theme="b" id="btn_back">Back</button>
</div>
<div data-role='fieldcontain'>
<table id='tab_log' border='1' cellpadding='4' style='border-collapse:collapse;'><tr><td align='center'><b>Event</b></td><td align='center'><b>DateTime</b></td><td align='center'><b>Dist.</b></td><td align='center' id='col_sn2' style='display:none;'><b>Switch</b></td></tr></table>
</div>
</div>
</div>
<script>
var curr_time = 0;
var sdate = new Date();
var date = new Date();
$("#btn_back").click(function(){history.back();});
$(document).ready(function(){
show_log();
setInterval(show_time, 1000);
});
function get_delta_time(date_now, date_future) {
// from: https://stackoverflow.com/a/13904120
// get total seconds between the times
var delta = Math.abs(date_future - date_now) / 1000;
// calculate (and subtract) whole days
var days = Math.floor(delta / 86400);
delta -= days * 86400;
// calculate (and subtract) whole hours
var hours = Math.floor(delta / 3600) % 24;
delta -= hours * 3600;
// calculate (and subtract) whole minutes
var minutes = Math.floor(delta / 60) % 60;
delta -= minutes * 60;
// what's left is seconds
var seconds = delta % 60;  // in theory the modulus is not required
return days + ":" + String(hours).padStart(2, '0') + ":" + String(minutes).padStart(2, '0') + ":" + String(seconds).padStart(2, '0');
}
function show_time() {
curr_time ++;
date.setTime(curr_time*1000);
$('#lbl_time').text(date.toLocaleString());
$('#lbl_up_time').text(get_delta_time(date, sdate));
}
function show_log() {
$.getJSON('jl', function(jd) {
$('#lbl_name').text(jd.name);
curr_time = jd.time;
$('#tab_log').find('tr:gt(0)').remove();
var logs=jd.logs;
logs.sort(function(a,b){return b[0]-a[0];});
$('#lbl_nr').text(logs.length);
sdate.setTime(jd.starttime*1000);
$('#lbl_start_time').text(sdate.toLocaleString());
var ldate = new Date();
for(var i=0;i<logs.length;i++) {
ldate.setTime(logs[i][0]*1000);
var r='<tr></td><td align="left"><img id="pic" src="data:image/png;base64,' + (logs[i][1]?'iVBORw0KGgoAAAANSUhEUgAAACAAAAAZCAYAAABQDyyRAAAABmJLR0QA/wD/AP+gvaeTAAAACXBIWXMAAA7EAAAOxAGVKw4bAAAAB3RJTUUH4gEBFiwkDx0OQQAAArtJREFUSMfFlstLlFEYxn/nfN/5HHVmskTF1LQCcRMRRBAZClbQH1CLatUiqm2rWrUP2rRqUZuKVgUREUFR2M2yMhC6iGSafl3QaZxJ53K+75wWSReaMTUdn+XhXJ73Pe/7vI94/e6mrXj2nPhIkpCAkkAIJuvL0Vu24cbu3qdhxAMRpZSoTkHq40NkjS9AUHoIiKc8pBcqlg0C5IIOGgN5jY555FeUYXUeQrOgq9x57bagRZ78xvXYjnai1a1gLdnECJmnd4g/G8YJBQixSASCEC1y0NhI0NqMamlDtLSiyCD7X5F6cRkroKJ6LdGuPbA7RtZ/izMwRDA0gDfk4xgPlFMgmB8khT150hb+H0l61yYimztRbhSMJeX3Ibof4ExpdFc7sXVbkAi++f3I7kfocBq5dTuVzRuRjiLLFEFPN5W3esDMPGMtVhj82DgqdIsTsLEomcP7cB73ksslkfEqIjVrsc1NqEhV4YTpaRj9QDo9zGR/L/HBCfTBA5R37EQlUugwwMtC5De5KZ4Ba8Eavh3aQ7RhQ7GSKFbcACQIWCXUAmtACKzrorw4uWCS/g8XEDNXWwFl77Os6ZvCyL/P8fkLK48ep6Jh9SJ0gbUYE9DWuJeoqv213gKms3D02Zc9MDGBXQwCAoHMhHw6tp/J6XAunYoz6lN+7sb/tWGgIJ/4SKgzlMsyGq88QSXTc9eMQq1XAH8roZWkKwMmrp9DnT2Pm5z6sewszcD4IwOBJxkf6yNy5hp1CYPxFFos7aT6SSBTFpC8ep76Xr+k88hFOIznhpEXr1A/+LXkA9Edy72h9vRlVNawHHDrTl3C1bNImuOA687ftLjuHLfpWTTAGJx73YiqamR+nn7x9m2cprp/exJbXNIXzYBiiz8h7VL7QTt7fPJLU2TZLKF2QPonDhLK5SHw5sgOvgNSpAhhsRiYRQAAAABJRU5ErkJggg==':'iVBORw0KGgoAAAANSUhEUgAAACAAAAAZCAYAAABQDyyRAAAABmJLR0QA/wD/AP+gvaeTAAAACXBIWXMAAAsTAAALEwEAmpwYAAAAB3RJTUUH4gEBFisqp+S1gQAAAuRJREFUSMfFlt1LFFEYxn9nZpx1dScV11w1NzNFQ7QouiiiDwq6kQiCoAir+6B/pbrLKLqqm6CrIIiMro2+sw9qzVXXdN00V3fc2dlzuuiL0t2OW+h7NcM5PO/znvd5n3NE/0BXA3AR2MbqxhBw3gJuAPtZ/WgHNhlrlPxH9BiscZRIQODLRRQSAD/vAqIkJGslmz1/nopAmJbwIbY0HSdStR2A5NwrXiduMjx1j/nsJ2zL0S+lf6BLFV5WZP004VAn0fABupv7cMqbUEoSm7rLm8QtFJL2yBHa6nsxDRvXS/Esfo2R6Qck0y+xLQdR5HQKEhAIdrSeo6f5LKZhk/GSvP90h+HkPWyrkvb6I7RFegGIpx7yduI2Xu4LkeqddDQcJVTeiELyPH6dR7GL5HwfXyqkBKUUUn1rWsEWmIZNa91hBmOXmJgdpKZyM50Nx+iJnlmyN1q7j2jtPgAyXpKnI1cZm45hiSZ2dZzgbfwCrpdBiKVKKdICRcZLcXL3feqcrpJVfvqSTXVlI0KIlYpQYBkBApZDLp/hycfLOMHGn2sLrk82tzz3ucwke7b04QTDlFnlBZNrT0Eu71Jf1cPG8EGtqmOTg2SyszjB8P8YQwEYDI0+xhT1WgTGU0N0bNj7bz5gCJjL5Mn5ElOAbYaoqoxogYaCtaBk6QSkVMQmFkmlXdT3NkslUVIPVEqp7YzW71ULkl88EtMehuCXeATMLkyQmHmjBZqajxMNd6+MgFKKd2MuizmJ8Qd5pRQt67fT2aTX16C9DqnyegSEADcr+TDu/pTccjKMTQ5iGKYW6FjqFbs7TuoRmEn7xCcXMQ1R5EZQVASqWRdcrwVaYY8DSo/A2FS2aHIhBEoponVbaW/YpXfDCQOlOwVFTIoyM8DAyyuYwmJmIcHo9Ast0M/pUQJlDqHyGmyrojjZ81db/nJWapmvv1tXob8SnVAHqsQnmRAMr+GTcNYA+oD8WmQXglNfAfQMC/0EmCdPAAAAAElFTkSuQmCC') +'" style="width:20px;height:15px;">'+(logs[i][1]?' Opened':' Closed')+'<td align="left">'+ldate.toLocaleString()+'</td><td align="right">'+logs[i][2]+' cm</td>';
if(typeof(jd.ncols)!='undefined'&&jd.ncols>3) {
r+='<td align="left">'+(logs[i][3]==255?'-':(logs[i][3]?'High':'Low'))+'</td></tr>';
}
r+='</tr>';
$('#tab_log').append(r);
}
if(typeof(jd.ncols)!='undefined'&&jd.ncols>3) { $('#col_sn2').show(); }
else { $('#col_sn2').hide(); }
});
setTimeout(show_log, 10000);
}
</script>
</body>
)OG";
const char sta_options_html[] PROGMEM = R"OG(<head><title>OpenGarage</title><meta name='viewport' content='width=device-width, initial-scale=1'><link rel='stylesheet' href='//code.jquery.com/mobile/1.3.1/jquery.mobile-1.3.1.min.css' type='text/css'><script src='//code.jquery.com/jquery-1.9.1.min.js' type='text/javascript'></script><script src='//code.jquery.com/mobile/1.3.1/jquery.mobile-1.3.1.min.js' type='text/javascript'></script></head>
<body>
<style> table, th, td { border: 0px solid black; padding: 1px; border-collapse: collapse; } .ui-select{width:160px;}
input[type="radio"].yellow:checked + label span{ background-color:#C0C000; }
input[type="radio"].purple:checked + label span{ background-color:#A000A0; }
input[type="radio"].green:checked + label span{ background-color:#00A000; }
</style>
<div data-role='page' id='page_opts'>
<div data-role='header'><h3>Edit Options</h3></div>
<div data-role='content'>
<fieldset data-role='controlgroup' data-type='horizontal'>
<input type='radio' name='opt_group' id='basic' onclick='toggle_opt()' checked><label for='basic'>Basic</label>
<input type='radio' name='opt_group' id='cloud' onclick='toggle_opt()'><label for='cloud'>Integration</label>
<input type='radio' name='opt_group' id='other' onclick='toggle_opt()'><label for='other'>Advanced</label>
</fieldset>
<div id='div_basic'>
<table cellpadding=2>
<tr><td><b>Device Name:</b></td><td><input type='text' size=10 maxlength=32 id='name' data-mini='true' value='-'></td></tr>
<tr><td><b>Distance Sensor:</b><br><small>mounting method</small></td><td>
<select name='sn1' id='sn1' data-mini='true'>
<option value=0>Ceiling Mount</option>
<option value=1>Side Mount</option>
</select></td></tr>
<tr id='secv'><td><b>Security+ Version:</b><br><small>Learn button color</small></td><td>
<fieldset data-role='controlgroup' data-mini='true' data-type='horizontal'>
<input type='radio' name='secv' id='secv2' value=2 class='yellow'><label for='secv2'>2.0</label>
<input type='radio' name='secv' id='secv1' value=1 class='purple'><label for='secv1'>1.0</label>
<input type='radio' name='secv' id='secv0' value=0 class='green'><label for='secv0'>None</label>
</fieldset>
</td></tr>
<tr><td><b>Door Thres. (cm): </b></td><td><input type='text' size=3 maxlength=4 id='dth' data-mini='true' value=0></td></tr>
<tr><td><b>Car Thres. (cm):</b><br><small>set to 0 to disable</small></td><td><input type='text' size=3 maxlength=4 id='vth' data-mini='true' value=0 ></td></tr>
<tr><td><b>Status Check (s):</b><br><small>check status every</small></td><td><input type='text' size=3 maxlength=3 id='riv' data-mini='true' value=0></td></tr>
<tr><td><b>Click Time (ms):</b></td><td><input type='text' size=3 maxlength=5 id='cdt' value=0 data-mini='true'></td></tr>
<tr><td><b>Switch Sensor:</b><br><small>on G04 and GND</small></td><td>
<select name='sn2' id='sn2' data-mini='true' onChange='update_sno()'>
<option value=0>(none)</option>
<option value=1>Normally Closed</option>
<option value=2>Normally Open</option>
</select></td></tr>
<tr><td><b>Sensor Logic:</b><a href='#snoInfo' data-rel='popup' data-role='button' data-inline='true' data-transition='pop' data-icon='info' data-theme='c' data-iconpos='notext'>Explanation</a><div data-role='popup' id='snoInfo' class='ui-content' data-theme='b' style='max-width:250px;'><p>Choose which sensor(s) determine door 'open' status.</p></div></td><td>
<select name='sno' id='sno' data-mini='true' disabled='true'>
<option value=0>Dist. sensor only</option>
<option value=1>Switch sensor only</option>
<option value=2>Dist. AND Switch</option>
<option value=3>Dist. OR Switch</option>
</select></td></tr>
<tr><td><b>T/H Sensor:</b></td><td>
<select name='tsn' id='tsn' data-mini='true'>
<option value=0>(none)</option>
<option value=2>DHT11 on G04</option>
<option value=3>DHT22 on G04</option>
<option value=4>DS18B20 on G04</option>
</select></td></tr>
<tr><td><b>Sound Alarm:</b></td><td>
<select name='alm' id='alm' data-mini='true'>
<option value=0>Disabled</option>
<option value=1>5 seconds</option>
<option value=2>10 seconds</option>
</select></td></tr>
<tr><td colspan=2><input type='checkbox' id='aoo' data-mini='true'><label for='aoo'>Do not alarm when opening</label></td></tr>
<tr><td><b>Log Size:</b><a href='#lszInfo' data-rel='popup' data-role='button' data-inline='true' data-transition='pop' data-icon='info' data-theme='c' data-iconpos='notext'>Important note</a><div data-role='popup' id='lszInfo' class='ui-content' data-theme='b' style='max-width:280px;'><p>If you change log size, please Clear Log for the new size to take effect.</p></div></td><td>
<select name='lsz' id='lsz' data-mini='true'>
<option value=20>20</option>
<option value=50>50</option>
<option value=100>100</option>
<option value=200>200</option>
<option value=400>400</option>
</select></td></tr>
</table>
</div>
<div id='div_cloud' style='display:none;'>
<table cellpadding=1>
<tr><td colspan=2><input type='checkbox' id='cld' data-mini='true' onclick='update_cld()'><label for='cld'>Enable Cloud Connection</label></td></tr>
<tr class='cld'><td><b>Cloud Type:</b></td><td>
<fieldset data-role='controlgroup' data-mini='true' data-type='horizontal'>
<input type='radio' name='rd_ct' id='blynk' onclick='update_ct()' checked><label for='blynk'>Blynk</label>
<input type='radio' name='rd_ct' id='otc' onclick='update_ct()'><label for='otc'>OTC</label>
</fieldset>
</td></tr>
<tr class='cld'><td><b>Cloud Token:</b></td><td><input type='text' size=22 maxlength=64 id='auth' data-mini='true'></td></tr>
<tr class='cld'><td><b>Cloud Server:</b></td><td><input type='text' size=22 maxlength=64 id='bdmn' data-mini='true'></td></tr>
<tr class='cld'><td><b>Cloud Port:</b></td><td><input type='text' size=5 maxlength=5 id='bprt' data-mini='true'></td></tr>
<tr class='cld'><td colspan=2><hr></td></tr>
<tr><td colspan=2><input type='checkbox' id='mqen' data-mini='true' onclick='update_mqtt()'><label for='mqen'>Enable MQTT</label></td></tr>
<tr class='mqt'><td><b>MQTT Server:</b></td><td><input type='text' size=16 maxlength=64 id='mqtt' data-mini='true'></td></tr>
<tr class='mqt'><td><b>MQTT Port:</b></td><td><input type='text' size=5 maxlength=5 id='mqpt' data-mini='true'></td></tr>
<tr class='mqt'><td><b>MQTT Username:</b></td><td><input type='text' size=16 maxlength=64 id='mqur' data-mini='true' placeholder='(optional)'></td></tr>
<tr class='mqt'><td><b>MQTT Password:</b></td><td><input type='password' size=16 maxlength=64 id='mqpw' data-mini='true' placeholder='(unchanged if left blank)'></td></tr>
<tr class='mqt'><td><b>MQTT Topic:</b></td><td><input type='text' size=16 maxlength=64 id='mqtp' data-mini='true' placeholder='(optional)'></td></tr>
<tr class='mqt'><td colspan=2><hr></td></tr>
<tr><td colspan=2><input type='checkbox' id='emen' data-mini='true' onclick='update_email()'><label for='emen'>Enable Email Notifications</label></td></tr>
<tr class='email'><td><b>SMTP Server:</b></td><td><input type='text' size=16 maxlength=64 id='smtp' data-mini='true'></td></tr>
<tr class='email'><td><b>SMTP Port:</b></td><td><input type='text' size=5 maxlength=5 id='sprt' data-mini='true'></td></tr>
<tr class='email'><td><b>Sender Email:</b></td><td><input type='text' size=16 maxlength=64 id='send' data-mini='true'></td></tr>
<tr class='email'><td><b>App Password:</b></td><td><input type='text' size=16 maxlength=64 id='apwd' data-mini='true'></td></tr>
<tr class='email'><td><b>Recipient Email:</b></td><td><input type='text' size=16 maxlength=64 id='recp' data-mini='true'></td></tr>
<tr height=30px><td colspan=2><b>Choose Notification Events:</b></td></tr>
<tr><td><input type='checkbox' id='noto0' data-mini='true'><label for='noto0'>Door Open</label></td><td><input type='checkbox' id='noto1' data-mini='true' ><label for='noto1'>Door Close</label></td></tr>
<tr><td><b>IFTTT Key:</b></td><td><input type='text' size=20 maxlength=64 id='iftt' data-mini='true' placeholder='(if using IFTTT notification)'></td></tr>
</table>
<table>
<tr><td colspan=4><hr></td></tr>
<tr><td colspan=4><b>Automation:</b></td></tr>
<tr><td colspan=4></td></tr><tr><td colspan=4></td></tr>
<tr><td colspan=4>If open for longer than:</td></tr>
<tr><td><input type='text' size=3 maxlength=3 id='ati' value=30 data-mini='true'></td><td>minutes:</td><td><input type='checkbox' id='ato0' data-mini='true'><label for='ato0'>Notify me</label></td><td><input type='checkbox' id='ato1' data-mini='true'><label for='ato1'>Auto-close</label></td></tr>
<tr><td colspan=4>If open after time:<small> (Use UTC 24hr format)</small>:</td></tr>
<tr><td><input type='text' size=3 maxlength=3 id='atib' value=3 data-mini='true'></td><td> UTC:</td><td><input type='checkbox' id='atob0' data-mini='true'><label for='atob0'>Notify me</label></td><td><input type='checkbox' id='atob1' data-mini='true'><label for='atob1'>Auto-close</label></td></tr>
</table><table>
</table>
</div>
<div id='div_other' style='display:none;'>
<table cellpadding=2>
<tr><td><b>Read Intv. (ms):</b><br><small>read sensor every</small></td><td><input type='text' size=3 maxlength=5 id='dri' value=0 data-mini='true'></td></tr>
<tr><td><b>Sensor Filter:</b><br><small>noise filter method</small></td><td>
<fieldset data-role='controlgroup' data-mini='true' data-type='horizontal'>
<input type='radio' name='rd_sf' id='sf_med' value=0 onclick='update_sfi()'><label for='sf_med'>Median</label>
<input type='radio' name='rd_sf' id='sf_con' value=1 onclick='update_sfi()'><label for='sf_con'>Consensus</label>
</fieldset>
</td></tr>
<tr id='tbl_cmr' style='display:none;'><td><b>Margin (cm):</b></td><td><input type='text' size=5 maxlength=5 id='cmr' value=10 data-mini='true'></td></tr>
<tr><td><b>Dist. Timeout:</b><br><small>timeout handling</small></td><td>
<fieldset data-role='controlgroup' data-mini='true' data-type='horizontal'>
<input type='radio' name='rd_to' id='to_ignore' value=0><label for='to_ignore'>Ignore</label>
<input type='radio' name='rd_to' id='to_cap' value=1><label for='to_cap'>Cap</label>
</fieldset>
</td></tr>
<tr><td><b>Stop Blink After:</b><br><small>set to 0 to disable</small></td><td><input type='text' size=2 maxlength=2 id='bas' data-mini='true' value=0></td></tr>
<tr><td><b>HTTP Port:</b></td><td><input type='text' size=5 maxlength=5 id='htp' value=0 data-mini='true'></td></tr>
<tr><td><b>Host Name:</b></td><td><input type='text' size=15 maxlength=32 id='host' data-mini='true' placeholder='(optional)'></td></tr>
<tr><td><b>NTP Server:</b></td><td><input type='text' size=15 maxlength=64 id='ntp1' data-mini='true' placeholder='(optional)'></td></tr>
<tr><td colspan=2><input type='checkbox' id='usi' data-mini='true' onclick='update_usi()'><label for='usi'>Use Static IP</label></td></tr>
<tr class='si'><td><b>Device IP:</b></td><td><input type='text' size=15 maxlength=15 id='dvip' data-mini='true'></td></tr>
<tr class='si'><td><b>Gateway IP:</b></td><td><input type='text' size=15 maxlength=15 id='gwip' data-mini='true'></td></tr>
<tr class='si'><td><b>Subnet:</b></td><td><input type='text' size=15 maxlength=15 id='subn' data-mini='true'></td></tr>
<tr class='si'><td><b>DNS1:</b></td><td><input type='text' size=15 maxlength=64 id='dns1' data-mini='true'></td></tr>
<tr><td colspan=2><input type='checkbox' id='cb_key' data-mini='true' onclick='update_ckey()'><label for='cb_key'>Change Device Key</label></td></tr>
<tr class='ckey'><td><b>New Key:</b></td><td><input type='password' size=16 maxlength=64 id='nkey' data-mini='true'></td></tr>
<tr class='ckey'><td><b>Confirm:</b></td><td><input type='password' size=16 maxlength=64 id='ckey' data-mini='true'></td></tr>
</table>
</div>
<br />
<table cellpadding=2>
<tr><td><b>Device Key:</b></td><td><input type='password' size=24 maxlength=64 id='dkey' data-mini='true'></td></tr>
<tr><td colspan=2><p id='msg'></p></td></tr>
</table>
<div data-role='controlgroup' data-type='horizontal'>
<a href='#' data-role='button' data-inline='true' data-theme='a' id='btn_back'>Back</a>
<a href='#' data-role='button' data-inline='true' data-theme='b' id='btn_submit'>Submit</a> 
</div>
</div>
<div data-role='footer' data-theme='c'>
<p>&nbsp; OpenGarage Firmware v<label id='fwv'>-</label><div data-role='controlgroup' data-type='horizontal'><a href='update' target='_top' data-role='button' data-inline=true data-mini=true>Firmware Update</a><a href='https://github.com/OpenGarage/OpenGarage-Firmware/tree/master/docs' target='_blank' data-role='button' data-inline=true data-mini=true>User Manual</a></p></div>
</div>
</div>
<script>
let prev_ct=1;
function clear_msg() {$('#msg').text('');}
function update_sno(){
if(parseInt($('#sn2 option:selected').val())>0){
$('#sno').selectmenu('enable'); 
}else{$('#sno').selectmenu('disable');}
}
function update_cld(){
if(eval_cb('#cld')) $('.cld').show();
else $('.cld').hide();
}
function update_ct(){
if(eval_cb('#blynk')&&prev_ct!=1){$('#bdmn').val('blynk.openthings.io');$('#bprt').val(8080);prev_ct=1;}
else if(eval_cb('#otc')&&prev_ct!=2){$('#bdmn').val('ws.cloud.openthings.io');$('#bprt').val(80);prev_ct=2;}
}
function update_mqtt(){
if(eval_cb('#mqen')) $('.mqt').show();
else $('.mqt').hide();
}
function update_email(){
if(eval_cb('#emen')) $('.email').show();
else $('.email').hide();
}
function update_sfi(){
if(eval_cb('#sf_con')) $('#tbl_cmr').show();
else $('#tbl_cmr').hide();
}
function show_msg(s) {$('#msg').text(s).css('color','red'); setTimeout(clear_msg, 2000);}
function goback() {history.back();}
function eval_cb(n)  {return $(n).is(':checked')?1:0;}
function update_ckey(){
if(eval_cb('#cb_key')) $('.ckey').show();
else $('.ckey').hide();
}
function update_usi(){
if(eval_cb('#usi')) $('.si').show();
else $('.si').hide();
}
function toggle_opt() {
$('#div_basic').hide();
$('#div_cloud').hide();
$('#div_other').hide();
if(eval_cb('#basic')) $('#div_basic').show();
if(eval_cb('#cloud')) {$('#div_cloud').show(); update_cld(); update_mqtt(); update_email();}
if(eval_cb('#other')) {$('#div_other').show(); update_usi();}
}
$('#btn_back').click(function(e){
e.preventDefault(); goback();
});
var comm;
function bc(n,e=0) {comm+='&'+n+'=';
if(e) comm+=encodeURIComponent($('#'+n).val());
else comm+=$('#'+n).val();
}
function cbt(n,v=true) {$('#'+n).attr('checked',v).checkboxradio('refresh');}
$('#btn_submit').click(function(e){
e.preventDefault();
if(confirm('Submit changes?')) {
comm='co?dkey='+encodeURIComponent($('#dkey').val());
bc('sn1');bc('sn2');bc('sno');bc('dth');bc('vth');bc('riv');bc('bas');bc('alm');
bc('lsz');bc('tsn');bc('htp');bc('cdt');bc('dri');bc('ati');bc('atib');
comm+='&aoo='+($('#aoo').is(':checked')?1:0);
comm+='&secv='+$('input[name="secv"]:checked').val();
comm+='&sto='+eval_cb('#to_cap');
comm+='&sfi='+eval_cb('#sf_con');
if(eval_cb('#sf_con')) bc('cmr');
var ato=0;
for(var i=1;i>=0;i--) { ato=(ato<<1)+eval_cb('#ato'+i); }
comm+='&ato='+ato;
var atob=0;
for(var i=1;i>=0;i--) { atob=(atob<<1)+eval_cb('#atob'+i); }
comm+='&atob='+atob;
var noto=0;
for(var i=1;i>=0;i--) { noto=(noto<<1)+eval_cb('#noto'+i); }
comm+='&noto='+noto;
if(eval_cb('#cld')) {comm+='&cld='+(eval_cb('#blynk')?1:2);
if($('#auth').val().length<32) {show_msg('Cloud token is too short!');return;}}
else comm+='&cld=0';
bc('name',1);bc('auth',1);bc('bdmn',1);bc('iftt',1);
bc('mqtt',1);bc('mqur',1);bc('mqtp',1);bc('mqpt');
if($('#mqpw').val().length>0) bc('mqpw',1);
comm+='&mqen='+(eval_cb('#mqen')?1:0);
bc('smtp',1);bc('send',1);bc('apwd',1);bc('recp',1);bc('sprt');
comm+='&emen='+(eval_cb('#emen')?1:0);
bc('bprt');bc('ntp1',1);bc('host',1);
if($('#cb_key').is(':checked')) {
if(!$('#nkey').val()) {
if(!confirm('New device key is empty. Are you sure?')) return;
}
bc('nkey',1);bc('ckey',1);
}
if($('#usi').is(':checked')) {
comm+='&usi=1';
bc('dvip');bc('gwip')+bc('subn');bc('dns1');
} else {
comm+='&usi=0';
}
$.getJSON(comm, function(jd) {
if(jd.result!=1) {
if(jd.result==2) show_msg('Check device key and try again.');
else show_msg('Error code: '+jd.result+', item: '+jd.item);
} else {
$('#msg').html('<font color=green>Options are saved. Many options may need<br>a reboot to take effect. If you changed log<br>size, please Clear Log for it to take effect.</font>');
setTimeout(goback, 4000);
}
});
}
});
$(document).ready(function() {
$('#secv').hide();
$.getJSON('jo', function(jd){
$('#fwv').text((jd.fwv/100>>0)+'.'+(jd.fwv/10%10>>0)+'.'+(jd.fwv%10>>0));
$('#alm').val(jd.alm).selectmenu('refresh');
if(jd.aoo>0) cbt('aoo');
$('#lsz').val(jd.lsz).selectmenu('refresh');
$('#tsn').val(jd.tsn).selectmenu('refresh');
$('#sn1').val(jd.sn1).selectmenu('refresh');
$('#sn2').val(jd.sn2).selectmenu('refresh');
$('#sno').val(jd.sno).selectmenu('refresh');
update_sno();
$('#dth').val(jd.dth);
$('#vth').val(jd.vth);
$('#riv').val(jd.riv);
$('#bas').val(jd.bas);
$('#htp').val(jd.htp);
$('#cdt').val(jd.cdt);
$('#dri').val(jd.dri);
if(jd.has_swrx){
$('#secv').show();
if(jd.secv==2) cbt('secv2');
else if(jd.secv==1) cbt('secv1');
else cbt('secv0');}
if(jd.sto) cbt('to_cap');
else cbt('to_ignore');
if(jd.sfi) cbt('sf_con');
else cbt('sf_med');
if(jd.cmr) $('#cmr').val(jd.cmr);
update_sfi();
$('#ati').val(jd.ati);
$('#atib').val(jd.atib);
for(var i=0;i<=1;i++) {if(jd.ato&(1<<i)) cbt('ato'+i);}
for(var i=0;i<=1;i++) {if(jd.atob&(1<<i)) cbt('atob'+i);}
for(var i=0;i<=1;i++) {if(jd.noto&(1<<i)) cbt('noto'+i);}
$('#name').val(jd.name);
if(jd.cld>0) cbt('cld');
if(jd.cld==1) {cbt('blynk');cbt('otc',false);prev_ct=1;}
else if(jd.cld==2) {cbt('otc');cbt('blynk',false);prev_ct=2;}
update_cld();
$('#auth').val(jd.auth);
$('#bdmn').val(jd.bdmn);
$('#bprt').val(jd.bprt);
if(jd.iftt) $('#iftt').val(jd.iftt);
if(jd.mqen>0) cbt('mqen');
$('#mqtt').val(jd.mqtt);
if(jd.mqur) $('#mqur').val(jd.mqur);
if(jd.mqtp) $('#mqtp').val(jd.mqtp);
if(jd.mqpt) $('#mqpt').val(jd.mqpt);
update_mqtt();
if(jd.emen>0) cbt('emen');
$('#smtp').val(jd.smtp);
$('#sprt').val(jd.sprt);
$('#send').val(jd.send);
$('#apwd').val(jd.apwd);
$('#recp').val(jd.recp);
update_email();
$('#dvip').val(jd.dvip);
$('#gwip').val(jd.gwip);
$('#subn').val(jd.subn);
$('#dns1').val(jd.dns1);
if(jd.usi>0) cbt('usi');
update_usi();
if(jd.ntp1) $('#ntp1').val(jd.ntp1);
if(jd.host) $('#host').val(jd.host);
});
});
update_ckey();
</script>
</body>
)OG";
const char sta_update_html[] PROGMEM = R"OG(<head>
<title>OpenGarage</title>
<meta name='viewport' content='width=device-width, initial-scale=1'>
<link rel='stylesheet' href='//code.jquery.com/mobile/1.3.1/jquery.mobile-1.3.1.min.css' type='text/css'>
<script src='//code.jquery.com/jquery-1.9.1.min.js' type='text/javascript'></script>
<script src='//code.jquery.com/mobile/1.3.1/jquery.mobile-1.3.1.min.js' type='text/javascript'></script>
</head>
<body>
<div data-role='page' id='page_update'>
<div data-role='header'><h3>OpenGarage Firmware Update</h3></div>
<div data-role='content'>
<form method='POST' action='' id='fm' enctype='multipart/form-data'>
<table cellspacing=4>
<tr><td><input type='file' name='file' accept='.bin' id='file'></td></tr>
<tr><td><b>Device key: </b><input type='password' name='dkey' size=16 maxlength=64 id='dkey'></td></tr>
<tr><td><label id='msg'></label></td></tr>
</table>
<div data-role='controlgroup' data-type='horizontal'>
<a href='#' data-role='button' data-inline='true' data-theme='a' id='btn_back'>Back</a>
<a href='#' data-role='button' data-inline='true' data-theme='b' id='btn_submit'>Submit</a>
</div>
</form>
</div>
</div>
<script>
function id(s) {return document.getElementById(s);}
function clear_msg() {id('msg').innerHTML='';}
function show_msg(s,t,c) {
id('msg').innerHTML=s.fontcolor(c);
if(t>0) setTimeout(clear_msg, t);
}
function goback() {history.back();}
$('#btn_back').click(function(e){
e.preventDefault(); goback();
});
$('#btn_submit').click(function(e){
var files= id('file').files;
if(files.length==0) {show_msg('Please select a file.',2000,'red'); return;}
if(id('dkey').value=='') {
if(!confirm('You did not input a device key. Are you sure?')) return;
}
var btn = id('btn_submit');
show_msg('Uploading. Please wait...',0,'green');
var fd = new FormData();
var file = files[0];
fd.append('file', file, file.name);
fd.append('dkey', id('dkey').value);
var xhr = new XMLHttpRequest();
xhr.onreadystatechange = function() {
if(xhr.readyState==4 && xhr.status==200) {
var jd=JSON.parse(xhr.responseText);
if(jd.result==1) {
show_msg('Update is successful. Rebooting. Please wait...',0,'green');
setTimeout(goback, 10000);
} else if (jd.result==2) {
show_msg('Check device key and try again.', 0, 'red');
} else {
show_msg('Update failed.',0,'red');
}
}
};
xhr.open('POST', '//' + window.location.hostname + ':8080' + window.location.pathname, true);
xhr.send(fd);
});
</script>
</body>
)OG";
