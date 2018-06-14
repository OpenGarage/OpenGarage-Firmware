const char ap_home_html[] PROGMEM = R"(<head>
<title>OpenGarage</title>
<meta name='viewport' content='width=device-width, initial-scale=1'>
</head>
<body>
<style> table, th, td {	border: 0px solid black;  border-collapse: collapse;}
table#rd th { border: 1px solid black;}
table#rd td {	border: 1px solid black; border-collapse: collapse;}</style>
<caption><b>OpenGarage WiFi Config</caption><br><br>
<table cellspacing=4 id='rd'>
<tr><td>SSID</td><td>Strength</td><td>Power Level</td></tr>
<tr><td>(Scanning...)</td></tr>
</table>
<br><br>
<table cellspacing=16>
<tr><td><input type='text' name='ssid' id='ssid' style='font-size:14pt;height:28px;'></td><td>(Your WiFi SSID)</td></tr>
<tr><td><input type='password' name='pass' id='pass' style='font-size:14pt;height:28px;'></td><td>(Your WiFi Password)</td></tr>
<tr><td><input type='text' name='auth' id='auth' style='font-size:14pt;height:28px;'></td><td><label id='lbl_auth'>(Blynk Token, Optional)</label></td></tr>
<tr><td colspan=2><p id='msg'></p></td></tr>
<tr><td><button type='button' id='butt' onclick='sf();' style='height:36px;width:180px'>Submit</button></td><td></td></tr>
</table>
<script>
function id(s) {return document.getElementById(s);}
function sel(i) {id('ssid').value=id('rd'+i).value;}
var tci;
function tryConnect() {
var xhr=new XMLHttpRequest();
xhr.onreadystatechange=function() {
if(xhr.readyState==4 && xhr.status==200) {
var jd=JSON.parse(xhr.responseText);
if(jd.ip==0) return;
var ip=''+(jd.ip%256)+'.'+((jd.ip/256>>0)%256)+'.'+(((jd.ip/256>>0)/256>>0)%256)+'.'+(((jd.ip/256>>0)/256>>0)/256>>0);
id('msg').innerHTML='<b><font color=green>Connected! Device IP: '+ip+'</font></b><br>Device is rebooting. Switch back to<br>the above WiFi network, and then<br>click the button below to redirect.'
id('butt').innerHTML='Go to '+ip;id('butt').disabled=false;
id('butt').onclick=function rd(){window.open('http://'+ip);}
clearInterval(tci);
}
}    
xhr.open('GET', 'jt', true); xhr.send();
}  
function sf() {
id('msg').innerHTML='';
var xhr=new XMLHttpRequest();
xhr.onreadystatechange=function() {
if(xhr.readyState==4 && xhr.status==200) {
var jd=JSON.parse(xhr.responseText);
if(jd.result==1) { id('butt').innerHTML='Connecting...'; id('msg').innerHTML='<font color=gray>Connecting, please wait...</font>'; tci=setInterval(tryConnect, 2000); return; }
id('msg').innerHTML='<b><font color=red>Error code: '+jd.result+', item: '+jd.item+'</font></b>'; id('butt').innerHTML='Submit'; id('butt').disabled=false;id('ssid').disabled=false;id('pass').disabled=false;id('auth').disabled=false;
}
};
var comm='cc?ssid='+encodeURIComponent(id('ssid').value)+'&pass='+encodeURIComponent(id('pass').value)+'&auth='+id('auth').value;
xhr.open('GET', comm, true); xhr.send();
id('butt').disabled=true;id('ssid').disabled=true;id('pass').disabled=true;id('auth').disabled=true;
}

