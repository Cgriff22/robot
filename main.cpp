#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>

// ============================================
// MOTOR PINS
// ============================================

// LEFT SIDE

// Front motor -- reversed in software
const int LEFT_FRONT_A = 2;
const int LEFT_FRONT_B = 1;

// Rear motor
const int LEFT_REAR_A = 42;
const int LEFT_REAR_B = 41;

// RIGHT SIDE

// Front motor -- reversed in software
const int RIGHT_FRONT_A = 5;
const int RIGHT_FRONT_B = 4;

// Rear motor
const int RIGHT_REAR_A = 6;
const int RIGHT_REAR_B = 7;

// ============================================
// MPU6050 / I2C
// ============================================

const int I2C_SDA = 11;
const int I2C_SCL = 12;

const uint8_t MPU6050_ADDRESS = 0x68;

// MPU6050 registers
const uint8_t MPU6050_PWR_MGMT_1 = 0x6B;
const uint8_t MPU6050_ACCEL_XOUT_H = 0x3B;

// Sensor scale factors using default ranges:
// Accelerometer: +/- 2g
// Gyroscope:     +/- 250 deg/s
const float ACCEL_SCALE = 16384.0;
const float GYRO_SCALE = 131.0;

// Unit conversions
const float GRAVITY = 9.80665;          // m/s^2


// Read IMU at 100 Hz
const unsigned long IMU_INTERVAL_MS = 10;
unsigned long lastImuRead = 0;

// ============================================
// WI-FI
// ============================================

const char* ssid = "STING-RC";
const char* password = "robot123";

float latestAx = 0.0;
float latestAy = 0.0;
float latestAz = 0.0;

float latestGx = 0.0;
float latestGy = 0.0;
float latestGz = 0.0;

WebServer server(80);

// ============================================
// MPU6050 FUNCTIONS
// ============================================

bool writeMPURegister(uint8_t reg, uint8_t value) {

  Wire.beginTransmission(MPU6050_ADDRESS);

  Wire.write(reg);
  Wire.write(value);

  return Wire.endTransmission() == 0;
}


// --------------------------------------------
// Initialize MPU6050
// --------------------------------------------

bool initMPU6050() {

  // MPU6050 powers up in sleep mode.
  // Writing 0 to PWR_MGMT_1 wakes it up.
  if (!writeMPURegister(MPU6050_PWR_MGMT_1, 0x00)) {
    return false;
  }

  delay(100);

  return true;
}


// --------------------------------------------
// Read MPU6050 accelerometer + gyroscope
// --------------------------------------------

bool readMPU6050(
  float& ax,
  float& ay,
  float& az,
  float& gx,
  float& gy,
  float& gz
) {

  // Tell MPU6050 which register we want to start reading from.
  Wire.beginTransmission(MPU6050_ADDRESS);
  Wire.write(MPU6050_ACCEL_XOUT_H);

  // false = keep communication active for repeated start
  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  // MPU6050 sensor block:
  //
  // Accel X: 2 bytes
  // Accel Y: 2 bytes
  // Accel Z: 2 bytes
  // Temp:    2 bytes
  // Gyro X:  2 bytes
  // Gyro Y:  2 bytes
  // Gyro Z:  2 bytes
  //
  // Total = 14 bytes

  const uint8_t bytesRequested = 14;

  uint8_t bytesReceived =
      Wire.requestFrom(MPU6050_ADDRESS, bytesRequested, true);

  if (bytesReceived != bytesRequested) {
    return false;
  }

  // Combine high byte and low byte into signed 16-bit values.

  int16_t rawAx =
      (static_cast<int16_t>(Wire.read()) << 8) | Wire.read();

  int16_t rawAy =
      (static_cast<int16_t>(Wire.read()) << 8) | Wire.read();

  int16_t rawAz =
      (static_cast<int16_t>(Wire.read()) << 8) | Wire.read();

  // Temperature -- not currently needed
  int16_t rawTemp =
      (static_cast<int16_t>(Wire.read()) << 8) | Wire.read();

  int16_t rawGx =
      (static_cast<int16_t>(Wire.read()) << 8) | Wire.read();

  int16_t rawGy =
      (static_cast<int16_t>(Wire.read()) << 8) | Wire.read();

  int16_t rawGz =
      (static_cast<int16_t>(Wire.read()) << 8) | Wire.read();


  // ------------------------------------------
  // Convert accelerometer to m/s^2
  // ------------------------------------------

  ax = (rawAx / ACCEL_SCALE) * GRAVITY;
  ay = (rawAy / ACCEL_SCALE) * GRAVITY;
  az = (rawAz / ACCEL_SCALE) * GRAVITY;


  // ------------------------------------------
  // Convert gyroscope to rad/s
  // ------------------------------------------

  gx = (rawGx / GYRO_SCALE) * DEG_TO_RAD;
  gy = (rawGy / GYRO_SCALE) * DEG_TO_RAD;
  gz = (rawGz / GYRO_SCALE) * DEG_TO_RAD;

  return true;
}


