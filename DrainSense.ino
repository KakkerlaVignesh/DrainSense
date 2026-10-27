#include <WiFi.h>
#include <WebServer.h>

// Wi-Fi network created by ESP32
const char* AP_SSID = "DrainSense";
const char* AP_PASSWORD = "drain123";

// Hardware pins
const int TRIG_PIN = 5;
const int ECHO_PIN = 18;
const int BUZZER_PIN = 23;

// Experiment settings
const float BASELINE_SECONDS = 5.51;
const float SLOW_THRESHOLD_SECONDS = 8.27;
const float REQUIRED_DISTANCE_CHANGE_CM = 3.0;
const unsigned long TEST_TIMEOUT_MS = 90000;

// Web server
WebServer server(80);

// Test state
float currentDistance = -1;
float startDistance = -1;
float lastElapsed = 0;

unsigned long testStartMillis = 0;
unsigned long lastSensorReadMillis = 0;

bool testRunning = false;
bool alertActive = false;
bool buzzerSilenced = false;

String testType = "None";
String lastResult = "Ready to test";
String systemMessage = "Ready. Place the sensor above the bottle.";

// Recent test history
String historyType[5];
String historyResult[5];
float historyTime[5];
int historyCount = 0;

// Dashboard webpage
const char webpage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>DrainSense Dashboard</title>
<style>
*{box-sizing:border-box}
body{margin:0;font-family:Arial,sans-serif;background:#0b1220;color:#eaf0ff}
header{padding:24px 18px;background:#111c30;border-bottom:1px solid #26334b}
h1{margin:0;font-size:27px}
header p{color:#aab8d0}
main{max-width:1000px;margin:auto;padding:18px}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:14px}
.card{background:#142039;border:1px solid #293955;border-radius:15px;padding:19px;margin-bottom:14px}
.label{font-size:13px;color:#aab8d0}
.value{font-size:30px;font-weight:bold;margin-top:10px;overflow-wrap:anywhere}
.small{font-size:13px;color:#aab8d0;line-height:1.5}
button{border:0;border-radius:10px;padding:14px 16px;font-weight:bold;cursor:pointer;margin:5px 5px 5px 0}
.full{background:#32c48d;color:#071a14}
.slow{background:#ffb454;color:#281500}
.neutral{background:#354867;color:white}
.status{font-size:20px;font-weight:bold;margin-top:12px}
.good{color:#32e6a0}.bad{color:#ff7777}.warn{color:#ffbf69}
table{width:100%;border-collapse:collapse}
th,td{text-align:left;padding:10px 5px;border-bottom:1px solid #293955;font-size:13px}
footer{color:#9eacc4;font-size:12px;padding:10px 0 25px;line-height:1.5}
</style>
</head>
<body>
<header>
<h1>DrainSense</h1>
<p>Smart Drainage Monitoring Dashboard · ESP32</p>
</header>
<main>
<div class="grid">
<div class="card">
<div class="label">LIVE SENSOR DISTANCE</div>
<div class="value" id="distance">-- cm</div>
<div class="small">Sensor to water surface</div>
</div>
<div class="card">
<div class="label">TEST ELAPSED TIME</div>
<div class="value" id="elapsed">0.00 s</div>
<div class="small">Time since test started</div>
</div>
<div class="card">
<div class="label">NORMAL BASELINE</div>
<div class="value">5.51 s</div>
<div class="small">Measured normal-flow average</div>
</div>
<div class="card">
<div class="label">SLOW-DRAIN THRESHOLD</div>
<div class="value">8.27 s</div>
<div class="small">1.5 times the baseline</div>
</div>
</div>

<div class="card">
<div class="label">TEST CONTROLS</div>
<p class="small">Keep the bottle and sensor steady. Start a test, then allow water to drain. The test finishes when distance increases by 3 cm.</p>
<button class="full" onclick="startTest('Full drainage')">START FULL DRAINAGE TEST</button>
<button class="slow" onclick="startTest('Restricted drainage')">START RESTRICTED DRAINAGE TEST</button>
<button class="neutral" onclick="silenceBuzzer()">SILENCE BUZZER</button>
<button class="neutral" onclick="resetTest()">RESET TEST</button>
<div id="status" class="status">Connecting...</div>
<p id="message" class="small">Waiting for sensor data.</p>
</div>

<div class="card">
<div class="label">LATEST RESULT</div>
<div id="result" class="value" style="font-size:23px">Ready to test</div>
<p class="small">The button labels the test; the alert decision uses measured drainage time.</p>
</div>

<div class="card">
<div class="label">RECENT TEST HISTORY</div>
<table>
<thead><tr><th>Test</th><th>Time</th><th>Result</th></tr></thead>
<tbody id="history"><tr><td colspan="3">No tests recorded yet</td></tr></tbody>
</table>
</div>

<footer>DrainSense experimental prototype. Results depend on sensor stability, bottle geometry and test conditions. Not a certified real-world drain safety system.</footer>
</main>

<script>
async function refreshDashboard(){
  try{
    const response = await fetch('/api');
    if(!response.ok) throw new Error('API error');
    const d = await response.json();

    document.getElementById('distance').textContent =
      d.distance < 0 ? '-- cm' : Number(d.distance).toFixed(1)+' cm';

    document.getElementById('elapsed').textContent =
      Number(d.elapsed).toFixed(2)+' s';

    const status = document.getElementById('status');
    status.textContent = d.status;
    status.className = 'status ' + d.statusClass;

    document.getElementById('message').textContent = d.message;
    document.getElementById('result').textContent = d.result;

    const tbody = document.getElementById('history');
    if(!d.history.length){
      tbody.innerHTML = '<tr><td colspan="3">No tests recorded yet</td></tr>';
    }else{
      tbody.innerHTML = d.history.map(x =>
        '<tr><td>'+x.type+'</td><td>'+
        Number(x.time).toFixed(2)+' s</td><td>'+x.result+'</td></tr>'
      ).join('');
    }
  }catch(e){
    const status = document.getElementById('status');
    status.textContent = 'Dashboard connection lost';
    status.className = 'status bad';
  }
}

async function startTest(type){
  try{
    const response = await fetch('/start',{
      method:'POST',
      headers:{'Content-Type':'application/x-www-form-urlencoded'},
      body:'type='+encodeURIComponent(type)
    });
    alert(await response.text());
    refreshDashboard();
  }catch(e){
    alert('Cannot contact ESP32. Check your Wi-Fi connection.');
  }
}

async function silenceBuzzer(){
  try{
    await fetch('/silence',{method:'POST'});
    refreshDashboard();
  }catch(e){
    alert('Cannot contact ESP32.');
  }
}

async function resetTest(){
  try{
    await fetch('/reset',{method:'POST'});
    refreshDashboard();
  }catch(e){
    alert('Cannot contact ESP32.');
  }
}

refreshDashboard();
setInterval(refreshDashboard,1000);
</script>
</body>
</html>
)rawliteral";

// Read ultrasonic distance
float readDistanceCM(){
  digitalWrite(TRIG_PIN,LOW);
  delayMicroseconds(3);

  digitalWrite(TRIG_PIN,HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN,LOW);

  unsigned long duration = pulseIn(ECHO_PIN,HIGH,30000);

  if(duration == 0) return -1;

  float distance = duration * 0.0343 / 2.0;

  if(distance < 2 || distance > 400) return -1;

  return distance;
}

// Active-low buzzer control
void updateBuzzer(){
  if(alertActive && !buzzerSilenced){
    digitalWrite(BUZZER_PIN,LOW);
  }else{
    digitalWrite(BUZZER_PIN,HIGH);
  }
}

// Store the latest five tests
void addHistory(String type,float seconds,String result){
  if(historyCount < 5){
    historyType[historyCount] = type;
    historyTime[historyCount] = seconds;
    historyResult[historyCount] = result;
    historyCount++;
  }else{
    for(int i=0;i<4;i++){
      historyType[i] = historyType[i+1];
      historyTime[i] = historyTime[i+1];
      historyResult[i] = historyResult[i+1];
    }

    historyType[4] = type;
    historyTime[4] = seconds;
    historyResult[4] = result;
  }
}

// Finish a drainage test
void finishTest(String result){
  lastElapsed = (millis()-testStartMillis)/1000.0;
  testRunning = false;
  lastResult = result;

  if(result == "SLOW DRAINAGE ALERT"){
    alertActive = true;
    buzzerSilenced = false;
    systemMessage = "Slow drainage detected. Inspect the outlet.";
  }else if(result == "NORMAL DRAINAGE"){
    alertActive = false;
    systemMessage = "Drainage time is below the warning threshold.";
  }else{
    alertActive = false;
    systemMessage = "Test inconclusive. Check setup and try again.";
  }

  addHistory(testType,lastElapsed,lastResult);
  updateBuzzer();

  Serial.print("Result: ");
  Serial.print(lastResult);
  Serial.print(" | Time: ");
  Serial.println(lastElapsed);
}

// Start test request
void startTest(){
  if(testRunning){
    server.send(409,"text/plain",
      "A test is already running. Wait or reset it.");
    return;
  }

  currentDistance = readDistanceCM();

  if(currentDistance < 0){
    server.send(400,"text/plain",
      "Invalid sensor reading. Check sensor wiring and placement.");
    return;
  }

  testType = server.arg("type");
  if(testType.length() == 0) testType = "Drainage test";

  startDistance = currentDistance;
  testStartMillis = millis();
  lastElapsed = 0;

  testRunning = true;
  alertActive = false;
  buzzerSilenced = false;

  lastResult = "Test in progress";
  systemMessage = "Let water drain; waiting for a 3 cm distance increase.";
  updateBuzzer();

  Serial.print("Started test: ");
  Serial.println(testType);

  server.send(200,"text/plain",
    "Test started. Let the water drain and keep the sensor steady.");
}

// Reset test and buzzer
void resetTest(){
  testRunning = false;
  alertActive = false;
  buzzerSilenced = false;
  lastElapsed = 0;
  lastResult = "Ready to test";
  systemMessage = "Test reset. Prepare the bottle for the next trial.";
  updateBuzzer();

  server.send(200,"text/plain","Test reset.");
}

// Silence buzzer
void silenceBuzzer(){
  buzzerSilenced = true;
  updateBuzzer();

  server.send(200,"text/plain",
    "Buzzer silenced. Alert remains visible.");
}

// Send dashboard status as JSON
void sendStatus(){
  String status;
  String statusClass;

  if(testRunning){
    status = "TEST IN PROGRESS";
    statusClass = "warn";
  }else if(alertActive){
    status = "SLOW DRAINAGE DETECTED";
    statusClass = "bad";
  }else if(lastResult == "NORMAL DRAINAGE"){
    status = "NORMAL DRAINAGE";
    statusClass = "good";
  }else if(lastResult == "Test timed out - inconclusive"){
    status = "TEST INCONCLUSIVE";
    statusClass = "warn";
  }else{
    status = "READY";
    statusClass = "good";
  }

  float elapsed = lastElapsed;
  if(testRunning){
    elapsed = (millis()-testStartMillis)/1000.0;
  }

  String json = "{";
  json += "\"distance\":" + String(currentDistance,2) + ",";
  json += "\"elapsed\":" + String(elapsed,2) + ",";
  json += "\"status\":\"" + status + "\",";
  json += "\"statusClass\":\"" + statusClass + "\",";
  json += "\"message\":\"" + systemMessage + "\",";
  json += "\"result\":\"" + lastResult + "\",";
  json += "\"history\":[";

  for(int i=historyCount-1;i>=0;i--){
    json += "{";
    json += "\"type\":\"" + historyType[i] + "\",";
    json += "\"time\":" + String(historyTime[i],2) + ",";
    json += "\"result\":\"" + historyResult[i] + "\"";
    json += "}";

    if(i>0) json += ",";
  }

  json += "]}";

  server.send(200,"application/json",json);
}

// Setup
void setup(){
  Serial.begin(115200);
  delay(500);

  pinMode(TRIG_PIN,OUTPUT);
  pinMode(ECHO_PIN,INPUT);
  pinMode(BUZZER_PIN,OUTPUT);

  digitalWrite(TRIG_PIN,LOW);
  digitalWrite(BUZZER_PIN,HIGH);

  Serial.println();
  Serial.println("Starting DrainSense...");

  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);

  bool apStarted = WiFi.softAP(AP_SSID,AP_PASSWORD);

  if(apStarted){
    Serial.println("DrainSense Wi-Fi started!");
    Serial.print("Wi-Fi name: ");
    Serial.println(AP_SSID);
    Serial.print("Dashboard URL: http://");
    Serial.println(WiFi.softAPIP());
  }else{
    Serial.println("ERROR: Failed to start Wi-Fi access point.");
  }

  server.on("/",HTTP_GET,[](){
    server.send_P(200,"text/html",webpage);
  });

  server.on("/api",HTTP_GET,sendStatus);
  server.on("/start",HTTP_POST,startTest);
  server.on("/reset",HTTP_POST,resetTest);
  server.on("/silence",HTTP_POST,silenceBuzzer);

  server.begin();
  Serial.println("Web server started.");
}

// Main loop
void loop(){
  server.handleClient();

  if(millis()-lastSensorReadMillis >= 200){
    lastSensorReadMillis = millis();
    currentDistance = readDistanceCM();

    if(testRunning){
      unsigned long elapsedMillis = millis()-testStartMillis;

      // Water level drops, so distance increases
      if(currentDistance >= 0 &&
         currentDistance >= startDistance + REQUIRED_DISTANCE_CHANGE_CM){

        if(elapsedMillis >= (unsigned long)(SLOW_THRESHOLD_SECONDS*1000)){
          finishTest("SLOW DRAINAGE ALERT");
        }else{
          finishTest("NORMAL DRAINAGE");
        }
      }else if(elapsedMillis >= TEST_TIMEOUT_MS){
        finishTest("Test timed out - inconclusive");
      }
    }
  }

  updateBuzzer();
}
