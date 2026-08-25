/*
 * ============================================================
 *  CLOVER - IoT Campus Sanitizing Rover
 *  Team Fuse Circuit | CSE 438 | UAP
 *  FINAL VERSION
 * ============================================================
 *
 *  NO EXTRA LIBRARIES NEEDED — WiFi and WebServer are built-in.
 *
 *  WIRING GUIDE:
 *  ─────────────────────────────────────────────────
 *  L298N Motor Driver:
 *    IN1 -> GPIO 23 | IN2 -> GPIO 22
 *    IN3 -> GPIO 21 | IN4 -> GPIO 19
 *    ENA -> JUMPER  | ENB -> JUMPER
 *    12V -> Battery+ | GND -> Battery- + ESP32 GND
 *
 *  HC-SR04:
 *    TRIG -> GPIO 18
 *    ECHO -> GPIO 35
 *    VCC  -> 3.3V
 *    GND  -> GND
 *
 *  MQ2 Gas Sensor:
 *    AO  -> GPIO 34  (analog)
 *    VCC -> 5V
 *    GND -> GND
 *    DO  -> not connected
 *
 *  Relay (4-channel CH1):
 *    IN1 -> GPIO 4
 *    VCC -> 5V | GND -> GND
 *    COM -> 3.7V Battery+ | NO -> Pump+
 *    3.7V Battery- -> Pump-
 *
 *  LED    : GPIO 2 (220 ohm resistor) -> GND
 *  Buzzer : GPIO 15 -> GND
 *
 *  ESP32 power without USB:
 *    L298N 5V -> ESP32 VIN
 *
 *  PHONE:
 *    WiFi: CLOVER_ROVER | Pass: 12345678
 *    URL : 192.168.4.1
 *  ─────────────────────────────────────────────────
 */


#include <WiFi.h>
#include <WebServer.h>

// ── WiFi ──────────────────────────────────────────
const char* ssid     = "CLOVER_ROVER";
const char* password = "12345678";

// ── Pins ──────────────────────────────────────────
#define IN1        23
#define IN2        22
#define IN3        21
#define IN4        19
#define TRIG_PIN   18
#define ECHO_PIN   35
#define MQ2_PIN    34
#define RELAY_PIN   4
#define LED_PIN     2
#define BUZZER_PIN 15

// ── Settings ──────────────────────────────────────
#define OBSTACLE_CM     25    
#define MQ2_THRESHOLD  600   
#define SCAN_INTERVAL  250   
#define CONFIRM_COUNT    3   

// ── Globals ───────────────────────────────────────
WebServer server(80);

bool pumpEnabled   = false;
bool obstacleFound = false;
bool gasDetected   = false;
bool alertActive   = false;

int  closeCount = 0;
int  clearCount = 0;
int  gasCount   = 0;
int  gasClear   = 0;

unsigned long lastScan    = 0;
unsigned long alertTimer  = 0;
unsigned long bootTime    = 0;

int  gasValue   = 0;  

// ── Motors ────────────────────────────────────────
void stopMotors()   { digitalWrite(IN1,LOW);  digitalWrite(IN2,LOW);  digitalWrite(IN3,LOW);  digitalWrite(IN4,LOW);  }
void moveForward()  { digitalWrite(IN1,HIGH); digitalWrite(IN2,LOW);  digitalWrite(IN3,HIGH); digitalWrite(IN4,LOW);  }
void moveBackward() { digitalWrite(IN1,LOW);  digitalWrite(IN2,HIGH); digitalWrite(IN3,LOW);  digitalWrite(IN4,HIGH); }
void turnRight()     { digitalWrite(IN1,LOW);  digitalWrite(IN2,HIGH); digitalWrite(IN3,HIGH); digitalWrite(IN4,LOW);  }
void turnLeft()    { digitalWrite(IN1,HIGH); digitalWrite(IN2,LOW);  digitalWrite(IN3,LOW);  digitalWrite(IN4,HIGH); }

// ── Pump ──────────────────────────────────────────
void pumpON()  { digitalWrite(RELAY_PIN, LOW);  pumpEnabled = true;  }
void pumpOFF() { digitalWrite(RELAY_PIN, HIGH); pumpEnabled = false; }

// ── Alert ─────────────────────────────────────────
void alertON()  { digitalWrite(LED_PIN,HIGH); digitalWrite(BUZZER_PIN,HIGH); }
void alertOFF() { digitalWrite(LED_PIN,LOW);  digitalWrite(BUZZER_PIN,LOW);  }

