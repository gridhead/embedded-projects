#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include "secret.h"

#define TEA5767_ADDR 0x60
#define FREQ_MIN 87.5f
#define FREQ_MAX 108.0f

float currentFreq = 98.3f;
bool muted = false;
WebServer server(80);

struct RadioStatus
{
    bool ready;
    bool bandLimit;
    bool stereo;
    byte rssi;
    float freq;
};

void writeRegisters(float freq, bool mute, bool seek, bool seekUp)
{
    uint16_t pll = (4 * (freq * 1000000UL + 225000UL)) / 32768UL;

    byte data[5];
    data[0] = (pll >> 8) & 0x3F;
    data[1] = pll & 0xFF;
    data[2] = 0xB0;
    data[3] = 0x10;
    data[4] = 0x00;

    if (mute)
        data[0] |= 0x80;
    if (seek)
        data[0] |= 0x40;
    if (!seekUp)
        data[2] &= ~0x80;

    // Mid search stop level for seek
    if (seek)
        data[2] = (data[2] & ~0x60) | 0x40;

    Wire.beginTransmission(TEA5767_ADDR);
    Wire.write(data, 5);
    Wire.endTransmission();

    delay(100);
}

RadioStatus readStatus()
{
    byte data[5];
    Wire.requestFrom(TEA5767_ADDR, 5);
    for (int i = 0; i < 5; i++)
        data[i] = Wire.read();

    RadioStatus s;
    s.ready = data[0] & 0x80;
    s.bandLimit = data[0] & 0x40;
    s.stereo = data[2] & 0x80;
    s.rssi = data[3] >> 4;

    uint16_t pll = ((data[0] & 0x3F) << 8) | data[1];
    s.freq = (pll * 32768.0f / 4.0f - 225000.0f) / 1000000.0f;

    return s;
}

void tuneFrequency(float freq)
{
    if (freq < FREQ_MIN) freq = FREQ_MIN;
    if (freq > FREQ_MAX) freq = FREQ_MAX;
    currentFreq = freq;
    writeRegisters(currentFreq, muted, false, false);
}

void doSeek(bool up)
{
    writeRegisters(currentFreq, muted, true, up);
    delay(200);

    for (int i = 0; i < 50; i++)
    {
        RadioStatus s = readStatus();
        if (s.ready)
        {
            if (!s.bandLimit)
                currentFreq = s.freq;
            writeRegisters(currentFreq, muted, false, false);
            return;
        }
        delay(50);
    }

    writeRegisters(currentFreq, muted, false, false);
}

const char PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>TEA5767 Radio</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:system-ui,-apple-system,sans-serif;background:#1a1a2e;color:#e0e0e0;
  display:flex;justify-content:center;align-items:center;min-height:100vh}
.card{background:#16213e;border-radius:16px;padding:32px;width:360px;
  box-shadow:0 8px 32px rgba(0,0,0,0.4)}
