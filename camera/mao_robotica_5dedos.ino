#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include "BluetoothSerial.h"

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Ative em Ferramentas > Partition Scheme (uma opção com BT) e confirme que a placa suporta Bluetooth Classic.
#endif

// PCA9685
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();

// Bluetooth
BluetoothSerial SerialBT;
#define BT_DEVICE_NAME "maorobotica"

// Mapeamento de canais (5 dedos)
// ATENÇÃO: o canal 1 era chamado de SERVO_THUMB no código antigo, mas o dedão
// não estava ligado nele. Confirme qual dedo o canal 1 move de verdade e
// renomeie se não for o mindinho.
#define SERVO_MIDDLE 0
#define SERVO_PINKY  1
#define SERVO_RING   2
#define SERVO_INDEX  3
#define SERVO_THUMB  4   // NOVO: dedão no canal 4 do PCA9685

#define NUM_FINGERS 5

#define ANGLE_MIN 25
#define ANGLE_MAX 155

// calibration
// 130 e 630 são valores de PULSO (contagem 0-4095 a 50Hz), não graus.
int angleToPulse(int angle) {
  angle = constrain(angle, 0, 180);
  return map(angle, 0, 180, 130, 630);
}

// Canais que giram ao contrário. Se o dedão girar para o lado oposto dos
// outros, adicione SERVO_THUMB nesta lista.
int maybeInvert(uint8_t ch, int angle) {
  if (ch == SERVO_PINKY || ch == SERVO_MIDDLE) {
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
  setServoAngle(SERVO_PINKY,  angle);
  setServoAngle(SERVO_RING,   angle);
  setServoAngle(SERVO_INDEX,  angle);
  setServoAngle(SERVO_THUMB,  angle);
}

// envia a resposta tanto pelo cabo USB (debug) quanto pelo Bluetooth
void reply(const String &msg) {
  Serial.println(msg);
  SerialBT.println(msg);
}

void replyf(const char *fmt, ...) {
  char buf[64];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  reply(String(buf));
}

// serial parser
static String inputLine;

void handleCommand(String cmd) {
  cmd.trim();
  if (cmd.length() == 0) return;

  const int MAX_PARTS = 6;   // "M" + 5 ângulos
  String parts[MAX_PARTS];
  int count = 0;
  int last = 0;

  for (int i = 0; i < (int)cmd.length() && count < MAX_PARTS - 1; i++) {
    if (cmd[i] == ',') {
      parts[count++] = cmd.substring(last, i);
      last = i + 1;
    }
  }
  // última parte (ou única, se não houver vírgula)
  if (count < MAX_PARTS) {
    parts[count++] = cmd.substring(last);
  }

  parts[0].toUpperCase();

  // A,90  -> todos os dedos
  if (parts[0] == "A" && count >= 2) {
    int angle = parts[1].toInt();
    setAll(angle);
    replyf("OK A %d", angle);
    return;
  }

  // S,channel,angle  -> um servo
  if (parts[0] == "S" && count >= 3) {
    int ch = parts[1].toInt();
    int angle = parts[2].toInt();
    if (ch >= 0 && ch <= 15) {
      setServoAngle(ch, angle);
      replyf("OK S %d %d", ch, angle);
    } else {
      reply("ERR bad channel");
    }
    return;
  }

  // M,middle,pinky,ring,index[,thumb]
  // Aceita 4 valores (como antes; o dedão não se move) ou 5 (com o dedão).
  if (parts[0] == "M" && count >= 5) {
    setServoAngle(SERVO_MIDDLE, parts[1].toInt());
    setServoAngle(SERVO_PINKY,  parts[2].toInt());
    setServoAngle(SERVO_RING,   parts[3].toInt());
    setServoAngle(SERVO_INDEX,  parts[4].toInt());
    if (count >= 6) {
      setServoAngle(SERVO_THUMB, parts[5].toInt());
    }
    reply("OK M");
    return;
  }

  reply("ERR unknown command");
}

// lê caracteres disponíveis em um stream (Serial ou SerialBT) e monta a linha de comando
template<typename StreamType>
void pollStream(StreamType &stream) {
  while (stream.available()) {
    char c = stream.read();
    if (c == '\n') {
      handleCommand(inputLine);
      inputLine = "";
    } else if (c != '\r') {
      inputLine += c;
      if (inputLine.length() > 64) {
        inputLine = "";
      }
    }
  }
}

void setup() {
  Serial.begin(115200);

  SerialBT.begin(BT_DEVICE_NAME);
  Serial.println("Bluetooth iniciado. Pareie com o dispositivo: " BT_DEVICE_NAME);

  // I2C for ESP32
  Wire.begin(21, 22);  // SDA, SCL
  pwm.begin();
  pwm.setOscillatorFrequency(27000000);
  pwm.setPWMFreq(50);
  delay(10);

  setAll(90);

  reply("READY");
  reply("Commands:");
  reply("A,<angle>");
  reply("S,<ch>,<angle>");
  reply("M,<middle>,<pinky>,<ring>,<index>[,<thumb>]");
}

void loop() {
  pollStream(Serial);
  pollStream(SerialBT);
}