// ── HC-SR04 Distance ──────────────────────────────
long getDistance() {
  unsigned long wait = millis();
  while (digitalRead(ECHO_PIN) == HIGH) {
    if (millis() - wait > 10) break;
  }
  digitalWrite(TRIG_PIN, LOW);  delayMicroseconds(4);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  unsigned long startWait = micros();
  while (digitalRead(ECHO_PIN) == LOW) {
    if (micros() - startWait > 30000) return 999;
  }
  unsigned long pulseStart = micros();
  while (digitalRead(ECHO_PIN) == HIGH) {
    if (micros() - pulseStart > 23000) return 999;
  }
  long cm = (micros() - pulseStart) * 0.034 / 2;
  if (cm < 2 || cm > 400) return 999;
  return cm;
}

// ── HTML Page ─────────────────────────────────────
const char PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html>
<head>
  <meta charset='UTF-8'>
  <meta name='viewport' content='width=device-width, initial-scale=1, user-scalable=no'>
  <title>CLOVER Rover</title>
  <style>
    *{box-sizing:border-box;margin:0;padding:0}
    body{font-family:Arial,sans-serif;background:#0f172a;color:#eee;
         display:flex;flex-direction:column;align-items:center;
         padding:16px;min-height:100vh}
    h1{color:#00d4aa;font-size:2em;margin-bottom:4px}
    .sub{color:#64748b;font-size:0.85em;margin-bottom:16px}

    .alertbox{
      display:none;width:100%;max-width:420px;
      background:#ef4444;color:white;
      padding:14px;border-radius:12px;
      font-weight:bold;font-size:1em;
      text-align:center;margin-bottom:12px;
      animation:blink 0.7s infinite;
    }
    @keyframes blink{0%,100%{opacity:1}50%{opacity:0.3}}

    .card{background:#1e293b;border-radius:16px;padding:18px;
          width:100%;max-width:420px;margin-bottom:14px}
    .card h2{color:#00d4aa;font-size:0.9em;text-transform:uppercase;
             letter-spacing:1px;margin-bottom:14px}

    .dpad{display:grid;grid-template-columns:repeat(3,80px);
          grid-template-rows:repeat(3,65px);gap:8px;justify-content:center}
    .btn{background:#0f3460;border:none;color:white;border-radius:12px;
         font-size:1.5em;cursor:pointer;user-select:none;
         -webkit-tap-highlight-color:transparent;touch-action:manipulation}
    .btn:active{background:#00d4aa;color:#0f172a;transform:scale(0.93)}
    .stopbtn{background:#c0392b}
    .stopbtn:active{background:#e74c3c}

    #pumpBtn{width:100%;padding:18px;border-radius:12px;border:none;
             font-size:1.1em;font-weight:bold;cursor:pointer;
             background:#334155;color:#94a3b8;
             -webkit-tap-highlight-color:transparent;
             transition:background 0.2s,color 0.2s}
    #pumpBtn.on{background:#00d4aa;color:#0f172a}

    .srow{display:flex;justify-content:space-between;
          margin-top:10px;font-size:0.8em;color:#64748b}
    .dot{display:inline-block;width:9px;height:9px;border-radius:50%;margin-right:5px}
    .dotg{background:#22c55e}
    .dotr{background:#ef4444}

    .infobox{background:#0f172a;border-radius:10px;padding:10px;
             margin-top:10px;font-size:0.85em;color:#64748b;text-align:center}
    .infobox span{color:#00d4aa;font-weight:bold;font-size:1.2em}

    footer{color:#334155;font-size:0.75em;margin-top:8px;text-align:center}
  </style>
</head>
<body>

<h1>CLOVER</h1>
<p class='sub'>Campus Sanitizing Rover — Team Fuse Circuit</p>

<!-- Obstacle Alert -->
<div id='obsAlert' class='alertbox'>!! OBJECT DETECTED !! Pump stopped.</div>

<!-- Gas Alert -->
<div id='gasAlert' class='alertbox' style='background:#f97316;'>!! GAS / SMOKE DETECTED !! Pump stopped.</div>

<!-- Movement -->
<div class='card'>
  <h2>MOVEMENT</h2>
  <div class='dpad'>
    <div></div>
    <button class='btn' data-dir='forward'>&#9650;</button>
    <div></div>
    <button class='btn' data-dir='left'>&#9664;</button>
    <button class='btn stopbtn' data-dir='stop'>&#9632;</button>
    <button class='btn' data-dir='right'>&#9654;</button>
    <div></div>
    <button class='btn' data-dir='backward'>&#9660;</button>
    <div></div>
  </div>
</div>

<!-- Pump + Sensors -->
<div class='card'>
  <h2>SANITIZER PUMP</h2>
  <button id='pumpBtn' onclick='togglePump()'>PUMP: OFF</button>

  <div class='srow'>
    <span><span id='obsDot' class='dot dotg'></span><span id='obsTxt'>Path Clear</span></span>
    <span><span id='gasDot' class='dot dotg'></span><span id='gasTxt'>Air Normal</span></span>
  </div>

  <div class='infobox'>
    Distance: <span id='distVal'>---</span> cm
  </div>
  <div class='infobox' style='margin-top:6px'>
    Gas Level: <span id='gasVal'>---</span>
  </div>
</div>

<footer>WiFi: CLOVER_ROVER | Pass: 12345678 | 192.168.4.1</footer>

<script>
var pumpOn = false;

function send(cmd){
  fetch('/cmd?action='+cmd)
    .then(function(r){return r.text();})
    .then(function(resp){
      if(resp==='OBSTACLE') showObsAlert();
      else if(resp==='GAS')  showGasAlert();
      else if(resp.startsWith('PUMP:')) updatePump(resp==='PUMP:ON');
    }).catch(function(){});
}

function setupButtons(){
  document.querySelectorAll('.btn').forEach(function(b){
    var dir=b.getAttribute('data-dir');
    if(!dir) return;
    b.addEventListener('touchstart',function(e){
      e.preventDefault();
      send(dir==='stop'?'stop':'move_'+dir);
    },{passive:false});
    b.addEventListener('touchend',function(e){
      e.preventDefault();
      if(dir!=='stop') send('stop');
    },{passive:false});
    b.addEventListener('touchcancel',function(e){
      e.preventDefault();
      if(dir!=='stop') send('stop');
    },{passive:false});
    b.addEventListener('mousedown',function(){ send(dir==='stop'?'stop':'move_'+dir); });
    b.addEventListener('mouseup',function(){ if(dir!=='stop') send('stop'); });
    b.addEventListener('mouseleave',function(){ if(dir!=='stop') send('stop'); });
  });
}

function togglePump(){ send(pumpOn?'pump_off':'pump_on'); }

function updatePump(state){
  pumpOn=state;
  var btn=document.getElementById('pumpBtn');
  btn.textContent='PUMP: '+(state?'ON':'OFF');
  btn.className=state?'on':'';
}

function showObsAlert(){
  document.getElementById('obsAlert').style.display='block';
  document.getElementById('obsDot').className='dot dotr';
  document.getElementById('obsTxt').textContent='Object Detected!';
  updatePump(false);
  setTimeout(function(){
    document.getElementById('obsAlert').style.display='none';
    document.getElementById('obsDot').className='dot dotg';
    document.getElementById('obsTxt').textContent='Path Clear';
  }, 4000);
}

function showGasAlert(){
  document.getElementById('gasAlert').style.display='block';
  document.getElementById('gasDot').className='dot dotr';
  document.getElementById('gasTxt').textContent='Gas Detected!';
  updatePump(false);
  setTimeout(function(){
    document.getElementById('gasAlert').style.display='none';
    document.getElementById('gasDot').className='dot dotg';
    document.getElementById('gasTxt').textContent='Air Normal';
  }, 4000);
}

// Keyboard support
document.addEventListener('keydown',function(e){
  if(e.repeat) return;
  var m={ArrowUp:'forward',w:'forward',W:'forward',
         ArrowDown:'backward',s:'backward',S:'backward',
         ArrowLeft:'left',a:'left',A:'left',
         ArrowRight:'right',d:'right',D:'right'};
  if(m[e.key]) send('move_'+m[e.key]);
});
document.addEventListener('keyup',function(e){
  var k=['ArrowUp','ArrowDown','ArrowLeft','ArrowRight',
         'w','W','s','S','a','A','d','D'];
  if(k.indexOf(e.key)>=0) send('stop');
});

// Poll every 1.5s
function pollStatus(){
  fetch('/status')
    .then(function(r){return r.text();})
    .then(function(t){
      // Format: "PUMP:ON,OBS:1,GAS:1,DIST:25,GASVAL:800"
      var p=t.split(',');
      updatePump(p[0]==='PUMP:ON');
      if(p[1]==='OBS:1') showObsAlert();
      if(p[2]==='GAS:1') showGasAlert();
      if(p[3]) document.getElementById('distVal').textContent=
        p[3].replace('DIST:','')==='999'?'Clear':p[3].replace('DIST:','');
      if(p[4]) document.getElementById('gasVal').textContent=
        p[4].replace('GASVAL:','');
    }).catch(function(){});
}
setInterval(pollStatus,1500);
window.onload=function(){ setupButtons(); pollStatus(); };
</script>
</body>
</html>
)rawhtml";

// ── Handlers ──────────────────────────────────────
void handleRoot() {
  server.send_P(200, "text/html", PAGE);
}

void handleStatus() {
  long dist = getDistance();
  String s  = pumpEnabled   ? "PUMP:ON"  : "PUMP:OFF";
  s        += obstacleFound ? ",OBS:1"   : ",OBS:0";
  s        += gasDetected   ? ",GAS:1"   : ",GAS:0";
  s        += ",DIST:"    + String(dist);
  s        += ",GASVAL:"  + String(gasValue);
  server.send(200, "text/plain", s);
}

void handleCmd() {
  String a = server.arg("action");
  String response = "OK";

  if      (a == "move_forward")  { moveForward();  response = "OK"; }
  else if (a == "move_backward") { moveBackward(); response = "OK"; }
  else if (a == "move_left")     { turnLeft();     response = "OK"; }
  else if (a == "move_right")    { turnRight();    response = "OK"; }
  else if (a == "stop")          { stopMotors();   response = "OK"; }
  else if (a == "pump_on") {
    if (!obstacleFound && !gasDetected) { pumpON(); response = "PUMP:ON"; }
    else if (obstacleFound) response = "OBSTACLE";
    else if (gasDetected)   response = "GAS";
  }
  else if (a == "pump_off") { pumpOFF(); response = "PUMP:OFF"; }

  server.send(200, "text/plain", response);
}

// ── Setup ─────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(1000);

  // Motors
  pinMode(IN1,OUTPUT); pinMode(IN2,OUTPUT);
  pinMode(IN3,OUTPUT); pinMode(IN4,OUTPUT);
  stopMotors();

  // HC-SR04
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // MQ2
  pinMode(MQ2_PIN, INPUT);

  // Relay / LED / Buzzer
  pinMode(RELAY_PIN,  OUTPUT);
  pinMode(LED_PIN,    OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pumpOFF();
  alertOFF();

  // WiFi AP
  WiFi.softAP(ssid, password);
  Serial.print("IP: "); Serial.println(WiFi.softAPIP());

  server.on("/",       handleRoot);
  server.on("/cmd",    handleCmd);
  server.on("/status", handleStatus);
  server.begin();

  bootTime = millis();
  Serial.println("CLOVER Ready!");
}

// ── Loop ──────────────────────────────────────────
void loop() {
  server.handleClient(); // ALWAYS runs — never blocked

  unsigned long now = millis();

  // Skip sensor scan for first 4 seconds only
  // server.handleClient() above still works during this time
  if (now - bootTime < 4000) return;

  if (now - lastScan > SCAN_INTERVAL) {
    lastScan = now;

    // ── HC-SR04 ────────────────────────────────
    long dist = getDistance();
    Serial.print("Dist: "); Serial.print(dist);

    if (dist != 999 && dist < OBSTACLE_CM) {
      closeCount++;
      clearCount = 0;
      if (closeCount >= CONFIRM_COUNT && !obstacleFound) {
        obstacleFound = true;
        pumpOFF();
        alertON();
        alertTimer  = now;
        alertActive = true;
        Serial.print(" >>> OBSTACLE!");
      }
    } else {
      clearCount++;
      closeCount = 0;
      if (clearCount >= CONFIRM_COUNT && obstacleFound) {
        obstacleFound = false;
        Serial.print(" >>> Clear.");
      }
    }

    // ── MQ2 ────────────────────────────────────
    gasValue = analogRead(MQ2_PIN);
    Serial.print(" | Gas: "); Serial.println(gasValue);

    if (gasValue > MQ2_THRESHOLD) {
      gasCount++;
      gasClear = 0;
      if (gasCount >= CONFIRM_COUNT && !gasDetected) {
        gasDetected = true;
        pumpOFF();
        alertON();
        alertTimer  = now;
        alertActive = true;
        Serial.println(">>> GAS DETECTED! Pump OFF.");
      }
    } else {
      gasClear++;
      gasCount = 0;
      if (gasClear >= CONFIRM_COUNT && gasDetected) {
        gasDetected = false;
        Serial.println(">>> Air normal.");
      }
    }
  }

  // ── Auto turn off LED + Buzzer after 2s ───────
  if (alertActive && (now - alertTimer > 2000)) {
    alertOFF();
    alertActive = false;
  }
}