h1{text-align:center;font-size:1.3em;margin-bottom:24px;color:#a8d8ea}
.freq-display{text-align:center;font-size:2.8em;font-weight:700;color:#fff;
  margin:8px 0 4px;font-variant-numeric:tabular-nums}
.freq-display span{font-size:0.4em;color:#888;margin-left:4px}
.slider-row{display:flex;align-items:center;gap:8px;margin:8px 0 20px}
.slider-row label{font-size:0.75em;color:#888;min-width:30px}
input[type=range]{flex:1;-webkit-appearance:none;background:#0f3460;height:6px;
  border-radius:3px;outline:none}
input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:20px;height:20px;
  background:#e94560;border-radius:50%;cursor:pointer}
.rssi{margin:16px 0}
.rssi-label{font-size:0.8em;color:#888;margin-bottom:6px;display:flex;
  justify-content:space-between}
.rssi-bar{display:flex;gap:3px;height:24px}
.rssi-bar .seg{flex:1;border-radius:3px;background:#0f3460;transition:background 0.15s}
.rssi-bar .seg.on{background:#53d769}
.rssi-bar .seg.on.mid{background:#f5a623}
.rssi-bar .seg.on.high{background:#e94560}
.indicators{display:flex;gap:16px;margin:16px 0;justify-content:center}
.ind{font-size:0.85em;padding:6px 14px;border-radius:20px;background:#0f3460}
.ind.active{background:#53d769;color:#1a1a2e;font-weight:600}
.controls{display:flex;gap:10px;margin-top:20px}
.controls button{flex:1;padding:12px;border:none;border-radius:10px;font-size:1.1em;
  cursor:pointer;background:#0f3460;color:#e0e0e0;transition:background 0.15s}
.controls button:active{background:#1a4a8a}
.controls button.mute-on{background:#e94560;color:#fff}
</style>
</head>
<body>
<div class="card">
  <h1>TEA5767 Radio</h1>
  <div class="freq-display"><span id="freq">98.3</span><span>MHz</span></div>
  <div class="slider-row">
    <label>87.5</label>
    <input type="range" id="slider" min="87.5" max="108.0" step="0.1" value="98.3">
    <label>108</label>
  </div>
  <div class="rssi">
    <div class="rssi-label"><span>Signal</span><span id="rssi-val">0/15</span></div>
    <div class="rssi-bar" id="rssi-bar"></div>
  </div>
  <div class="indicators">
    <div class="ind" id="stereo-ind">Stereo</div>
    <div class="ind" id="ready-ind">Ready</div>
  </div>
  <div class="mem" style="display:flex;gap:16px;justify-content:center;font-size:0.75em;color:#888;margin:12px 0 4px">
    <span>Heap: <span id="heap">--</span></span>
    <span>PSRAM: <span id="psram">--</span></span>
  </div>
  <div class="controls">
    <button onclick="seek('down')">&#9198; Seek</button>
    <button id="mute-btn" onclick="toggleMute()">Mute</button>
    <button onclick="seek('up')">Seek &#9197;</button>
  </div>
</div>
<script>
const slider=document.getElementById('slider');
const freqEl=document.getElementById('freq');
const rssiBar=document.getElementById('rssi-bar');
const rssiVal=document.getElementById('rssi-val');
const stereoInd=document.getElementById('stereo-ind');
const readyInd=document.getElementById('ready-ind');
const muteBtn=document.getElementById('mute-btn');
let muted=false,tuneTimer=null;

for(let i=0;i<15;i++){const s=document.createElement('div');s.className='seg';rssiBar.appendChild(s);}

slider.addEventListener('input',()=>{
  freqEl.textContent=parseFloat(slider.value).toFixed(1);
  clearTimeout(tuneTimer);
  tuneTimer=setTimeout(()=>{
    fetch('/api/tune?freq='+slider.value,{method:'POST'});
  },120);
});

function seek(dir){fetch('/api/seek?dir='+dir,{method:'POST'});}

function toggleMute(){
  muted=!muted;
  fetch('/api/mute?mute='+(muted?1:0),{method:'POST'});
  muteBtn.classList.toggle('mute-on',muted);
}

function updateStatus(){
  fetch('/api/status').then(r=>r.json()).then(d=>{
    freqEl.textContent=d.freq.toFixed(1);
    slider.value=d.freq;
    rssiVal.textContent=d.rssi+'/15';
    const segs=rssiBar.children;
    for(let i=0;i<15;i++){
      const on=i<d.rssi;
      segs[i].className='seg'+(on?' on':'')+(on&&i>=10?' high':on&&i>=7?' mid':'');
    }
    stereoInd.classList.toggle('active',d.stereo);
    readyInd.classList.toggle('active',d.ready);
    muted=d.mute;
    muteBtn.classList.toggle('mute-on',muted);
    document.getElementById('heap').textContent=(d.freeHeap/1024).toFixed(1)+'KB';
    document.getElementById('psram').textContent=(d.freePsram/1024/1024).toFixed(1)+'MB';
  }).catch(()=>{});
}

setInterval(updateStatus,250);
updateStatus();
</script>
</body>
</html>
)rawliteral";

void handleRoot()
{
    server.send(200, "text/html", PAGE);
}

void handleStatus()
{
    RadioStatus s = readStatus();
    String json = "{\"freq\":" + String(currentFreq, 1)
        + ",\"rssi\":" + String(s.rssi)
        + ",\"stereo\":" + (s.stereo ? "true" : "false")
        + ",\"ready\":" + (s.ready ? "true" : "false")
        + ",\"bandLimit\":" + (s.bandLimit ? "true" : "false")
        + ",\"mute\":" + (muted ? "true" : "false")
        + ",\"freeHeap\":" + String(ESP.getFreeHeap())
        + ",\"freePsram\":" + String(ESP.getFreePsram())
        + "}";
    server.send(200, "application/json", json);
}

void handleTune()
{
    if (server.hasArg("freq"))
    {
        float f = server.arg("freq").toFloat();
        tuneFrequency(f);
    }
    server.send(200, "application/json", "{\"ok\":true}");
}

void handleSeek()
{
    if (server.hasArg("dir"))
    {
        bool up = server.arg("dir") == "up";
        doSeek(up);
    }
    server.send(200, "application/json", "{\"ok\":true}");
}

void handleMute()
{
    if (server.hasArg("mute"))
    {
        muted = server.arg("mute") == "1";
        writeRegisters(currentFreq, muted, false, false);
    }
    server.send(200, "application/json", "{\"ok\":true}");
}

void setup()
{
    Serial.begin(115200);
    Wire.begin(4, 5);
    delay(100);

    tuneFrequency(currentFreq);

    Serial.println();
    Serial.print("Connecting to WiFi");
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }

    Serial.println();
    Serial.print("Connected — http://");
    Serial.println(WiFi.localIP());

    server.on("/", handleRoot);
    server.on("/api/status", HTTP_GET, handleStatus);
    server.on("/api/tune", HTTP_POST, handleTune);
    server.on("/api/seek", HTTP_POST, handleSeek);
    server.on("/api/mute", HTTP_POST, handleMute);
    server.begin();
}

void loop()
{
    server.handleClient();
}
