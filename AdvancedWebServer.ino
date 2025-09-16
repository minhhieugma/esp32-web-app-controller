/*
  ESP32-C6 Web GPIO + ADC + PWM + Gamepad Controller
  - Serves a single-page app at http://<board-ip>/  (GPIO/ADC/PWM)
  - Serves gamepad UI at http://<board-ip>/game-pad (10 channels)
  - Control digital pins (mode + state), write PWM, read analog, stream gamepad channels
  - Tested with Arduino-ESP32 core 3.x

  UPDATE THESE LISTS for your exact ESP32-C6 board:
    - DIGITAL_PINS: pins you want to control
    - ADC_PINS: pins that support analogRead()
    - CHANNEL_PINS: mapping from CH1..CH10 -> GPIO (set to -1 to disable a channel)

  PWM: Uses LEDC. Resolution = 10 bits (0..1023). Frequency = 1 kHz.
*/

#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>   // Install "ArduinoJson" by Benoit Blanchon (v6+)

// ==== Wi-Fi (you provided these) ====
const char *ssid     = "HieuLe";
const char *password = "Lmhieu13071990";

// ==== CONFIGURE YOUR PINS HERE ====
// Common ESP32-C6 dev boards expose something like these; adjust as needed!
int DIGITAL_PINS[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 18, 19, 20, 21 };
int NUM_DIGITAL = sizeof(DIGITAL_PINS) / sizeof(DIGITAL_PINS[0]);

// ADC-capable pins (example set; VERIFY for your board!)
// On many ESP32-C6 boards, ADC is available on a subset of GPIOs.
int ADC_PINS[] = { 0, 1, 2, 3, 4, 5, 6, 7 };
int NUM_ADC = sizeof(ADC_PINS) / sizeof(ADC_PINS[0]);

// === Gamepad channel -> GPIO mapping (1-based index) ===
// Adjust these to your wiring. Unused channel? set to -1.
int CHANNEL_PINS[11] = {
  -1,  // [0] unused
   2,  // CH1
   3,  // CH2
   4,  // CH3
   5,  // CH4
   6,  // CH5
   7,  // CH6
   8,  // CH7
   9,  // CH8
  18,  // CH9
  19   // CH10
};

// LEDC (PWM) settings
const int LEDC_FREQ       = 1000;    // 1 kHz
const int LEDC_RESOLUTION = 10;      // 10-bit -> duty 0..1023
const int LEDC_CHANNELS   = 8;       // ESP32 family typically supports 8 (low-speed)

// Simple mapping pin -> ledc channel (we’ll reuse channels by modulo)
int ledcChannelForPin(int pin) {
  return (pin % LEDC_CHANNELS);
}

// Extended pin mode for UI reporting
enum PinModeEx { PM_INPUT=0, PM_INPUT_PULLUP=1, PM_OUTPUT=2 };
PinModeEx pinModes[60]; // big enough for typical pin numbers; adjust if needed

// Forward declarations
const char* modeToStr(PinModeEx m);
void applyMode(int pin, PinModeEx m);

// HTTP server
WebServer server(80);