function loadSSIDs() {
var xhr=new XMLHttpRequest();
xhr.onreadystatechange=function() {
if(xhr.readyState==4 && xhr.status==200) {
id('rd').deleteRow(1);
var i, jd=JSON.parse(xhr.responseText);
//TODO Sort and remove dups
for(i=0;i<jd.ssids.length;i++) {
var signalstrength= jd.rssis[i]>-71?'Ok':(jd.rssis[i]>-81?'Weak':'Poor');
var row=id('rd').insertRow(-1);
row.innerHTML ="<tr><td><input name='ssids' id='rd"+i+"' onclick='sel(" + i + ")' type='radio' value='"+jd.ssids[i]+"'>" + jd.ssids[i] + "</td>"  + "<td align='center'>"+signalstrength+"</td>" + "<td align='center'>("+jd.rssis[i] +" dbm)</td>" + "</tr>";
}
};
}
xhr.open('GET','js',true); xhr.send();
}
setTimeout(loadSSIDs, 1000);
</script>
</body>
)";
const char ap_update_html[] PROGMEM = R"(<head>
<title>OpenGarage</title>
<meta name='viewport' content='width=device-width, initial-scale=1'>
</head>
<body>
<div id='page_update'>
<div><h3>OpenGarage AP-mode Firmware Update</h3></div>
<div>
<form method='POST' action='/update' id='fm' enctype='multipart/form-data'>
<table cellspacing=4>
<tr><td><input type='file' name='file' accept='.bin' id='file'></td></tr>
<tr><td><b>Device key: </b><input type='password' name='dkey' size=16 maxlength=16 id='dkey'></td></tr>
<tr><td><label id='msg'></label></td></tr>
</table>
<button id='btn_submit' style='height:48px;'>Submit</a>
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
id('btn_submit').addEventListener('click', function(e){
e.preventDefault();
var files= id('file').files;
if(files.length==0) {show_msg('Please select a file.',2000,'red'); return;}
if(id('dkey').value=='') {
if(!confirm('You did not input a device key. Are you sure?')) return;
}
show_msg('Uploading. Please wait...',10000,'green');
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
} else if (jd.result==2) {
show_msg('Check device key and try again.', 10000, 'red');
} else {
show_msg('Update failed.',0,'red');
}
}
};
xhr.open('POST', 'update', true);
xhr.send(fd);
});
</script>
</body>
)";
const char sta_home_html[] PROGMEM = R"(<head><title>OpenGarage</title><meta name='viewport' content='width=device-width, initial-scale=1'><link rel='stylesheet' href='http://code.jquery.com/mobile/1.3.1/jquery.mobile-1.3.1.min.css' type='text/css'><script src='http://code.jquery.com/jquery-1.9.1.min.js' type='text/javascript'></script><script src='http://code.jquery.com/mobile/1.3.1/jquery.mobile-1.3.1.min.js' type='text/javascript'></script></head>
<body>
<style> table, th, td {border: 0px solid black;padding: 6px; border-collapse: collapse; }</style>
<div data-role='page' id='page_home'><div data-role='header'><h3 id='head_name'>OG</h3></div>
<div data-role='content'><div data-role='fieldcontain'>
<table><tr><td><b>Door&nbsp;State:<br></td><td><label id='lbl_status'>-</label></td>
<td rowspan="2"><img id='pic' src='' style='width:112px;height:64px;'></td>
</tr><tr><td><b><label id='lbl_vstatus1'>Vehicle&nbsp;State:&nbsp</label></b></td>
<td><label id='lbl_vstatus'>-</label></td></tr>
<tr><td><b>Distance:</b></td><td><label id='lbl_dist'>-</label></td><td></td></tr>
<tr><td><b>Read&nbsp;Count:</b></td><td><label id='lbl_beat'>-</label></td><td></td></tr>
<tr><td><b>WiFi&nbsp;Signal:</b></td><td colspan="2"><label id='lbl_rssi'>-</label></td></tr>
<tr><td><b>Device&nbsp;Key:</b></td><td colspan="2" ><input type='password' size=20 maxlength=32 name='dkey' id='dkey'></td></tr>
<tr><td colspan=3><label id='msg'></label></td></tr>
</table><br />
<div data-role='controlgroup' data-type='horizontal'>
<button data-theme='b' id='btn_click'>Button</button>  
<button data-theme='b' id='btn_opts'>Options</button>
<button data-theme='b' id='btn_log'>Show Log</button>
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
<p>&nbsp; OpenGarage Firmware <label id='fwv'>-</label>&nbsp;<a href='update' target='_top' data-role='button' data-inline=true data-mini=true>Update</a></p>
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
var comm = 'clearlog?dkey='+($('#dkey').val());
clear_msg();
$.getJSON(comm, function(jd) {
if(jd.result!=1) show_msg('Check device key and try again.',2000,'red');
else { show_msg('Log data cleared',2000,'green'); }
});
}
});  
$('#btn_rbt').click(function(e){
if(confirm('Reboot the device now?')){
var comm = 'cc?reboot=1&dkey='+($('#dkey').val());
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
var comm = 'cc?apmode=1&dkey='+($('#dkey').val());
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
$('#btn_click').click(function(e) {
var comm = 'cc?click=1&dkey='+($('#dkey').val());
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
});
$(document).ready(function() { show(); si=setInterval('show()', 5000); });
function show() {
$.getJSON('jc', function(jd) {
$('#fwv').text('v'+(jd.fwv/100>>0)+'.'+(jd.fwv/10%10>>0)+'.'+(jd.fwv%10>>0));
$('#lbl_dist').text(jd.dist +' (cm)').css('color', jd.dist==450?'red':'black');
$('#lbl_status').text(jd.door?'OPEN':'CLOSED').css('color',jd.door?'red':'green'); 
//Hide or Show vehicle info
if (jd.vehicle >=2){
$('#lbl_vstatus1').hide();
$('#lbl_vstatus').text('');
}else {
$('#lbl_vstatus1').show()
$('#lbl_vstatus').text(jd.vehicle & !jd.door?'Present':(!jd.vehicle & !jd.door?'Absent':''));
}
//Use correct graphics
if (jd.vehicle>=3){ //3 is disabled
$('#pic').attr('src', (jd.door?'https://github.com/OpenGarage/OpenGarage-Firmware/raw/master/icons/DoorOpen.png':'https://github.com/OpenGarage/OpenGarage-Firmware/raw/master/icons/DoorShut.png'));
}else{
$('#pic').attr('src', jd.door?'https://github.com/OpenGarage/OpenGarage-Firmware/raw/master/icons/Open.png':(jd.vehicle?'https://github.com/OpenGarage/OpenGarage-Firmware/raw/master/icons/ClosedPresent.png':'https://github.com/OpenGarage/OpenGarage-Firmware/raw/master/icons/ClosedAbsent.png'));
}
$('#lbl_beat').text(jd.rcnt);
$('#lbl_rssi').text((jd.rssi>-71?'Good':(jd.rssi>-81?'Weak':'Poor')) +' ('+ jd.rssi +' dBm)');
$('#head_name').text(jd.name);
$('#btn_click').html(jd.door?'Close Door':'Open Door').button('refresh');
});
}
</script>
</body>
)";
const char sta_logs_html[] PROGMEM = R"(<head>
<title>OpenGarage</title>
<meta name='viewport' content='width=device-width, initial-scale=1'>
<link rel='stylesheet' href='http://code.jquery.com/mobile/1.3.1/jquery.mobile-1.3.1.min.css' type='text/css'>
<script src='http://code.jquery.com/jquery-1.9.1.min.js' type='text/javascript'></script>
<script src='http://code.jquery.com/mobile/1.3.1/jquery.mobile-1.3.1.min.js' type='text/javascript'></script>
</head>
<body>
<div data-role='page' id='page_log'>
<div data-role='header'><h3><label id='lbl_name'></label> Log</h3></div>    
<div data-role='content'>
<p>Below are the most recent <label id='lbl_nr'></label> records</p>
<p>Current time is <label id='lbl_time'></label></p>
<div data-role="controlgroup" data-type="horizontal">
<button data-theme="b" id="btn_back">Back</button>
</div>
<div data-role='fieldcontain'>
<table id='tab_log' border='1' cellpadding='4' style='border-collapse:collapse;'><tr><td align='center'><b>Event</b></td><td align='center'><b>DateTime</b></td><td align='center'><b>Details</b></td></tr></table>
</div>
</div>
</div>
<script>
var curr_time = 0;
var date = new Date();
$("#btn_back").click(function(){history.back();});
$(document).ready(function(){
show_log();
setInterval(show_time, 1000);
});
function show_time() {
curr_time ++;
date.setTime(curr_time*1000);
$('#lbl_time').text(date.toLocaleString());
}
function show_log() {
$.getJSON('jl', function(jd) {
$('#lbl_name').text(jd.name);
curr_time = jd.time;
$('#tab_log').find('tr:gt(0)').remove();
var logs=jd.logs;
logs.sort(function(a,b){return b[0]-a[0];});
$('#lbl_nr').text(logs.length);
var ldate = new Date();
for(var i=0;i<logs.length;i++) {
ldate.setTime(logs[i][0]*1000);
var r='<tr></td><td align="left"><img id="pic" src="data:image/png;base64,' + (logs[i][1]?'iVBORw0KGgoAAAANSUhEUgAAACAAAAAZCAYAAABQDyyRAAAABmJLR0QA/wD/AP+gvaeTAAAACXBIWXMAAA7EAAAOxAGVKw4bAAAAB3RJTUUH4gEBFiwkDx0OQQAAArtJREFUSMfFlstLlFEYxn/nfN/5HHVmskTF1LQCcRMRRBAZClbQH1CLatUiqm2rWrUP2rRqUZuKVgUREUFR2M2yMhC6iGSafl3QaZxJ53K+75wWSReaMTUdn+XhXJ73Pe/7vI94/e6mrXj2nPhIkpCAkkAIJuvL0Vu24cbu3qdhxAMRpZSoTkHq40NkjS9AUHoIiKc8pBcqlg0C5IIOGgN5jY555FeUYXUeQrOgq9x57bagRZ78xvXYjnai1a1gLdnECJmnd4g/G8YJBQixSASCEC1y0NhI0NqMamlDtLSiyCD7X5F6cRkroKJ6LdGuPbA7RtZ/izMwRDA0gDfk4xgPlFMgmB8khT150hb+H0l61yYimztRbhSMJeX3Ibof4ExpdFc7sXVbkAi++f3I7kfocBq5dTuVzRuRjiLLFEFPN5W3esDMPGMtVhj82DgqdIsTsLEomcP7cB73ksslkfEqIjVrsc1NqEhV4YTpaRj9QDo9zGR/L/HBCfTBA5R37EQlUugwwMtC5De5KZ4Ba8Eavh3aQ7RhQ7GSKFbcACQIWCXUAmtACKzrorw4uWCS/g8XEDNXWwFl77Os6ZvCyL/P8fkLK48ep6Jh9SJ0gbUYE9DWuJeoqv213gKms3D02Zc9MDGBXQwCAoHMhHw6tp/J6XAunYoz6lN+7sb/tWGgIJ/4SKgzlMsyGq88QSXTc9eMQq1XAH8roZWkKwMmrp9DnT2Pm5z6sewszcD4IwOBJxkf6yNy5hp1CYPxFFos7aT6SSBTFpC8ep76Xr+k88hFOIznhpEXr1A/+LXkA9Edy72h9vRlVNawHHDrTl3C1bNImuOA687ftLjuHLfpWTTAGJx73YiqamR+nn7x9m2cprp/exJbXNIXzYBiiz8h7VL7QTt7fPJLU2TZLKF2QPonDhLK5SHw5sgOvgNSpAhhsRiYRQAAAABJRU5ErkJggg==':'iVBORw0KGgoAAAANSUhEUgAAACAAAAAZCAYAAABQDyyRAAAABmJLR0QA/wD/AP+gvaeTAAAACXBIWXMAAAsTAAALEwEAmpwYAAAAB3RJTUUH4gEBFisqp+S1gQAAAuRJREFUSMfFlt1LFFEYxn9nZpx1dScV11w1NzNFQ7QouiiiDwq6kQiCoAir+6B/pbrLKLqqm6CrIIiMro2+sw9qzVXXdN00V3fc2dlzuuiL0t2OW+h7NcM5PO/znvd5n3NE/0BXA3AR2MbqxhBw3gJuAPtZ/WgHNhlrlPxH9BiscZRIQODLRRQSAD/vAqIkJGslmz1/nopAmJbwIbY0HSdStR2A5NwrXiduMjx1j/nsJ2zL0S+lf6BLFV5WZP004VAn0fABupv7cMqbUEoSm7rLm8QtFJL2yBHa6nsxDRvXS/Esfo2R6Qck0y+xLQdR5HQKEhAIdrSeo6f5LKZhk/GSvP90h+HkPWyrkvb6I7RFegGIpx7yduI2Xu4LkeqddDQcJVTeiELyPH6dR7GL5HwfXyqkBKUUUn1rWsEWmIZNa91hBmOXmJgdpKZyM50Nx+iJnlmyN1q7j2jtPgAyXpKnI1cZm45hiSZ2dZzgbfwCrpdBiKVKKdICRcZLcXL3feqcrpJVfvqSTXVlI0KIlYpQYBkBApZDLp/hycfLOMHGn2sLrk82tzz3ucwke7b04QTDlFnlBZNrT0Eu71Jf1cPG8EGtqmOTg2SyszjB8P8YQwEYDI0+xhT1WgTGU0N0bNj7bz5gCJjL5Mn5ElOAbYaoqoxogYaCtaBk6QSkVMQmFkmlXdT3NkslUVIPVEqp7YzW71ULkl88EtMehuCXeATMLkyQmHmjBZqajxMNd6+MgFKKd2MuizmJ8Qd5pRQt67fT2aTX16C9DqnyegSEADcr+TDu/pTccjKMTQ5iGKYW6FjqFbs7TuoRmEn7xCcXMQ1R5EZQVASqWRdcrwVaYY8DSo/A2FS2aHIhBEoponVbaW/YpXfDCQOlOwVFTIoyM8DAyyuYwmJmIcHo9Ast0M/pUQJlDqHyGmyrojjZ81db/nJWapmvv1tXob8SnVAHqsQnmRAMr+GTcNYA+oD8WmQXglNfAfQMC/0EmCdPAAAAAElFTkSuQmCC') +'" style="width:20px;height:15px;">'+(logs[i][1]?' Opened':' Closed')+'<td align="center">'+ldate.toLocaleString()+'</td><td align="center">'+logs[i][2]+' cm</td></tr>';
$('#tab_log').append(r);
}
});
setTimeout(show_log, 10000);
}
</script>
</body>
)";
const char sta_options_html[] PROGMEM = R"(<head><title>OpenGarage</title><meta name='viewport' content='width=device-width, initial-scale=1'><link rel='stylesheet' href='http://code.jquery.com/mobile/1.3.1/jquery.mobile-1.3.1.min.css' type='text/css'><script src='http://code.jquery.com/jquery-1.9.1.min.js' type='text/javascript'></script><script src='http://code.jquery.com/mobile/1.3.1/jquery.mobile-1.3.1.min.js' type='text/javascript'></script></head>
<body>
<style> table, th, td { border: 0px solid black; padding: 1px; border-collapse: collapse; } </style>
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
<tr><td><b>Sensor Type:</b></td><td>
<select name='mnt' id='mnt' data-mini='true' onChange='disable_dth()'>
<option value=0>Ceiling Mount</option>
<option value=1>Side Mount</option>
<option value=2>Switch (Low Mount)</option>
<option value=3>Switch (High Mount)</option>
</select></td></tr> 
<tr><td><b>Door Threshold (cm): </b></td><td><input type='text' size=3 maxlength=4 id='dth' data-mini='true' value=0></td></tr>
<tr><td><b>Vehicle Threshold (cm):</b><br><small>(Set to 0 to disable) </small></td><td><input type='text' size=3 maxlength=4 id='vth' data-mini='true' value=0 ></td></tr>
<tr><td><b>Read Interval (s):</b></td><td><input type='text' size=3 maxlength=3 id='riv' data-mini='true' value=0></td></tr>
<tr><td><b>Click Time (ms):</b></td><td><input type='text' size=3 maxlength=5 id='cdt' value=0 data-mini='true'></td></tr>
<tr><td><b>Sound Alarm:</b></td><td>
<select name='alm' id='alm' data-mini='true'>      
<option value=0>Disabled</option>
<option value=1>5 seconds</option>                  
<option value=2>10 seconds</option>      
</select></td></tr>
</table>
</div>
<div id='div_cloud' style='display:none;'>
<table cellpadding=1>
<tr><td><b>Blynk Token:<a href='#BlynkInfo' data-rel='popup' data-role='button' data-inline='true' data-transition='pop' data-icon='info' data-theme='c' data-iconpos='notext'>Setup info</a><div data-role='popup' id='BlynkInfo' class='ui-content' data-theme='b' style='max-width:320px;'><p>Blynk provides remote access and monitoring. Install the app and use this QR to configure <a href='https://github.com/OpenGarage/OpenGarage-Firmware/blob/master/OGBlynkApp/og_blynk_1.0.png' target='_blank'>Blynk QR</a></p></div></b></td><td><input type='text' size=20 maxlength=32 id='auth' data-mini='true' value='-'></td></tr>
<tr><td><b>IFTTT Key:<a href='#ifttInfo' data-rel='popup' data-role='button' data-inline='true' data-transition='pop' data-icon='info' data-theme='c' data-iconpos='notext'>Learn more</a><div data-role='popup' id='ifttInfo' class='ui-content' data-theme='b' style='max-width:320px;'><p><a href='https://ifttt.com' target='_blank'>IFTTT</a> provides additional notification options (e.g. SMS, email) besides Blynk.</p></div></b></td><td><input type='text' size=20 maxlength=64 id='iftt' data-mini='true' value='-'></td></tr>
<tr><td><b>MQTT Server:<a href='#mqttInfo' data-rel='popup' data-role='button' data-inline='true' data-transition='pop' data-icon='info' data-theme='c' data-iconpos='notext'>Learn more</a><div data-role='popup' id='mqttInfo' class='ui-content' data-theme='b' style='max-width:320px;'><p>MQTT provides additional workflow options through tools like NodeRed (e.g. SMS, email).</p></div></b></td><td><input type='text' size=16 maxlength=20 id='mqtt' data-mini='true' value=''></td></tr>
</table> 
<table>
<tr><td colspan=4><b>Choose Notifications:</b></td></tr>
<tr><td><input type='checkbox' id='noto0' data-mini='true'><label for='noto0'>Door<br> Open</label></td><td><input type='checkbox' id='noto1' data-mini='true' ><label for='noto1'>Door<br> Close</label></td>
<td><input type='checkbox' id='noto2' data-mini='true' disabled><label for='noto2'>Vehicle<br> Leave</label></td><td><input type='checkbox' id='noto3' data-mini='true' disabled ><label for='noto3'>Vehicle<br> Arrive</label></td></tr>
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
<tr><td><b>HTTP Port:</b></td><td><input type='text' size=5 maxlength=5 id='htp' value=0 data-mini='true'></td></tr>
<tr><td colspan=2><input type='checkbox' id='usi' data-mini='true'><label for='usi'>Use Static IP</label></td></tr>
<tr><td><b>Device IP:</b></td><td><input type='text' size=15 maxlength=15 id='dvip' data-mini='true' disabled></td></tr>
<tr><td><b>Gateway IP:</b></td><td><input type='text' size=15 maxlength=15 id='gwip' data-mini='true' disabled></td></tr>
<tr><td><b>Subnet:</b></td><td><input type='text' size=15 maxlength=15 id='subn' data-mini='true' disabled></td></tr> 
<tr><td colspan=2><input type='checkbox' id='cb_key' data-mini='true'><label for='cb_key'>Change Device Key</label></td></tr>
<tr><td><b>New Key:</b></td><td><input type='password' size=24 maxlength=32 id='nkey' data-mini='true' disabled></td></tr>
<tr><td><b>Confirm:</b></td><td><input type='password' size=24 maxlength=32 id='ckey' data-mini='true' disabled></td></tr>      
</table>
</div>
<br />
<table cellpadding=2>
<tr><td><b>Device Key:</b></td><td><input type='password' size=24 maxlength=32 id='dkey' data-mini='true'></td></tr>
<tr><td colspan=2><p id='msg'></p></td></tr>
</table>
<div data-role='controlgroup' data-type='horizontal'>
<a href='#' data-role='button' data-inline='true' data-theme='a' id='btn_back'>Back</a>
<a href='#' data-role='button' data-inline='true' data-theme='b' id='btn_submit'>Submit</a>      
</div>
<table>
</table>
</div>
<div data-role='footer' data-theme='c'>
<p>&nbsp; OpenGarage Firmware <label id='fwv'>-</label>&nbsp;<a href='update' target='_top' data-role='button' data-inline=true data-mini=true>Update</a></p>
</div> 
</div>
<script>
function clear_msg() {$('#msg').text('');}  
function disable_dth() {
if (parseInt($('#mnt option:selected').val()) >1){
$('#dth').textinput('disable'); 
$('#vth').textinput('disable'); 
}else{$('#dth').textinput('enable');}
}
function show_msg(s) {$('#msg').text(s).css('color','red'); setTimeout(clear_msg, 2000);}
function goback() {history.back();}
function eval_cb(n)  {return $(n).is(':checked')?1:0;}
$('#cb_key').click(function(e){
$('#nkey').textinput($(this).is(':checked')?'enable':'disable');
$('#ckey').textinput($(this).is(':checked')?'enable':'disable');
});
$('#usi').click(function(e){
$('#dvip').textinput($(this).is(':checked')?'enable':'disable');
$('#gwip').textinput($(this).is(':checked')?'enable':'disable');
$('#subn').textinput($(this).is(':checked')?'enable':'disable');      
});
function toggle_opt() {
$('#div_basic').hide();
$('#div_cloud').hide();
$('#div_other').hide();
if(eval_cb('#basic')) $('#div_basic').show();
if(eval_cb('#cloud')) $('#div_cloud').show();
if(eval_cb('#other')) $('#div_other').show();
}
$('#btn_back').click(function(e){
e.preventDefault(); goback();
});
$('#btn_submit').click(function(e){
e.preventDefault();
if(confirm('Submit changes?')) {
var comm='co?dkey='+encodeURIComponent($('#dkey').val());
comm+='&mnt='+$('#mnt').val();
comm+='&dth='+$('#dth').val();
comm+='&vth='+$('#vth').val();
comm+='&riv='+$('#riv').val();
comm+='&alm='+$('#alm').val();
comm+='&htp='+$('#htp').val();
comm+='&cdt='+$('#cdt').val();
comm+='&ati='+$('#ati').val();
comm+='&atib='+$('#atib').val();
var ato=0;
for(var i=1;i>=0;i--) { ato=(ato<<1)+eval_cb('#ato'+i); }
comm+='&ato='+ato;
var atob=0;
for(var i=1;i>=0;i--) { atob=(atob<<1)+eval_cb('#atob'+i); }
comm+='&atob='+atob;
var noto=0;
for(var i=1;i>=0;i--) { noto=(noto<<1)+eval_cb('#noto'+i); }
comm+='&noto='+noto;
comm+='&name='+encodeURIComponent($('#name').val());
comm+='&auth='+encodeURIComponent($('#auth').val());
comm+='&iftt='+encodeURIComponent($('#iftt').val());
comm+='&mqtt='+encodeURIComponent($('#mqtt').val());
if($('#cb_key').is(':checked')) {
if(!$('#nkey').val()) {
if(!confirm('New device key is empty. Are you sure?')) return;
}
comm+='&nkey='+encodeURIComponent($('#nkey').val());
comm+='&ckey='+encodeURIComponent($('#ckey').val());
}
if($('#usi').is(':checked')) {
comm+='&usi=1&dvip='+($('#dvip').val())+'&gwip='+($('#gwip').val());
} else {
comm+='&usi=0';
}
$.getJSON(comm, function(jd) {
if(jd.result!=1) {
if(jd.result==2) show_msg('Check device key and try again.');
else show_msg('Error code: '+jd.result+', item: '+jd.item);
} else {
$('#msg').html('<font color=green>Options are successfully saved. Note that<br>changes to some options may require a reboot.</font>');
setTimeout(goback, 4000);
}
});
}
});
$(document).ready(function() {
$.getJSON('jo', function(jd) {
$('#fwv').text('v'+(jd.fwv/100>>0)+'.'+(jd.fwv/10%10>>0)+'.'+(jd.fwv%10>>0));
$('#alm').val(jd.alm).selectmenu('refresh');
$('#mnt').val(jd.mnt).selectmenu('refresh');
if(jd.mnt>1) $('#dth').textinput('disable'); 
if(jd.mnt>0) $('#vth').textinput('disable'); 
$('#dth').val(jd.dth);
$('#vth').val(jd.vth);
$('#riv').val(jd.riv);
$('#htp').val(jd.htp);
$('#cdt').val(jd.cdt);
$('#ati').val(jd.ati);
$('#atib').val(jd.atib);
for(var i=0;i<=1;i++) {if(jd.ato&(1<<i)) $('#ato'+i).attr('checked',true).checkboxradio('refresh');}
for(var i=0;i<=1;i++) {if(jd.atob&(1<<i)) $('#atob'+i).attr('checked',true).checkboxradio('refresh');}
for(var i=0;i<=1;i++) {if(jd.noto&(1<<i)) $('#noto'+i).attr('checked',true).checkboxradio('refresh');}
$('#name').val(jd.name);
$('#auth').val(jd.auth);
$('#iftt').val(jd.iftt);
$('#mqtt').val(jd.mqtt);
$('#dvip').val(jd.dvip);
$('#gwip').val(jd.gwip);
$('#subn').val(jd.subn);
if(jd.usi>0) $('#usi').attr('checked',true).checkboxradio('refresh');
$('#dvip').textinput(jd.usi>0?'enable':'disable');
$('#gwip').textinput(jd.usi>0?'enable':'disable');
$('#subn').textinput(jd.usi>0?'enable':'disable');      
});
});
</script>
</body>
)";
const char sta_update_html[] PROGMEM = R"(<head>
<title>OpenGarage</title>
<meta name='viewport' content='width=device-width, initial-scale=1'>
<link rel='stylesheet' href='http://code.jquery.com/mobile/1.3.1/jquery.mobile-1.3.1.min.css' type='text/css'>
<script src='http://code.jquery.com/jquery-1.9.1.min.js' type='text/javascript'></script>
<script src='http://code.jquery.com/mobile/1.3.1/jquery.mobile-1.3.1.min.js' type='text/javascript'></script>
</head>
<body>
<div data-role='page' id='page_update'>
<div data-role='header'><h3>OpenGarage Firmware Update</h3></div>
<div data-role='content'>
<form method='POST' action='/update' id='fm' enctype='multipart/form-data'>
<table cellspacing=4>
<tr><td><input type='file' name='file' accept='.bin' id='file'></td></tr>
<tr><td><b>Device key: </b><input type='password' name='dkey' size=16 maxlength=16 id='dkey'></td></tr>
<tr><td><label id='msg'></label></td></tr>
</table>
<a href='#' data-role='button' data-inline='true' data-theme='a' id='btn_back'>Back</a>
<a href='#' data-role='button' data-inline='true' data-theme='b' id='btn_submit'>Submit</a>
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
xhr.open('POST', 'update', true);
xhr.send(fd);
});
</script>
</body>
)";
