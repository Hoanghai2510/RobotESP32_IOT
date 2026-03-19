#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>
#include <HTTPClient.h>

/* WIFI */
const char* ssid = "HOANG HAI";
const char* password = "25102005";

/* AUTH */
const char* www_username = "admin";
const char* www_password = "123456";
String secretKey = "abc123";

/* THINGSPEAK */
String apiKey = "X2ZCX24PM7S3D14B";

/* SERVER */
WebServer server(80);

/* MOTOR */
#define IN1 27
#define IN2 26
#define IN3 25
#define IN4 33
#define ENA 14
#define ENB 32

/* PWM CHANNEL */
#define PWM_A 0
#define PWM_B 1

/* ULTRASONIC */
#define TRIG 5
#define ECHO 18

/* SERVO */
#define SERVO_PIN 13
Servo radar;

/* MODE */
bool autoMode = true;

/* SPEED */
int currentSpeed = 0;

/* DATA */
int distanceFront = 0;
float batteryVoltage = 0;

/* ================= AUTH ================= */

bool isAuth(){
  if(!server.authenticate(www_username,www_password)){
    server.requestAuthentication();
    return false;
  }
  return true;
}

bool checkKey(){
  if(server.arg("key") != secretKey){
    server.send(403,"text/plain","Forbidden");
    return false;
  }
  return true;
}

/* ================= MOTOR ================= */

void setSpeedSmooth(int target){
  if(target > currentSpeed){
    for(int i=currentSpeed;i<=target;i+=5){
      ledcWrite(PWM_A,i);
      ledcWrite(PWM_B,i);
      delay(2);
    }
  }else{
    for(int i=currentSpeed;i>=target;i-=5){
      ledcWrite(PWM_A,i);
      ledcWrite(PWM_B,i);
      delay(2);
    }
  }
  currentSpeed = target;
}

void stopCar(){ setSpeedSmooth(0); }

void forward(int s){
  digitalWrite(IN1,HIGH); digitalWrite(IN2,LOW);
  digitalWrite(IN3,HIGH); digitalWrite(IN4,LOW);
  setSpeedSmooth(s);
}

void backward(int s){
  digitalWrite(IN1,LOW); digitalWrite(IN2,HIGH);
  digitalWrite(IN3,LOW); digitalWrite(IN4,HIGH);
  setSpeedSmooth(s);
}

void turnLeft(){
  digitalWrite(IN1,LOW); digitalWrite(IN2,HIGH);
  digitalWrite(IN3,HIGH); digitalWrite(IN4,LOW);
  setSpeedSmooth(150);
}

void turnRight(){
  digitalWrite(IN1,HIGH); digitalWrite(IN2,LOW);
  digitalWrite(IN3,LOW); digitalWrite(IN4,HIGH);
  setSpeedSmooth(150);
}

/* ================= SENSOR ================= */

int readDistance(){
  digitalWrite(TRIG,LOW); delayMicroseconds(2);
  digitalWrite(TRIG,HIGH); delayMicroseconds(10);
  digitalWrite(TRIG,LOW);

  long duration = pulseIn(ECHO,HIGH,25000);
  int d = duration*0.034/2;
  if(d==0) d=400;
  return d;
}

float readBattery(){
  int val = analogRead(34);
  return val * (3.3 / 4095.0) * 2;
}

/* ================= SERVO ================= */

void servoMove(int pos){
  int cur = radar.read();
  if(pos > cur){
    for(int i=cur;i<=pos;i++){ radar.write(i); delay(2);}
  }else{
    for(int i=cur;i>=pos;i--){ radar.write(i); delay(2);}
  }
}

/* ================= AI ================= */

int scanBestDirection(){
  int angles[] = {30,60,90,120,150};
  int bestAngle = 90;
  int maxDist = 0;

  for(int i=0;i<5;i++){
    servoMove(angles[i]);
    delay(80);
    int d = readDistance();

    if(d > maxDist){
      maxDist = d;
      bestAngle = angles[i];
    }
  }
  return bestAngle;
}

int stuckCount = 0;

void autoDrive(){

  servoMove(90);
  distanceFront = readDistance();

  if(distanceFront > 80){
    forward(200);
    stuckCount = 0;
  }
  else if(distanceFront > 40){
    forward(140);
    stuckCount = 0;
  }
  else{
    stopCar();
    delay(150);

    stuckCount++;

    int best = scanBestDirection();

    if(best < 90) turnRight();
    else turnLeft();

    delay(350);

    if(stuckCount > 4){
      backward(180);
      delay(400);
      turnRight();
      delay(300);
      stuckCount = 0;
    }
  }
}