// --------------------------------------------
// Send IMU data over USB serial
// --------------------------------------------

void sendIMUData() {

  float ax;
  float ay;
  float az;

  float gx;
  float gy;
  float gz;

  if (!readMPU6050(ax, ay, az, gx, gy, gz)) {
    Serial.println("IMU_ERROR");
    return;
  }

  // Format:
  //
  // IMU,ax,ay,az,gx,gy,gz
  //
  // Example:
  // IMU,0.03,-0.01,9.79,0.002,-0.004,0.001
  //
  // Prefixing with "IMU" allows the ROS node
  // to distinguish sensor data from debug messages.

  Serial.print("IMU,");

  Serial.print(ax, 6);
  Serial.print(",");

  Serial.print(ay, 6);
  Serial.print(",");

  Serial.print(az, 6);
  Serial.print(",");

  Serial.print(gx, 6);
  Serial.print(",");

  Serial.print(gy, 6);
  Serial.print(",");

  Serial.println(gz, 6);
  latestAx = ax;
  latestAy = ay;
  latestAz = az;

  latestGx = gx;
  latestGy = gy;
  latestGz = gz;
}


// ============================================
// MOTOR FUNCTIONS
// ============================================

void motorForward(int pinA, int pinB) {

  digitalWrite(pinA, HIGH);
  digitalWrite(pinB, LOW);
}


void motorBackward(int pinA, int pinB) {

  digitalWrite(pinA, LOW);
  digitalWrite(pinB, HIGH);
}


void motorStop(int pinA, int pinB) {

  digitalWrite(pinA, LOW);
  digitalWrite(pinB, LOW);
}


void leftForward() {

  motorForward(LEFT_FRONT_A, LEFT_FRONT_B);
  motorForward(LEFT_REAR_A, LEFT_REAR_B);
}


void leftBackward() {

  motorBackward(LEFT_FRONT_A, LEFT_FRONT_B);
  motorBackward(LEFT_REAR_A, LEFT_REAR_B);
}


void rightForward() {

  motorForward(RIGHT_FRONT_A, RIGHT_FRONT_B);
  motorForward(RIGHT_REAR_A, RIGHT_REAR_B);
}


void rightBackward() {

  motorBackward(RIGHT_FRONT_A, RIGHT_FRONT_B);
  motorBackward(RIGHT_REAR_A, RIGHT_REAR_B);
}


void stopRobot() {

  motorStop(LEFT_FRONT_A, LEFT_FRONT_B);
  motorStop(LEFT_REAR_A, LEFT_REAR_B);

  motorStop(RIGHT_FRONT_A, RIGHT_FRONT_B);
  motorStop(RIGHT_REAR_A, RIGHT_REAR_B);
}


// ============================================
// ROBOT MOVEMENT
// ============================================

void forward() {

  Serial.println("FORWARD");

  leftForward();
  rightForward();
}


void backward() {

  Serial.println("BACKWARD");

  leftBackward();
  rightBackward();
}


void turnLeft() {

  Serial.println("LEFT");

  // Tank/skid steer
  leftBackward();
  rightForward();
}


void turnRight() {

  Serial.println("RIGHT");

  // Tank/skid steer
  leftForward();
  rightBackward();
}


// ============================================
// WEBPAGE
// ============================================

const char webpage[] PROGMEM = R"rawliteral(

<!DOCTYPE html>

<html>

<head>

<meta name="viewport" content="width=device-width, initial-scale=1">

<title>STING RC</title>

<style>

body {
  font-family: Arial;
  text-align: center;
  background: #111;
  color: white;
  touch-action: none;
}

h1 {
  margin-top: 30px;
}

button {
  width: 110px;
  height: 90px;
  margin: 8px;
  font-size: 30px;
  border-radius: 15px;
  border: none;
}

.stop {
  background: red;
  color: white;
}

.control {
  background: #ddd;
}

</style>

</head>

<body>

<div id="imu">
  <h2>IMU Data</h2>
  <p>Accel X: <span id="ax">0</span></p>
  <p>Accel Y: <span id="ay">0</span></p>
  <p>Accel Z: <span id="az">0</span></p>

  <p>Gyro X: <span id="gx">0</span></p>
  <p>Gyro Y: <span id="gy">0</span></p>
  <p>Gyro Z: <span id="gz">0</span></p>
</div>

<h1>STING RC</h1>

