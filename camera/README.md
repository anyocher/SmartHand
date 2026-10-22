# Mão Robótica com ESP32, PCA9685 e Controle por Visão Computacional

Projeto de mão robótica de 5 dedos controlada por gestos da mão capturados pela webcam.

O Python detecta a mão com MediaPipe, converte cada dedo em um ângulo de servo e envia comandos pela porta serial para o ESP32. O ESP32 controla os servos por meio do driver PWM PCA9685.

---

## Estrutura do projeto

```text
mao-robotica/
├── mao_robotica_5dedos.ino   # Código do ESP32
├── cam.py                    # Controle pela webcam
├── hand_landmarker.task      # Modelo do MediaPipe
└── README.md
```

---

## Como funciona

1. A webcam captura a imagem.
2. O MediaPipe detecta os pontos da mão.
3. O script verifica se cada dedo está aberto ou fechado.
4. Cada estado é convertido em um ângulo:
   - Dedo aberto: `ANGLE_OPEN`
   - Dedo fechado: `ANGLE_CLOSED`
5. O Python envia um comando serial para o ESP32.
6. O ESP32 interpreta o comando e move os servos pelo PCA9685.

Exemplo de comando enviado:

```text
M,90,30,150,30,90\n
```

Significado:

```text
M,middle,pinky,ring,index,thumb
```

Ou seja:

| Posição | Dedo |
|---|---|
| 1 | Médio |
| 2 | Mindinho |
| 3 | Anelar |
| 4 | Indicador |
| 5 | Polegar |

O caractere `\n` indica o fim do comando. O ESP32 só processa a linha quando recebe esse caractere.

---

## Comandos aceitos pelo ESP32

### Mover todos os dedos

```text
A,90\n
```

Move todos os servos para 90 graus.

### Mover um servo específico

```text
S,3,120\n
```

Move o canal 3 do PCA9685 para 120 graus.

### Mover a mão completa

```text
M,90,30,150,30,90\n
```

Move, nesta ordem:

```text
M,middle,pinky,ring,index,thumb
```

---

## Instalação

### 1. Instalar as bibliotecas Python

```bash
pip install --upgrade opencv-python pyserial mediapipe
```

### 2. Baixar o modelo do MediaPipe

Baixe o arquivo:

```text
hand_landmarker.task
```

Coloque o arquivo na mesma pasta do `cam.py`.

O modelo é necessário porque o `cam.py` atualizado usa a API moderna do MediaPipe Tasks, e não a API antiga `mp.solutions.hands`.

### 3. Configurar a porta serial

No início do `cam.py`, altere esta linha se necessário:

```python
SERIAL_PORT = None
```

Exemplos:

```python
# Windows
SERIAL_PORT = "COM5"

# Linux
SERIAL_PORT = "/dev/ttyUSB0"

# macOS
SERIAL_PORT = "/dev/tty.usbserial-0001"
```

Se deixar `None`, o script tenta detectar automaticamente uma porta compatível.

### 4. Carregar o código no ESP32

1. Abra o `mao_robotica_5dedos.ino` na Arduino IDE.
2. Selecione a placa ESP32 correta.
3. Selecione a porta serial correta.
4. Faça upload do código.
5. Abra o Serial Monitor somente para testes.
6. **Feche o Serial Monitor antes de rodar o `cam.py`.**

Apenas um programa pode usar a porta serial por vez.

---

## Configurações principais do cam.py

```python
MODEL_PATH = Path("hand_landmarker.task")
SERIAL_PORT = None
BAUD = 115200

ANGLE_OPEN = 30
ANGLE_CLOSED = 150
SEND_HZ = 15

CAMERA_INDEX = 0
```

| Configuração | Função |
|---|---|
| `MODEL_PATH` | Caminho do modelo `hand_landmarker.task` |
| `SERIAL_PORT` | Porta USB usada pelo ESP32 |
| `BAUD` | Velocidade da comunicação serial |
| `ANGLE_OPEN` | Ângulo enviado quando o dedo está aberto |
| `ANGLE_CLOSED` | Ângulo enviado quando o dedo está fechado |
| `SEND_HZ` | Quantas vezes por segundo o comando é enviado |
| `CAMERA_INDEX` | Webcam usada; `0` normalmente é a câmera principal |

