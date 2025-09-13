/*
  ESP32-C6 Web GPIO + ADC + PWM Controller
  - Serves a single-page app at http://<board-ip>/
  - Control digital pins (mode + state), write PWM, read analog
  - Tested with Arduino-ESP32 core 3.x

  UPDATE THESE LISTS for your exact ESP32-C6 board:
    - DIGITAL_PINS: pins you want to control
    - ADC_PINS: pins that support analogRead()

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

// LEDC (PWM) settings
const int LEDC_FREQ       = 1000;    // 1 kHz
const int LEDC_RESOLUTION = 10;      // 10-bit -> duty 0..1023
const int LEDC_CHANNELS   = 8;       // ESP32 family typically supports 8 (low-speed)

// Simple mapping pin -> ledc channel (we’ll reuse channels by modulo)
int ledcChannelForPin(int pin) {
  return (pin % LEDC_CHANNELS);
}

// Track pin modes to report to UI

// Extended pin mode for UI reporting
enum PinModeEx { PM_INPUT=0, PM_INPUT_PULLUP=1, PM_OUTPUT=2 };
PinModeEx pinModes[60]; // big enough for typical pin numbers; adjust if needed
// Forward declarations to avoid redeclaration/scope issues
const char* modeToStr(PinModeEx m);
void applyMode(int pin, PinModeEx m);

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

// ---------- HTTP Handlers ----------
void handleRoot(){
  server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
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

  // Conveniently mark it as OUTPUT in our table

  pinModes[pin] = PM_OUTPUT;

  DynamicJsonDocument doc(128);
  doc["ok"]=true; doc["pin"]=pin; doc["duty"]=duty;
  String out; serializeJson(doc,out);
  server.send(200, "application/json", out);
}

void setupPinBookkeeping(){
  // default every listed digital pin to INPUT (safe)
  for(int i=0;i<NUM_DIGITAL;i++){
    int pin = DIGITAL_PINS[i];
    applyMode(pin, PM_INPUT);
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
  server.on("/api/pins", HTTP_GET, handlePins);
  server.on("/api/digital", HTTP_GET, handleGetDigital);
  server.on("/api/digital", HTTP_POST, handleSetDigital);
  server.on("/api/mode", HTTP_POST, handleSetMode);
  server.on("/api/analog/read", HTTP_GET, handleAnalogRead);
  server.on("/api/analog/write", HTTP_POST, handleAnalogWrite);

  // Basic CORS for convenience (optional)
  server.enableCORS(true);

  server.begin();
  Serial.println("HTTP server started.");
}

void loop() {
  server.handleClient();
}