// ---------- HTML UI (served from flash for simplicity) ----------
const char INDEX_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8" />
<meta name="viewport" content="width=device-width,initial-scale=1" />
<title>ESP32-C6 Pin Controller</title>
<style>
  body{font-family:system-ui,-apple-system,Segoe UI,Roboto,Helvetica,Arial,sans-serif;margin:16px}
  h1{font-size:1.25rem;margin:0 0 8px}
  .grid{display:grid;gap:12px}
  .card{border:1px solid #ddd;border-radius:12px;padding:12px}
  .row{display:flex;gap:8px;flex-wrap:wrap;align-items:center}
  label{font-size:.9rem}
  input[type="number"]{width:5rem}
  .muted{color:#666;font-size:.85rem}
  .pill{display:inline-block;padding:2px 8px;border-radius:999px;background:#f2f2f2}
  button{cursor:pointer}
  .ok{color:#0a0}
  .warn{color:#d60}
  .err{color:#c00}
</style>
</head>
<body>
  <h1>ESP32-C6 Pin Controller</h1>
  <div class="muted">Use responsibly. Analog write is PWM (not true DAC).</div>
  <p><b>Board IP:</b> <span id="ip">loading…</span></p>

  <div class="grid">
    <div class="card">
      <h3>Digital Pins</h3>
      <div id="digitalPins" class="grid"></div>
    </div>

    <div class="card">
      <h3>PWM (analogWrite) </h3>
      <div class="row">
        <label>Pin: <input id="pwmPin" type="number" value="2" /></label>
        <label>Duty (0..1023): <input id="pwmDuty" type="number" value="512" min="0" max="1023" /></label>
        <button onclick="setPwm()">Set PWM</button>
      </div>
      <div class="muted">LEDC: 1 kHz, 10-bit resolution (0..1023)</div>
    </div>

    <div class="card">
      <h3>Analog Read (ADC)</h3>
      <div class="row">
        <label>Pin: <input id="adcPin" type="number" value="2" /></label>
        <button onclick="readAdc()">Read</button>
        <span id="adcValue" class="pill">—</span>
      </div>
      <div class="muted">Range 0..4095 (default). Max 3.3V input.</div>
    </div>
  </div>

<script>
async function getJSON(u, opts){ const r = await fetch(u, opts); if(!r.ok) throw new Error(await r.text()); return r.json(); }

async function loadPins(){
  try{
    const data = await getJSON('/api/pins');
    document.getElementById('ip').textContent = data.ip || '(unknown)';
    const container = document.getElementById('digitalPins');
    container.innerHTML = '';
    data.digital.forEach(p => {
      const div = document.createElement('div');
      div.className = 'card';
      div.innerHTML = `
        <div class="row">
          <b>GPIO ${p.pin}</b>
          <span class="pill">${p.mode}</span>
        </div>
        <div class="row" style="margin-top:6px">
          <label>
            Mode:
            <select id="mode-${p.pin}">
              <option value="INPUT" ${p.mode==='INPUT'?'selected':''}>INPUT</option>
              <option value="INPUT_PULLUP" ${p.mode==='INPUT_PULLUP'?'selected':''}>INPUT_PULLUP</option>
              <option value="OUTPUT" ${p.mode==='OUTPUT'?'selected':''}>OUTPUT</option>
            </select>
          </label>
          <button onclick="setMode(${p.pin})">Apply Mode</button>
        </div>
        <div class="row" style="margin-top:6px">
          <span>Status: <b id="state-${p.pin}">${p.state}</b></span>
          <button onclick="refreshState(${p.pin})">Refresh</button>
          <button onclick="writeDigital(${p.pin},1)">HIGH</button>
          <button onclick="writeDigital(${p.pin},0)">LOW</button>
        </div>
        <div class="row" style="margin-top:6px">
          <label>PWM Duty (0..1023): <input id="pwm-${p.pin}" type="number" min="0" max="1023" value="512" /></label>
          <button onclick="writePwm(${p.pin})">Set PWM</button>
        </div>
      `;
      container.appendChild(div);
    });
  }catch(e){
    alert('Failed to load pin list: '+e.message);
  }
}

async function setMode(pin){
  const sel = document.getElementById('mode-'+pin);
  const mode = sel.value;
  await getJSON('/api/mode', { method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({pin, mode})});
  await loadPins(); // reload all pins to update mode label
}

async function refreshState(pin){
  const j = await getJSON(`/api/digital?pin=${pin}`);
  document.getElementById('state-'+pin).textContent = j.state;
}

async function writeDigital(pin, value){
  await getJSON('/api/digital', { method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({pin, value})});
  await refreshState(pin);
}

async function writePwm(pin){
  const duty = parseInt(document.getElementById('pwm-'+pin).value, 10);
  await getJSON('/api/digital', { method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({pin, duty})});
  await refreshState(pin);
}

async function readAdc(){
  const pin = parseInt(document.getElementById('adcPin').value,10);
  const j = await getJSON(`/api/analog/read?pin=${pin}`);
  document.getElementById('adcValue').textContent = j.value;
}

loadPins();
</script>
</body>
</html>
)HTML";

// ---------- Joystick / Gamepad HTML (served from /game-pad) ----------
const char JOYSTICK_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="en">
  <head>
    <meta charset="UTF-8" />
    <meta
      name="viewport"
      content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no, viewport-fit=cover"
    />
    <title>ESP32 Joystick Simulator — iPhone Fit</title>

    <!-- iOS fullscreen (when added to Home Screen) -->
    <meta name="apple-mobile-web-app-capable" content="yes" />
    <meta name="apple-mobile-web-app-status-bar-style" content="black-translucent" />
    <meta name="apple-mobile-web-app-title" content="ESP32 Gamepad" />
    <meta name="theme-color" content="#1976d2" />

    <style>
      :root {
        --vh: 1vh;
        --gap: 2.5vw;
        --accent: #1976d2;
        --soft: #e3f2fd;
        --soft2: #bbdefb;
      }
      * { box-sizing: border-box; -webkit-tap-highlight-color: transparent; }
      html, body { margin: 0; padding: 0; height: calc(var(--vh)*100); overflow: hidden; }
      body {
        font-family: "Segoe UI", system-ui, -apple-system, Arial, sans-serif;
        background: linear-gradient(135deg,#e3f0ff 0%,#f8f9fa 100%);
        display: flex; flex-direction: column; align-items: center;
        padding-bottom: env(safe-area-inset-bottom);
        -webkit-user-select: none; user-select: none;
      }

      /* Title */
      .page-title {
        margin: 0; padding: 1.2vh 0 .6vh;
        font-size: clamp(14px,2.6vh,22px);
        font-weight: 700; color: var(--accent);
        text-shadow: 0 2px 8px #b3c6e0; text-align: center;
      }

      /* Layout shell */
      .shell {
        display: flex; flex-direction: column;
        width: 100vw; height: calc(var(--vh)*100);
        overflow: hidden; padding: 1vh 3vw;
      }
      .content {
        display: grid; gap: var(--gap);
        grid-template:
          "sticks" auto
          "sliders" 1fr / 1fr;
        place-items: center; flex: 1; min-height: 0; width: 100%;
        padding-bottom: 2.5vh; /* keep bottom sliders visible */
      }

      /* Sticks w/ center CH9/CH10 */
      .sticks {
        grid-area: sticks; width: 100%;
        display: grid; gap: var(--gap);
        grid-template-columns: 1fr auto 1fr;
        align-items: center; justify-items: center;
      }
      .stick-card { display: grid; justify-items: center; gap: .6vh; }
      .joystick-box {
        width: min(36vw,200px); /* compact to avoid clipping sliders */
        aspect-ratio: 1/1; position: relative;
        border-radius: 14px; background: linear-gradient(135deg,#f0f4ff 0%,#e0e7ef 100%);
        box-shadow: 0 6px 20px #b3c6e0; border: 2px solid #90caf9;
        display: grid; place-items: center; overflow: hidden;
        touch-action: none; /* CRITICAL: prevent Safari gestures */
      }
      .stick-base {
        position: absolute; inset: 10%; border-radius: 50%;
        background: radial-gradient(circle,#bbdefb 60%,#e3f2fd 100%);
        border: 2px solid #64b5f6; box-shadow: inset 0 2px 12px rgba(0,0,0,.06);
      }
      /* Transform-only updates via CSS vars */
      .stick-knob {
        position: absolute; left: 50%; top: 50%;
        transform: translate3d(calc(-50% + var(--dx, 0px)), calc(-50% + var(--dy, 0px)), 0);
        will-change: transform;
        width: 16%; aspect-ratio: 1/1; border-radius: 50%;
        background: linear-gradient(135deg,#1976d2 0%,#64b5f6 100%);
        border: 2px solid #fff; box-shadow: 0 4px 16px #90caf9;
        touch-action: none; /* also block on the knob itself */
      }
      .stick-label { font-size: clamp(12px,1.6vh,16px); color: #333; font-weight: 600; }

      /* Center CH9/CH10 (side-by-side) */
      .center-controls {
        display: grid; gap: 8px; justify-items: center; align-items: center;
        background: #f5faff; border: 1px solid var(--soft2); border-radius: 12px;
        box-shadow: 0 2px 8px var(--soft); padding: .6vh 1vw;
      }
      .center-title { font-size: clamp(11px,1.5vh,14px); color: var(--accent); font-weight: 700; }
      .center-row { display: grid; grid-template-columns: auto auto; gap: 10px; align-items: center; }
      .center-slider { display: grid; gap: 4px; justify-items: center; width: 72px; }
      .center-slider label { font-size: clamp(10px,1.4vh,13px); color: #235; font-weight: 600; }
      .center-slider input[type="range"] { width: 100%; height: 14px; border-radius: 7px; background: linear-gradient(90deg,#90caf9 0%,var(--accent) 100%); }
      .center-slider input[type="range"]::-webkit-slider-thumb {
        width: 22px; height: 22px; border-radius: 50%;
        background: var(--accent); border: 2px solid #fff; box-shadow: 0 2px 6px #90caf9;
      }

      /* Sliders CH5–CH8 */
      .sliders {
        grid-area: sliders; width: 100%;
        display: grid; gap: var(--gap); grid-template-columns: 1fr 1fr;
      }
      .slider-card {
        display: grid; justify-items: center; gap: .6vh; padding: .8vh 2vw;
        background: #f5faff; border-radius: 10px; border: 1px solid var(--soft2);
        box-shadow: 0 2px 8px var(--soft);
      }
      .slider-card label { font-size: clamp(12px,1.6vh,15px); color: var(--accent); font-weight: 600; }
      .slider-card input[type="range"] {
        width: 86%; height: 14px; border-radius: 7px;
        background: linear-gradient(90deg,#90caf9 0%,var(--accent) 100%);
      }
      .slider-card input[type="range"]::-webkit-slider-thumb {
        width: 26px; height: 26px; border-radius: 50%;
        background: var(--accent); border: 2px solid #fff; box-shadow: 0 2px 8px #90caf9;
      }

      /* Landscape tighten */
      @media (orientation:landscape) and (max-height:480px){
        .joystick-box { width: min(32vw,180px); }
        .center-slider { width: 64px; }
        .sliders { grid-template-columns: repeat(4,1fr); }
      }
    </style>
  </head>
  <body>
    <div class="shell">
      <h2 class="page-title">Gamepad / RC Transmitter</h2>

      <div class="content">
        <!-- Sticks + Center CH9/CH10 -->
        <div class="sticks">
          <div class="stick-card">
            <div class="joystick-box" id="joy1">
              <div class="stick-base" id="joy1-base"></div>
              <div class="stick-knob" id="joy1-knob"></div>
            </div>
            <div class="stick-label">Joystick 1 (CH1:X, CH2:Y)</div>
          </div>

          <div class="center-controls">
            <div class="center-title">Aux Channels</div>
            <div class="center-row">
              <div class="center-slider">
                <label>CH9</label>
                <input type="range" min="0" max="100" value="0" id="slider5" />
              </div>
              <div class="center-slider">
                <label>CH10</label>
                <input type="range" min="0" max="100" value="0" id="slider6" />
              </div>
            </div>
          </div>

          <div class="stick-card">
            <div class="joystick-box" id="joy2">
              <div class="stick-base" id="joy2-base"></div>
              <div class="stick-knob" id="joy2-knob"></div>
            </div>
            <div class="stick-label">Joystick 2 (CH3:X, CH4:Y)</div>
          </div>
        </div>

        <!-- Sliders CH5–CH8 -->
        <div class="sliders">
          <div class="slider-card">
            <label>Slider 1 (CH5)</label>
            <input type="range" min="0" max="100" value="50" id="slider1" />
          </div>
          <div class="slider-card">
            <label>Slider 2 (CH6)</label>
            <input type="range" min="0" max="100" value="50" id="slider2" />
          </div>
          <div class="slider-card">
            <label>Slider 3 (CH7)</label>
            <input type="range" min="0" max="100" value="50" id="slider3" />
          </div>
          <div class="slider-card">
            <label>Slider 4 (CH8)</label>
            <input type="range" min="0" max="100" value="50" id="slider4" />
          </div>
        </div>
      </div>
    </div>

    <script>
      /* ---------- Logging helpers (non-invasive) ---------- */
      function logEvent(name, data = {}){ try { console.log(`[EVT] ${name} ${JSON.stringify(data)}`); } catch { console.log(`[EVT] ${name}`); } }
      function frameCSV(){ return Array.from({length:10},(_,i)=> (+window.gamepadChannels[i+1]||0).toFixed(2)).join(","); }
      (function(){ try { console.clear = function(){}; } catch(_){} })();

      /* ---------- Viewport fix for iOS Safari chrome ---------- */
      function setVH(){ const vh = window.innerHeight * 0.01; document.documentElement.style.setProperty('--vh',`${vh}px`); logEvent('viewport_set',{vh}); }
      setVH(); window.addEventListener('resize', setVH);

      /* ---------- Channel state + logging ---------- */
      window.gamepadChannels = {1:0,2:0,3:0,4:0,5:0.5,6:0.5,7:0.5,8:0.5,9:0,10:0};
      logEvent('channels_init',{csv: frameCSV()});
      function logChannels(){
        console.clear();
        console.log("CHANNELS:");
        for(let i=1;i<=10;i++){ console.log(`CH${i}: ${(+window.gamepadChannels[i]||0).toFixed(2)}`); }
        console.log("CSV:", frameCSV());
      }

      /* ---------- Joystick logic (robust pointer capture) ---------- */
      function clamp(dx,dy,r){ const d = Math.hypot(dx,dy); if(d<=r || d===0) return {dx,dy}; const s=r/d; return {dx:dx*s,dy:dy*s}; }
      function setupJoystick(knobId, baseId, chX, chY){
        const knob = document.getElementById(knobId);
        const base = document.getElementById(baseId);
        const box  = base.parentElement;

        let cx=0, cy=0, radius=0;
        let activeId = null;

        function measure(){ const rect = box.getBoundingClientRect(); cx = rect.left + rect.width/2; cy = rect.top  + rect.height/2; radius = (rect.width/2) * 0.9; }
        function updateFromClientXY(x, y){
          const {dx, dy} = clamp(x - cx, y - cy, radius);
          knob.style.setProperty('--dx', `${dx}px`);
          knob.style.setProperty('--dy', `${dy}px`);
          window.gamepadChannels[chX] = dx / radius;
          window.gamepadChannels[chY] = dy / radius;
          logChannels();
          logEvent('joystick_move',{ joy: knobId, chX, chY, x: +window.gamepadChannels[chX]||0, y: +window.gamepadChannels[chY]||0, csv: frameCSV() });
        }
        function recenter(){
          knob.style.setProperty('--dx', `0px`);
          knob.style.setProperty('--dy', `0px`);
          window.gamepadChannels[chX] = 0;
          window.gamepadChannels[chY] = 0;
          logChannels();
          logEvent('joystick_recenter',{joy: knobId, chX, chY, csv: frameCSV()});
        }

        function onPD(e){ if (activeId !== null) return; activeId = e.pointerId; box.setPointerCapture?.(activeId); measure(); updateFromClientXY(e.clientX, e.clientY); logEvent('joystick_down',{joy: knobId, chX, chY, pointerId: activeId}); e.preventDefault(); }
        function onPM(e){ if (e.pointerId !== activeId) return; updateFromClientXY(e.clientX, e.clientY); e.preventDefault(); }
        function onPU(e){ if (e.pointerId !== activeId) return; activeId = null; recenter(); logEvent('joystick_up',{joy: knobId, chX, chY}); e.preventDefault(); }

        if ('onpointerdown' in window){
          knob.addEventListener('pointerdown', onPD, {passive:false});
          box .addEventListener('pointerdown', onPD, {passive:false});
          box .addEventListener('pointermove', onPM, {passive:false});
          box .addEventListener('pointerup',   onPU, {passive:false});
          box .addEventListener('pointercancel', onPU, {passive:false});
        } else {
          function ts(e){ const t=e.touches[0]; measure(); updateFromClientXY(t.clientX,t.clientY); logEvent('joystick_touchstart',{joy: knobId}); e.preventDefault(); }
          function tm(e){ const t=e.touches[0]; updateFromClientXY(t.clientX,t.clientY); e.preventDefault(); }
          function te(){ recenter(); logEvent('joystick_touchend',{joy: knobId}); }
          box.addEventListener('touchstart', ts, {passive:false});
          box.addEventListener('touchmove',  tm, {passive:false});
          box.addEventListener('touchend',   te, {passive:false});
          box.addEventListener('touchcancel',te, {passive:false});
          box.addEventListener('mousedown', (e)=>{ measure(); updateFromClientXY(e.clientX,e.clientY); logEvent('joystick_mousedown',{joy: knobId}); });
          window.addEventListener('mousemove', (e)=> updateFromClientXY(e.clientX,e.clientY));
          window.addEventListener('mouseup', ()=>{ recenter(); logEvent('joystick_mouseup',{joy: knobId}); });
        }

        window.addEventListener('resize', ()=>{ measure(); recenter(); logEvent('window_resize',{joy: knobId}); });
        window.addEventListener('orientationchange', ()=>{ setTimeout(()=>{ measure(); recenter(); logEvent('orientation_change',{joy: knobId}); }, 300); });
        if (document.readyState === 'complete' || document.readyState === 'interactive'){ measure(); recenter(); logEvent('joystick_ready',{joy: knobId, chX, chY}); }
        else { window.addEventListener('DOMContentLoaded', ()=>{ measure(); recenter(); logEvent('joystick_ready',{joy: knobId, chX, chY}); }); }
      }

      setupJoystick('joy1-knob','joy1-base',1,2);
      setupJoystick('joy2-knob','joy2-base',3,4);

      /* ---------- Sliders CH5–CH10 (CH9=slider5, CH10=slider6) ---------- */
      for (let i=1; i<=6; i++){
        const el = document.getElementById('slider'+i);
        if (!el) continue;
        el.addEventListener('input', e=>{
          window.gamepadChannels[4+i] = e.target.value/100;
          logChannels();
          logEvent('slider_change',{ slider: 'slider'+i, channel: 4+i, value: +e.target.value/100, csv: frameCSV() });
        });
        logEvent('slider_ready',{slider: 'slider'+i, channel: 4+i, value: el ? +el.value/100 : null});
      }

      /* ---------- TRANSMIT LOOP: send 30 fps to /api/gamepad ---------- */
      const SEND_HZ = 30;
      let _lastSent = 0;
      async function sendFrame(){
        try {
          const now = performance.now();
          if (now - _lastSent < (1000 / SEND_HZ)) return;
          _lastSent = now;

          const ch = [];
          for (let i=1; i<=10; i++){
            let v = +window.gamepadChannels[i] || 0;
            // CH1..CH4 are -1..+1; CH5..CH10 are 0..1
            ch.push(v);
          }
          await fetch('/api/gamepad', { method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({ ch }) });
        } catch (e) { /* keep UI responsive if network hiccups */ }
      }
      setInterval(sendFrame, 1000 / SEND_HZ);
    </script>
  </body>
</html>
)HTML";

// ---------- Helpers ----------
bool isInList(int pin, int* list, int n) {
  for (int i=0;i<n;i++) if (list[i] == pin) return true;
  return false;
}

const char* modeToStr(PinModeEx m){
  switch(m){
    case PM_INPUT: return "INPUT";
    case PM_INPUT_PULLUP: return "INPUT_PULLUP";
    case PM_OUTPUT: return "OUTPUT";
  }
  return "UNKNOWN";
}

void applyMode(int pin, PinModeEx m){
  if(m == PM_INPUT) pinMode(pin, INPUT);
  else if(m == PM_INPUT_PULLUP) pinMode(pin, INPUT_PULLUP);
  else pinMode(pin, OUTPUT);
  pinModes[pin] = m;
}

// Normalize helpers for gamepad
static inline float clamp01(float v){ if(v<0) return 0; if(v>1) return 1; return v; }
static inline uint16_t toDuty10(float v){ v = clamp01(v); return (uint16_t)lroundf(v * ((1<<LEDC_RESOLUTION)-1)); }

// Write one channel (CH = 1..10), value in [-1..1] for sticks, [0..1] for sliders
void writeChannelPWM(int ch, float val, bool stickStyle){
  if (ch < 1 || ch > 10) return;
  int pin = CHANNEL_PINS[ch];
  if (pin < 0) return; // unmapped
  float norm = stickStyle ? (val + 1.0f) * 0.5f : val;  // map -1..1 -> 0..1
  uint16_t duty = toDuty10(norm);
  applyMode(pin, PM_OUTPUT);
  // Attach and write (API is idempotent in Arduino-ESP32 3.x)
  ledcAttach(pin, LEDC_FREQ, LEDC_RESOLUTION);
  ledcWrite(pin, duty);
}

// ---------- HTTP Handlers ----------
void handleRoot(){
  server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
}

void handleGamePad(){
  server.send_P(200, "text/html; charset=utf-8", JOYSTICK_HTML);
}

void handlePins(){
  // Build a JSON describing all configured digital pins
  DynamicJsonDocument doc(1024 + NUM_DIGITAL*64);
  doc["ip"] = WiFi.localIP().toString();

  JsonArray arr = doc.createNestedArray("digital");
  for(int i=0;i<NUM_DIGITAL;i++){
    int pin = DIGITAL_PINS[i];
    JsonObject o = arr.createNestedObject();
    o["pin"] = pin;
    o["mode"] = modeToStr(pinModes[pin]);
    int state = -1;
    if(pinModes[pin] == PM_INPUT || pinModes[pin] == PM_INPUT_PULLUP){
      state = digitalRead(pin);
    } else if (pinModes[pin] == PM_OUTPUT){
      state = digitalRead(pin);
    }
    o["state"] = state;
  }

  // Also expose channel -> pin mapping (useful for UI/debug)
  JsonArray ch = doc.createNestedArray("channels");
  for (int i=1; i<=10; i++){
    JsonObject o = ch.createNestedObject();
    o["ch"] = i;
    o["pin"] = CHANNEL_PINS[i];
  }

  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

void handleGetDigital(){
  if(!server.hasArg("pin")) { server.send(400, "application/json", "{\"error\":\"pin required\"}"); return; }
  int pin = server.arg("pin").toInt();
  if(!isInList(pin, DIGITAL_PINS, NUM_DIGITAL)){
    server.send(400, "application/json", "{\"error\":\"pin not in DIGITAL_PINS list\"}");
    return;
  }
  int state = digitalRead(pin);
  DynamicJsonDocument doc(128);
  doc["pin"]=pin; doc["state"]=state;
  String out; serializeJson(doc,out);
  server.send(200, "application/json", out);
}

void handleSetMode(){
  if(server.method() != HTTP_POST){ server.send(405, "application/json", "{\"error\":\"POST only\"}"); return; }
  DynamicJsonDocument body(256);
  DeserializationError err = deserializeJson(body, server.arg("plain"));
  if(err){ server.send(400, "application/json", "{\"error\":\"bad json\"}"); return; }
  int pin = body["pin"] | -1;
  const char* mode = body["mode"] | "";
  if(!isInList(pin, DIGITAL_PINS, NUM_DIGITAL)){
    server.send(400, "application/json", "{\"error\":\"pin not in DIGITAL_PINS list\"}");
    return;
  }
  PinModeEx m = PM_INPUT;
  if(strcmp(mode,"INPUT")==0) m = PM_INPUT;
  else if(strcmp(mode,"INPUT_PULLUP")==0) m = PM_INPUT_PULLUP;
  else if(strcmp(mode,"OUTPUT")==0) m = PM_OUTPUT;
  else { server.send(400, "application/json", "{\"error\":\"mode must be INPUT, INPUT_PULLUP, or OUTPUT\"}"); return; }

  applyMode(pin, m);

  DynamicJsonDocument doc(128);
  doc["ok"]=true; doc["pin"]=pin; doc["mode"]=modeToStr(m);
  String out; serializeJson(doc,out);
  server.send(200, "application/json", out);
}

void handleSetDigital(){
  if(server.method() != HTTP_POST){ server.send(405, "application/json", "{\"error\":\"POST only\"}"); return; }
  DynamicJsonDocument body(256);
  DeserializationError err = deserializeJson(body, server.arg("plain"));
  if(err){ server.send(400, "application/json", "{\"error\":\"bad json\"}"); return; }
  int pin = body["pin"] | -1;
  int value = body["value"] | -1;
  int duty = body["duty"] | -1;
  if(!isInList(pin, DIGITAL_PINS, NUM_DIGITAL)){
    server.send(400, "application/json", "{\"error\":\"pin not in DIGITAL_PINS list\"}");
    return;
  }
  if(duty >= 0) {
    // PWM requested
    applyMode(pin, PM_OUTPUT);
    ledcAttach(pin, LEDC_FREQ, LEDC_RESOLUTION);
    ledcWrite(pin, duty);
    DynamicJsonDocument doc(128);
    doc["ok"]=true; doc["pin"]=pin; doc["duty"]=duty;
    String out; serializeJson(doc,out);
    server.send(200, "application/json", out);
    return;
  }
  if(value >= 0) {
    if(pinModes[pin] != PM_OUTPUT){
      // auto-switch to OUTPUT to make it convenient
      applyMode(pin, PM_OUTPUT);
    }
    digitalWrite(pin, value ? HIGH : LOW);
    DynamicJsonDocument doc(128);
    doc["ok"]=true; doc["pin"]=pin; doc["value"]=value?1:0;
    String out; serializeJson(doc,out);
    server.send(200, "application/json", out);
    return;
  }
  server.send(400, "application/json", "{\"error\":\"value or duty required\"}");
}

void handleAnalogRead(){
  if(!server.hasArg("pin")) { server.send(400, "application/json", "{\"error\":\"pin required\"}"); return; }
  int pin = server.arg("pin").toInt();
  if(!isInList(pin, ADC_PINS, NUM_ADC)){
    server.send(400, "application/json", "{\"error\":\"pin not in ADC_PINS list\"}");
    return;
  }
  int val = analogRead(pin);
  DynamicJsonDocument doc(128);
  doc["pin"]=pin; doc["value"]=val;
  String out; serializeJson(doc,out);
  server.send(200, "application/json", out);
}

void handleAnalogWrite(){
  if(server.method() != HTTP_POST){ server.send(405, "application/json", "{\"error\":\"POST only\"}"); return; }
  DynamicJsonDocument body(256);
  DeserializationError err = deserializeJson(body, server.arg("plain"));
  if(err){ server.send(400, "application/json", "{\"error\":\"bad json\"}"); return; }
  int pin = body["pin"] | -1;
  int duty = body["duty"] | -1;
  if(!isInList(pin, DIGITAL_PINS, NUM_DIGITAL)){
    server.send(400, "application/json", "{\"error\":\"pin not in DIGITAL_PINS list\"}");
    return;
  }
  if(duty < 0) duty = 0;
  if(duty > ((1<<LEDC_RESOLUTION)-1)) duty = (1<<LEDC_RESOLUTION)-1;

  // Attach and set up (idempotent)
  ledcAttach(pin, LEDC_FREQ, LEDC_RESOLUTION);
  ledcWrite(pin, duty);
  // Mark as OUTPUT in our table
  pinModes[pin] = PM_OUTPUT;

  DynamicJsonDocument doc(128);
  doc["ok"]=true; doc["pin"]=pin; doc["duty"]=duty;
  String out; serializeJson(doc,out);
  server.send(200, "application/json", out);
}

// === NEW: /api/gamepad — accept 10 channels and drive PWM ===
//
// Body can be:
//   { "ch": [v1..v10] }  // floats, CH1..CH4 in -1..+1; CH5..CH10 in 0..1
// or individual keys "ch1".."ch10"
void handleGamepadFrame(){
  if (server.method() != HTTP_POST){
    server.send(405, "application/json", "{\"error\":\"POST only\"}");
    return;
  }
  DynamicJsonDocument body(768);
  DeserializationError err = deserializeJson(body, server.arg("plain"));
  if (err){
    server.send(400, "application/json", "{\"error\":\"bad json\"}");
    return;
  }

  bool gotArray = body.containsKey("ch") && body["ch"].is<JsonArray>();
  float vals[11];  // 1..10 used
  for (int i=1; i<=10; i++){
    float v = 0.0f;
    if (gotArray){
      JsonArray a = body["ch"].as<JsonArray>();
      if ((int)a.size() >= i) v = a[i-1].as<float>();
    } else {
      char key[6]; // "ch10\0"
      snprintf(key, sizeof(key), "ch%d", i);
      if (body.containsKey(key)) v = body[key].as<float>();
    }
    vals[i] = v;
  }

  // Write all channels (CH1..CH4 sticks: -1..1; others sliders: 0..1)
  for (int i=1; i<=10; i++){
    bool stick = (i <= 4);
    writeChannelPWM(i, vals[i], stick);
  }

  DynamicJsonDocument resp(256);
  resp["ok"] = true;
  String out; serializeJson(resp, out);
  server.send(200, "application/json", out);
}

// ---------- Setup ----------
void setupPinBookkeeping(){
  // default every listed digital pin to INPUT (safe)
  for(int i=0;i<NUM_DIGITAL;i++){
    int pin = DIGITAL_PINS[i];
    applyMode(pin, PM_INPUT);
  }
  // ensure mapped CHANNEL_PINS are at least known to bookkeeping
  for (int ch=1; ch<=10; ch++){
    int pin = CHANNEL_PINS[ch];
    if (pin >= 0){
      // If not in DIGITAL_PINS, set mode table entry to INPUT
      if (!isInList(pin, DIGITAL_PINS, NUM_DIGITAL)){
        // safe default
        applyMode(pin, PM_INPUT);
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);

  setupPinBookkeeping();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.printf("Connecting to %s", ssid);
  while (WiFi.status() != WL_CONNECTED) {
    delay(400);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected, IP address: ");
  Serial.println(WiFi.localIP());

  // Routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/game-pad", HTTP_GET, handleGamePad);
  server.on("/api/pins", HTTP_GET, handlePins);
  server.on("/api/digital", HTTP_GET, handleGetDigital);
  server.on("/api/digital", HTTP_POST, handleSetDigital);
  server.on("/api/mode", HTTP_POST, handleSetMode);
  server.on("/api/analog/read", HTTP_GET, handleAnalogRead);
  server.on("/api/analog/write", HTTP_POST, handleAnalogWrite);
  server.on("/api/gamepad", HTTP_POST, handleGamepadFrame);

  // Basic CORS for convenience (optional)
  server.enableCORS(true);

  server.begin();
  Serial.println("HTTP server started.");
}

void loop() {
  server.handleClient();
}
