/* ============================================================
   WEB UI
   ------------------------------------------------------------
   MAIN PAGE : signs (peace, middle finger, rock...) aur
               gestures (wave, count, grab...) ke bade buttons.
   SETTINGS  : gear icon dabane par popout khulta hai -- usme
               har ungli ka alag control (degree slider).

   Sab kuch DEGREES mein hai. Microseconds sirf settings ke
   chhote dim text mein -- wo internal unit hai.

   Buttons ki list firmware se aati hai (/api/list), isliye
   main.cpp mein naya sign add karne par yeh page apne aap
   update ho jata hai -- yahan kuch nahi badalna.

   Poora page ESP32 ki flash mein PROGMEM par padta hai.
   Koi internet ya CDN nahi chahiye.
   ============================================================ */
#pragma once
#include <pgmspace.h>

const char PAGE_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="hi">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Robotic Hand</title>
<style>
:root{--bg:#0e1116;--card:#171c23;--line:#272f3a;--txt:#e6edf3;
      --dim:#8b98a5;--acc:#4da3ff;--ok:#3fb950;--warn:#ff7b52}
*{box-sizing:border-box;margin:0;padding:0}
body{background:var(--bg);color:var(--txt);padding:16px;max-width:680px;margin:0 auto;
     font:15px/1.5 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif}
button{font:inherit;color:var(--txt);background:var(--card);border:1px solid var(--line);
       border-radius:10px;padding:10px 14px;cursor:pointer;-webkit-tap-highlight-color:transparent}
button:active{transform:translateY(1px)}

.hd{display:flex;justify-content:space-between;align-items:flex-start;margin-bottom:18px}
h1{font-size:19px;letter-spacing:.6px}
.sub{color:var(--dim);font-size:13px;margin-top:2px}
.dot{display:inline-block;width:8px;height:8px;border-radius:50%;background:var(--warn);
     margin-right:6px;vertical-align:middle}
.dot.live{background:var(--ok)}
.gear{flex:0 0 auto;width:44px;height:44px;padding:0;font-size:19px;line-height:1}

.lbl{color:var(--dim);font-size:11px;text-transform:uppercase;letter-spacing:.8px;margin-bottom:8px}
.grid{display:grid;grid-template-columns:repeat(3,1fr);gap:8px;margin-bottom:22px}
.grid button{display:flex;flex-direction:column;align-items:center;gap:4px;padding:14px 4px}
.grid button.on{border-color:var(--acc);background:#13202e}
.ic{font-size:26px;line-height:1.1}
.tx{font-size:11px;color:var(--dim);letter-spacing:.4px;text-align:center}
.grid button.on .tx{color:var(--acc)}
.stopbar{display:flex;gap:8px;margin:-12px 0 22px}
.stopbar button{flex:1}
.danger{border-color:var(--warn);color:var(--warn)}

/* ---- settings popout ---- */
.mask{position:fixed;inset:0;background:rgba(0,0,0,.72);display:none;z-index:9;
      overflow-y:auto;padding:16px}
.mask.show{display:block}
.sheet{background:var(--bg);border:1px solid var(--line);border-radius:16px;
       max-width:660px;margin:0 auto;padding:16px}
.shd{display:flex;justify-content:space-between;align-items:center;margin-bottom:16px}
.shd h2{font-size:16px;letter-spacing:.5px}
.x{width:38px;height:38px;padding:0;font-size:17px;line-height:1}
.bar{display:flex;gap:8px;margin-bottom:18px}
.bar button{flex:1;font-variant-numeric:tabular-nums}
.bar button.on{border-color:var(--acc);color:var(--acc);background:#13202e}
.row{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:12px;margin-bottom:10px}
.top{display:flex;justify-content:space-between;align-items:baseline;margin-bottom:10px}
.nm{font-weight:600;letter-spacing:.3px}
.ch{color:var(--dim);font-size:12px;margin-left:7px}
.deg{color:var(--acc);font-size:22px;font-weight:700;font-variant-numeric:tabular-nums}
.us{color:var(--dim);font-size:11px;margin-left:7px;font-variant-numeric:tabular-nums}
.ctl{display:flex;gap:10px;align-items:center}
.big{flex:0 0 58px;height:44px;font-size:18px;font-weight:700;line-height:1}
input[type=range]{flex:1;accent-color:var(--acc);height:30px}
.chips{display:flex;gap:6px;margin-top:10px}
.chips button{flex:1;padding:7px 0;font-size:12px;color:var(--dim);border-radius:8px}
.chips button:hover{color:var(--acc);border-color:var(--acc)}
.cal{margin-top:10px;padding-top:10px;border-top:1px dashed var(--line)}
.calinfo{color:var(--dim);font-size:11px;letter-spacing:.4px;margin-bottom:7px;
         font-variant-numeric:tabular-nums}
.calinfo b{color:var(--txt);font-weight:600}
.calbtn{display:flex;gap:6px}
.calbtn button{flex:1;padding:8px 0;font-size:11px;letter-spacing:.4px;border-radius:8px}
.calbtn button:hover{border-color:var(--acc);color:var(--acc)}
.save{border-color:var(--ok);color:var(--ok)}
.g{display:flex;gap:8px;margin-top:16px}
.g button{flex:1}
footer{color:var(--dim);font-size:11px;margin:18px 0 8px;text-align:center}
</style>
</head>
<body>

<div class="hd">
  <div>
    <h1>ROBOTIC HAND</h1>
    <div class="sub"><span class="dot" id="dot"></span><span id="stat">connecting...</span></div>
  </div>
  <button class="gear" onclick="openCfg()" title="Settings">&#9881;</button>
</div>

<div class="lbl">Signs</div>
<div class="grid" id="signs"></div>

<div class="lbl">Gestures</div>
<div class="grid" id="seqs"></div>
<div class="stopbar">
  <button class="danger" onclick="stopSeq()">STOP</button>
</div>

<!-- ================= SETTINGS POPOUT ================= -->
<div class="mask" id="mask" onclick="maskClick(event)">
 <div class="sheet">
  <div class="shd">
    <h2>PER-FINGER CONTROL</h2>
    <button class="x" onclick="closeCfg()">&#10005;</button>
  </div>

  <div class="lbl">Step size &mdash; ek click par kitne degree</div>
  <div class="bar" id="steps"></div>

  <div id="list"></div>

  <div class="g">
    <button onclick="sign('open')">OPEN</button>
    <button onclick="sign('fist')">FIST</button>
    <button class="danger" onclick="g('relax')">RELAX</button>
  </div>
  <div class="g">
    <button id="idlebtn" onclick="cycleIdle()">AUTO-RELAX</button>
  </div>
  <div class="lbl" style="margin-top:18px">Calibration &mdash; flash mein pakka karo</div>
  <div class="g" style="margin-top:0">
    <button class="save" onclick="calSave()">SAVE</button>
    <button onclick="calLoad()">RELOAD</button>
    <button onclick="calDef()">DEFAULTS</button>
  </div>
  <footer id="foot">&nbsp;</footer>
 </div>
</div>

<script>
var step = 5, built = false, busy = false, cfgOpen = false, curIdle = 8, curSeq = '';
var PRESET = [0, 45, 90, 135, 180];

// id -> icon. Firmware ASCII hi bhejta hai, emoji yahan lagte hain.
var IC = {
  open:'✋', fist:'✊', peace:'✌️', middle:'🖕',
  thumbsup:'👍', rock:'🤘', ok:'👌',
  point:'☝️', callme:'🤙', gun:'🔫',
  three:'3️⃣', four:'4️⃣',
  wave:'👋', hello:'🙋', count:'🔢',
  grab:'🤚', drum:'🥁'
};

/* ---------- main page buttons ---------- */
function buildList(L){
  var h = '';
  for (var i = 0; i < L.signs.length; i++){
    var x = L.signs[i];
    h += '<button id="sg_' + x.id + '" onclick="sign(\'' + x.id + '\')">'
       + '<span class="ic">' + (IC[x.id] || '●') + '</span>'
       + '<span class="tx">' + x.l + '</span></button>';
  }
  document.getElementById('signs').innerHTML = h;

  h = '';
  for (var j = 0; j < L.seqs.length; j++){
    var y = L.seqs[j];
    h += '<button id="sq_' + y.id + '" onclick="seq(\'' + y.id + '\')">'
       + '<span class="ic">' + (IC[y.id] || '▶') + '</span>'
       + '<span class="tx">' + y.l + '</span></button>';
  }
  document.getElementById('seqs').innerHTML = h;
}

/* ---------- settings rows ---------- */
function mk(f, i){
  var h = '<div class="row">'
    + '<div class="top">'
    + '<div><span class="nm">' + f.n + '</span><span class="ch">CH' + f.ch + '</span></div>'
    + '<div><span class="deg" id="d' + i + '">--</span>'
    + '<span class="us" id="u' + i + '"></span></div>'
    + '</div><div class="ctl">'
    + '<button class="big" onclick="nudge(' + i + ',-1)">&#9660;</button>'
    + '<input type="range" id="s' + i + '" min="' + f.lod + '" max="' + f.hid + '" '
    + 'onchange="slide(' + i + ',this.value)">'
    + '<button class="big" onclick="nudge(' + i + ',1)">&#9650;</button>'
    + '</div><div class="chips">';
  for (var k = 0; k < PRESET.length; k++){
    var p = PRESET[k];
    if (p < f.lod || p > f.hid) continue;
    h += '<button onclick="go(' + i + ',' + p + ')">' + p + '&deg;</button>';
  }
  h += '</div>'
     + '<div class="cal"><div class="calinfo">'
     + 'OPEN <b id="o' + i + '">--</b>'
     + ' &nbsp;&middot;&nbsp; CLOSED <b id="c' + i + '">--</b></div>'
     + '<div class="calbtn">'
     + '<button onclick="setOpen(' + i + ')">YAHI = OPEN</button>'
     + '<button onclick="setClose(' + i + ')">YAHI = CLOSED</button>'
     + '</div></div>';
  return h + '</div>';
}

function steps(){
  var o = [1, 5, 15, 30], h = '';
  for (var i = 0; i < o.length; i++){
    h += '<button class="' + (o[i] == step ? 'on' : '') + '" onclick="setStep('
       + o[i] + ')">' + o[i] + '&deg;</button>';
  }
  document.getElementById('steps').innerHTML = h;
}
function setStep(v){ step = v; steps(); }

/* ---------- state ---------- */
function draw(S){
  document.getElementById('dot').className = 'dot live';
  document.getElementById('stat').textContent =
    S.relaxed ? 'RELAXED - PWM band hai' : 'active  -  speed ' + S.speed;

  if (S.seq !== curSeq){
    var old = document.getElementById('sq_' + curSeq);
    if (old) old.className = '';
    var nw = document.getElementById('sq_' + S.seq);
    if (nw) nw.className = 'on';
    curSeq = S.seq;
  }

  if (!cfgOpen) return;            // settings band hai to aage kuch nahi

  if (!built){
    var h = '';
    for (var i = 0; i < S.f.length; i++) h += mk(S.f[i], i);
    document.getElementById('list').innerHTML = h;
    built = true;
  }
  for (var i = 0; i < S.f.length; i++){
    var f = S.f[i];
    document.getElementById('d' + i).innerHTML = f.deg + '&deg;';
    document.getElementById('u' + i).textContent = f.us + ' us';
    var sl = document.getElementById('s' + i);
    if (document.activeElement !== sl) sl.value = f.deg;
    document.getElementById('o' + i).innerHTML = f.opd + '&deg;';
    document.getElementById('c' + i).innerHTML = f.cld + '&deg;';
  }
  curIdle = S.idle;
  document.getElementById('idlebtn').textContent =
    S.idle === 0 ? 'AUTO-RELAX: BAND' : 'AUTO-RELAX: ' + S.idle + 's';
  if (!toastT) document.getElementById('foot').textContent = S.ip;
}

function fail(){
  document.getElementById('dot').className = 'dot';
  document.getElementById('stat').textContent = 'board se baat nahi ho rahi';
}

function poll(){
  if (busy) return;
  fetch('/api/state').then(function(r){ return r.json(); }).then(draw).catch(fail);
}

function send(url){
  busy = true;
  fetch(url).then(function(r){ return r.json(); })
    .then(function(S){ busy = false; draw(S); })
    .catch(function(){ busy = false; fail(); });
}

/* ---------- actions ---------- */
function sign(id){ send('/api/sign?n=' + id); }
function seq(id){  send('/api/seq?n=' + id); }
function stopSeq(){ send('/api/stop'); }
function nudge(i, d){ send('/api/nudge?ch=' + i + '&d=' + d + '&step=' + step); }
function slide(i, v){ send('/api/set?ch=' + i + '&deg=' + v); }
function go(i, v){ send('/api/set?ch=' + i + '&deg=' + v); }
function g(n){ send('/api/g?n=' + n); }

function setOpen(i){  send('/api/setopen?ch=' + i);  toast('OPEN set'); }
function setClose(i){ send('/api/setclose?ch=' + i); toast('CLOSED set'); }
function calSave(){ send('/api/save');     toast('SAVED - reboot par bhi rahega'); }
function calLoad(){ send('/api/load');     toast('flash se wapas padha'); }
function calDef(){  send('/api/defaults'); toast('defaults (abhi save nahi hue)'); }

var toastT = null;
function toast(msg){
  var f = document.getElementById('foot');
  f.textContent = msg;
  f.style.color = '#3fb950';
  if (toastT) clearTimeout(toastT);
  toastT = setTimeout(function(){ f.style.color = ''; toastT = null; }, 2500);
}

var IDLE = [0, 5, 8, 15, 30];
function cycleIdle(){
  var i = IDLE.indexOf(curIdle);
  send('/api/idle?s=' + IDLE[(i + 1) % IDLE.length]);
}

/* ---------- popout ---------- */
function openCfg(){ cfgOpen = true; document.getElementById('mask').className = 'mask show'; poll(); }
function closeCfg(){ cfgOpen = false; document.getElementById('mask').className = 'mask'; }
function maskClick(e){ if (e.target.id === 'mask') closeCfg(); }

fetch('/api/list').then(function(r){ return r.json(); }).then(buildList).catch(fail);
steps();
poll();
setInterval(poll, 700);
</script>
</body>
</html>
)rawliteral";