O valor de `BAUD` precisa ser igual ao do ESP32:

```cpp
Serial.begin(115200);
```

---

## Pontos importantes do código Python

### Detecção dos dedos

Para os quatro dedos, o script compara a posição vertical da ponta do dedo com a articulação PIP:

```python
def finger_is_open(tip, pip, landmarks):
    return landmarks[tip].y < landmarks[pip].y
```

Se a ponta estiver acima da articulação, o dedo é considerado aberto.

### Detecção do polegar

O polegar é detectado comparando a posição horizontal da ponta com a articulação IP:

```python
def thumb_is_open(landmarks):
    return landmarks[THUMB_TIP].x < landmarks[THUMB_IP].x
```

Essa lógica funciona bem para a mão direita na imagem espelhada.

Se o polegar se mover invertido, troque:

```python
return landmarks[THUMB_TIP].x < landmarks[THUMB_IP].x
```

por:

```python
return landmarks[THUMB_TIP].x > landmarks[THUMB_IP].x
```

### Controle da taxa de envio

O comando não é enviado a cada frame, pois isso pode sobrecarregar a serial e o ESP32.

O envio é limitado por:

```python
SEND_HZ = 15
```

Ou seja, o Python envia no máximo 15 comandos por segundo.

---

## Pontos importantes do código ESP32

O ESP32 recebe comandos pela serial USB e também pelo Bluetooth Classic.

### Mapeamento dos canais

```cpp
#define SERVO_MIDDLE 0
#define SERVO_PINKY 1
#define SERVO_RING 2
#define SERVO_INDEX 3
#define SERVO_THUMB 4
```

| Canal PCA9685 | Dedo |
|---|---|
| 0 | Médio |
| 1 | Mindinho |
| 2 | Anelar |
| 3 | Indicador |
| 4 | Polegar |

### Limites dos servos

```cpp
#define ANGLE_MIN 25
#define ANGLE_MAX 155
```

Mesmo que o Python envie 0 ou 180 graus, o ESP32 limita o movimento para evitar danos mecânicos.

### Conversão de ângulo para pulso

```cpp
int angleToPulse(int angle) {
    angle = constrain(angle, 0, 180);
    return map(angle, 0, 180, 130, 630);
}
```

Os valores `130` e `630` não são graus. São contagens do PWM do PCA9685, que trabalha com valores de 0 a 4095.

### Inversão de alguns dedos

Alguns servos podem estar montados de forma que girem para o lado oposto:

```cpp
int maybeInvert(uint8_t ch, int angle) {
    if (ch == SERVO_PINKY || ch == SERVO_MIDDLE) {
        return 180 - angle;
    }
    return angle;
}
```

Se o polegar girar invertido, adicione `SERVO_THUMB` na condição:

```cpp
if (ch == SERVO_PINKY || ch == SERVO_MIDDLE || ch == SERVO_THUMB) {
    return 180 - angle;
}
```

---

## Ligação eletrônica recomendada

### Conexões I2C

```text
ESP32 GPIO21 --> PCA9685 SDA
ESP32 GPIO22 --> PCA9685 SCL
ESP32 3V3    --> PCA9685 VCC
ESP32 GND    --> PCA9685 GND
```

### Alimentação dos servos

```text
Bateria 11 V --> Buck 5 V --> PCA9685 V+
```

O pino `V+` do PCA9685 é o responsável por alimentar os servos.

O pino `VCC` alimenta apenas a lógica do PCA9685.

### Ground comum

Todos os grounds precisam estar conectados:

```text
GND da bateria/buck
GND do ESP32
GND do PCA9685
GND dos servos
```

Sem o ground comum, o ESP32 e o PCA9685 não têm a mesma referência elétrica, e a comunicação I2C pode falhar.

---

## Alimentação: USB e bateria ao mesmo tempo

### Situação comum

```text
Bateria 11 V --> Buck 5 V --> ESP32 e PCA9685
USB do PC --> ESP32
```