<div>

  <button class="control"
    onmousedown="send('forward')"
    onmouseup="send('stop')"
    ontouchstart="send('forward')"
    ontouchend="send('stop')">

    ▲

  </button>

</div>

<div>

  <button class="control"
    onmousedown="send('left')"
    onmouseup="send('stop')"
    ontouchstart="send('left')"
    ontouchend="send('stop')">

    ◀

  </button>

  <button class="stop"
    onclick="send('stop')">

    ■

  </button>

  <button class="control"
    onmousedown="send('right')"
    onmouseup="send('stop')"
    ontouchstart="send('right')"
    ontouchend="send('stop')">

    ▶

  </button>

</div>

<div>

  <button class="control"
    onmousedown="send('backward')"
    onmouseup="send('stop')"
    ontouchstart="send('backward')"
    ontouchend="send('stop')">

    ▼

  </button>

</div>

<script>

function send(command) {
  fetch('/' + command);
}

async function updateIMU() {

  try {

    const response = await fetch('/imu');
    const data = await response.json();

    document.getElementById('ax').textContent = data.ax;
    document.getElementById('ay').textContent = data.ay;
    document.getElementById('az').textContent = data.az;

    document.getElementById('gx').textContent = data.gx;
    document.getElementById('gy').textContent = data.gy;
    document.getElementById('gz').textContent = data.gz;

  } catch (error) {

    console.log("IMU fetch error:", error);

  }
}

setInterval(updateIMU, 100);

</script>

</body>

</html>

)rawliteral";


// ============================================
// SETUP
// ============================================

void setup() {

  Serial.begin(115200);

  // ------------------------------------------
  // Motor GPIO
  // ------------------------------------------

  pinMode(LEFT_FRONT_A, OUTPUT);
  pinMode(LEFT_FRONT_B, OUTPUT);

  pinMode(LEFT_REAR_A, OUTPUT);
  pinMode(LEFT_REAR_B, OUTPUT);

  pinMode(RIGHT_FRONT_A, OUTPUT);
  pinMode(RIGHT_FRONT_B, OUTPUT);

  pinMode(RIGHT_REAR_A, OUTPUT);
  pinMode(RIGHT_REAR_B, OUTPUT);

  stopRobot();


  // ------------------------------------------
  // I2C / MPU6050
  // ------------------------------------------

  Wire.begin(I2C_SDA, I2C_SCL);

  Serial.println("Starting MPU6050...");

  if (initMPU6050()) {

    Serial.println("MPU6050 connected.");

  } else {

    Serial.println("ERROR: MPU6050 not detected.");
  }


  // ------------------------------------------
  // Start ESP32 Wi-Fi network
  // ------------------------------------------

  WiFi.softAP(ssid, password);

  Serial.println();
  Serial.println("STING RC started");

  Serial.print("Connect to WiFi: ");
  Serial.println(ssid);

  Serial.print("Open browser at: ");
  Serial.println(WiFi.softAPIP());


  // ------------------------------------------
  // Web server endpoints
  // ------------------------------------------

  server.on("/", []() {

    server.send(200, "text/html", webpage);

  });


  server.on("/forward", []() {

    forward();

    server.send(200, "text/plain", "OK");

  });


  server.on("/backward", []() {

    backward();

    server.send(200, "text/plain", "OK");

  });

echo "# uwb" >> README.md
git init
git add README.md
git commit -m "first commit"
git branch -M main
git remote add origin https://github.com/Cgriff22/uwb.git
git push -u origin main
  server.on("/left", []() {

    turnLeft();

    server.send(200, "text/plain", "OK");

  });


  server.on("/right", []() {

    turnRight();

    server.send(200, "text/plain", "OK");

  });


  server.on("/stop", []() {

    stopRobot();

    server.send(200, "text/plain", "OK");

  });

  server.on("/imu", []() {

  String data = "{";

  data += "\"ax\":" + String(latestAx, 3) + ",";
  data += "\"ay\":" + String(latestAy, 3) + ",";
  data += "\"az\":" + String(latestAz, 3) + ",";

  data += "\"gx\":" + String(latestGx, 3) + ",";
  data += "\"gy\":" + String(latestGy, 3) + ",";
  data += "\"gz\":" + String(latestGz, 3);

  data += "}";

  server.send(200, "application/json", data);
  });


  server.begin();

  Serial.println("Web server running.");
}


// ============================================
// LOOP
// ============================================

void loop() {

  // Handle RC commands
  server.handleClient();


  // ------------------------------------------
  // Read and transmit IMU at 100 Hz
  // ------------------------------------------

  unsigned long currentTime = millis();

  if (currentTime - lastImuRead >= IMU_INTERVAL_MS) {

    lastImuRead = currentTime;

    sendIMUData();
  }
}
