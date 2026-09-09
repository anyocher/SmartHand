#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include "BluetoothSerial.h" // Biblioteca de Bluetooth do ESP32

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run make menuconfig to enable it.
#endif

BluetoothSerial SerialBT;
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();

// Channel mapping 
#define SERVO_MIDDLE 0
#define SERVO_THUMB  1
#define SERVO_RING   2   
#define SERVO_INDEX  3

// Safe motion limits 
#define ANGLE_MIN 25
#define ANGLE_MAX 155

// calibration
int angleToPulse(int angle) {
  angle = constrain(angle, 0, 180);
  return map(angle, 0, 180, 130, 630); 
}

int maybeInvert(uint8_t ch, int angle) {
  if (ch == SERVO_THUMB || ch == SERVO_MIDDLE) {
    return 180 - angle;
  }
  return angle;
}

// servo control
void setServoAngle(uint8_t ch, int angle) {
  angle = constrain(angle, ANGLE_MIN, ANGLE_MAX);
  angle = maybeInvert(ch, angle);

  int pulse = angleToPulse(angle);
  pwm.setPWM(ch, 0, pulse);
}

void setAll(int angle) {
  setServoAngle(SERVO_MIDDLE, angle);
  setServoAngle(SERVO_THUMB,  angle);
  setServoAngle(SERVO_RING,   angle);
  setServoAngle(SERVO_INDEX,  angle);
}

// serial passer
static String inputLine;

void responder(String msg) {
  Serial.print(msg);
  SerialBT.print(msg);
}

void handleCommand(String cmd) {
  cmd.trim();

  // Se o comando vier com \r (retorno de carro) do app, removemos aqui
  cmd.replace("\r", ""); 

  String parts[6]; // Corrigido: Matriz de strings restaurada
  int count = 0;
  int last = 0;

  for (int i = 0; i < cmd.length(); i++) {
    if (cmd[i] == ',') {
      parts[count++] = cmd.substring(last, i);
      last = i + 1;
    }
  }
  parts[count++] = cmd.substring(last);

  parts[0].toUpperCase();

  // A,90 
  if (parts[0] == "A" && count >= 2) {
    int angle = parts[1].toInt();
    setAll(angle);
    responder("OK A " + String(angle) + "\n");
    return;
  }

  // S,channel,angle 
  if (parts[0] == "S" && count >= 3) {
    int ch = parts[1].toInt();
    int angle = parts[2].toInt();
    if (ch >= 0 && ch <= 15) {
      setServoAngle(ch, angle);
      responder("OK S " + String(ch) + " " + String(angle) + "\n");
    } else {
      responder("ERR bad channel\n");
    }
    return;
  }

  // M,a0,a1,a2,a3 
  if (parts[0] == "M" && count >= 5) {
    setServoAngle(SERVO_MIDDLE, parts[1].toInt());
    setServoAngle(SERVO_THUMB,  parts[2].toInt());
    setServoAngle(SERVO_RING,   parts[3].toInt());
    setServoAngle(SERVO_INDEX,  parts[4].toInt());
    responder("OK M\n");
    return;
  }

  responder("ERR unknown command\n");
}

void setup() {
  Serial.begin(115200);

  SerialBT.begin("Braco_Robotico_ESP32"); 
  Serial.println("Bluetooth iniciado! Nome: Braco_Robotico_ESP32");

  Wire.begin(21, 22);  // SDA, SCL
  pwm.begin();
  pwm.setPWMFreq(50);
  delay(10);

  setAll(90);

  Serial.println("READY");
}

void loop() {
  // Cabo USB
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      handleCommand(inputLine);
      inputLine = "";
    } else {
      inputLine += c;
    }
  }

  // Bluetooth
  while (SerialBT.available()) {
    char c = SerialBT.read();
    if (c == '\n') {
      handleCommand(inputLine);
      inputLine = "";
    } else {
      inputLine += c;
    }
  }
}