Nesse caso, existe risco de **backfeed**.

Backfeed é quando a corrente volta por um caminho que não deveria. Por exemplo:

- O USB do PC pode tentar alimentar a saída do buck.
- O buck pode tentar alimentar a porta USB do computador.
- Isso pode causar aquecimento, reinicializações, queda da USB ou danos.

### Regra segura para testes no computador

Use esta configuração:

```text
USB do PC --> ESP32
Buck 5 V --> PCA9685 V+ e servos
GND comum entre todos
```

Não ligue a saída de 5 V do buck diretamente no pino `5V/VIN` do ESP32 enquanto o USB estiver conectado, a menos que o circuito tenha proteção adequada.

### Regra prática

| Situação | Recomendação |
|---|---|
| Programar ou testar pelo PC | Alimente o ESP32 pelo USB |
| Usar a mão robótica | Alimente servos pelo buck |
| USB e bateria juntos | Use diodo Schottky ou circuito de seleção de fonte |
| Bateria desligada | Servos não funcionam, mas o ESP32 pode ligar pelo USB |

### Proteção com diodo Schottky

Se quiser poder ligar o USB e a bateria ao mesmo tempo, coloque um diodo Schottky em série com a saída de 5 V do buck antes do `5V/VIN` do ESP32.

Exemplos de diodos:

```text
SS34
SS54
1N5822
```

Isso ajuda a impedir que o USB alimente de volta o conversor buck.

Ainda assim, verifique o esquema elétrico da sua placa ESP32, porque a proteção interna varia entre modelos.

---

## Capacitor de proteção

Servos consomem picos altos de corrente ao se moverem ou travarem.

Recomenda-se colocar um capacitor eletrolítico entre `V+` e `GND`, próximo ao PCA9685:

```text
470 µF a 1000 µF
```

Isso reduz quedas de tensão e ajuda a evitar:

- Reinicializações do ESP32.
- Erros de I2C.
- Movimentos instáveis dos servos.
- Reinicialização do PCA9685.

---

## Problemas comuns

### A porta serial não abre

- Feche o Serial Monitor da Arduino IDE.
- Verifique se o driver USB do ESP32 está instalado.
- Teste outra porta COM/USB.
- No Linux, verifique permissões:

```bash
sudo usermod -a -G dialout $USER
```

Depois, reinicie o computador ou faça logout/login.

### A mão é detectada, mas os servos não mexem

Verifique:

1. Se a bateria está ligada.
2. Se o buck está entregando 5 V.
3. Se o `V+` do PCA9685 está alimentado.
4. Se o GND do ESP32 está conectado ao GND do PCA9685.
5. Se os fios SDA e SCL estão corretos.
6. Se o endereço I2C do PCA9685 está correto.

### O ESP32 reinicia sozinho

Possíveis causas:

- Servos consumindo corrente demais.
- Buck insuficiente para a corrente dos servos.
- Cabos finos ou longos demais.
- Falta de capacitor no `V+`.
- ESP32 e servos compartilhando um trilho de 5 V com queda de tensão.

### O dedo se move invertido

Inverta o ângulo no ESP32:

```cpp
return 180 - angle;
```

Ou inverta a lógica de detecção no Python.

### O polegar se move invertido

Troque o operador na função `thumb_is_open`:

```python
return landmarks[THUMB_TIP].x < landmarks[THUMB_IP].x
```

para:

```python
return landmarks[THUMB_TIP].x > landmarks[THUMB_IP].x
```

---

## Como executar

Com o ESP32 conectado e o modelo presente na pasta:

```bash
python cam.py
```

Para encerrar:

- Pressione `Q`
- Ou pressione `ESC`

O script fecha a webcam e a porta serial automaticamente.

---

## Resumo do funcionamento

```text
Webcam
  ↓
MediaPipe detecta a mão
  ↓
Python decide dedos abertos/fechados
  ↓
Python envia comando serial
  ↓
ESP32 recebe e interpreta
  ↓
PCA9685 gera PWM
  ↓
Servos movem os dedos
```