/* ================= CLOUD ================= */

void sendToCloud(){

  if(WiFi.status()==WL_CONNECTED){

    HTTPClient http;

    String url = "http://api.thingspeak.com/update?api_key=" + apiKey;
    url += "&field1=" + String(distanceFront);
    url += "&field2=" + String(autoMode);
    url += "&field3=" + String(batteryVoltage);

    http.begin(url);
    http.GET();
    http.end();
  }
}

/* ================= WEB ================= */

void handleRoot(){

if(!isAuth()) return;

String page = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
body{background:#0f172a;color:white;text-align:center;font-family:sans-serif;}
.card{background:#1e293b;padding:15px;border-radius:12px;margin:10px;}
button{width:80px;height:60px;margin:5px;border-radius:12px;font-size:20px;border:none;}
.green{background:#22c55e;}
.blue{background:#3b82f6;}
.red{background:#ef4444;}
.yellow{background:#f59e0b;}
</style>
</head>

<body>

<h2>🤖 ESP32 AI ROBOT</h2>

<div class="card">
Distance: <span id="d">0</span> cm<br>
Battery: <span id="b">0</span> V<br>
Mode: <span id="m">AUTO</span>
</div>

<button class="green" onclick="cmd('f')">↑</button><br>
<button class="blue" onclick="cmd('l')">←</button>
<button class="red" onclick="cmd('s')">■</button>
<button class="blue" onclick="cmd('r')">→</button><br>
<button class="green" onclick="cmd('b')">↓</button>

<br><br>

<button class="yellow" onclick="cmd('auto')">AUTO</button>
<button class="yellow" onclick="cmd('manual')">MANUAL</button>

<script>
function cmd(c){ fetch("/"+c+"?key=abc123"); }

setInterval(()=>{
fetch("/data?key=abc123")
.then(r=>r.json())
.then(d=>{
document.getElementById("d").innerHTML=d.distance;
document.getElementById("b").innerHTML=d.battery;
document.getElementById("m").innerHTML=d.mode;
});
},2000);
</script>

</body>
</html>
)rawliteral";

server.send(200,"text/html",page);
}

/* API DATA */

void handleData(){
if(!checkKey()) return;

String json = "{";
json += "\"distance\":" + String(distanceFront) + ",";
json += "\"battery\":" + String(batteryVoltage) + ",";
json += "\"mode\":\"" + String(autoMode?"AUTO":"MANUAL") + "\"";
json += "}";

server.send(200,"application/json",json);
}

/* ================= SERVER ================= */

void setupServer(){

server.on("/",handleRoot);
server.on("/data",handleData);

server.on("/f",[]{ if(!checkKey())return; autoMode=false; forward(200); server.send(200);});
server.on("/b",[]{ if(!checkKey())return; autoMode=false; backward(200); server.send(200);});
server.on("/l",[]{ if(!checkKey())return; autoMode=false; turnLeft(); server.send(200);});
server.on("/r",[]{ if(!checkKey())return; autoMode=false; turnRight(); server.send(200);});
server.on("/s",[]{ if(!checkKey())return; autoMode=false; stopCar(); server.send(200);});

server.on("/auto",[]{ if(!checkKey())return; autoMode=true; server.send(200);});
server.on("/manual",[]{ if(!checkKey())return; autoMode=false; stopCar(); server.send(200);});

server.begin();
}

/* ================= SETUP ================= */

void setup(){

Serial.begin(115200);

pinMode(TRIG,OUTPUT);
pinMode(ECHO,INPUT);

pinMode(IN1,OUTPUT);
pinMode(IN2,OUTPUT);
pinMode(IN3,OUTPUT);
pinMode(IN4,OUTPUT);

/* PWM FIX */
ledcSetup(PWM_A,1000,8);
ledcSetup(PWM_B,1000,8);

ledcAttachPin(ENA,PWM_A);
ledcAttachPin(ENB,PWM_B);

radar.attach(SERVO_PIN);

/* WIFI */
WiFi.begin(ssid,password);
while(WiFi.status()!=WL_CONNECTED){ delay(500); }

/* SERVER */
setupServer();
}

/* ================= LOOP ================= */

unsigned long lastSend = 0;

void loop(){

server.handleClient();

batteryVoltage = readBattery();

if(autoMode) autoDrive();

if(millis() - lastSend > 15000){
  sendToCloud();
  lastSend = millis();
}

}