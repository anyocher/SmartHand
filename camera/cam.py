import cv2
import time
import sys
from pathlib import Path

import mediapipe as mp
from mediapipe.tasks import python as mp_python
from mediapipe.tasks.python import vision
import serial
from serial.tools import list_ports

# ==============================
# CONFIGURAÇÃO
# ==============================

MODEL_PATH = Path("hand_landmarker.task")

# Troque pela sua porta:
# Windows: "COM3", "COM4", etc.
# Linux:   "/dev/ttyUSB0" ou "/dev/ttyACM0"
# macOS:   "/dev/tty.usbserial-XXXX"
SERIAL_PORT = None  # None = detecta automaticamente

BAUD = 115200

ANGLE_OPEN = 30
ANGLE_CLOSED = 150
SEND_HZ = 15

CAMERA_INDEX = 0

# ==============================
# LANDMARKS DA MÃO
# ==============================

THUMB_TIP = 4
THUMB_IP = 3

INDEX_TIP = 8
INDEX_PIP = 6

MIDDLE_TIP = 12
MIDDLE_PIP = 10

RING_TIP = 16
RING_PIP = 14

PINKY_TIP = 20
PINKY_PIP = 18


def find_serial_port():
    """Tenta encontrar automaticamente uma porta serial provável."""
    if SERIAL_PORT:
        return SERIAL_PORT

    ports = list(list_ports.comports())

    if not ports:
        return None

    # Prioridade comum para ESP32 no Windows e Linux
    keywords = ("usbserial", "usbmodem", "ttyusb", "ttyacm", "ch340", "cp210", "com")

    for port in ports:
        name = (port.device + " " + port.description).lower()
        if any(k in name for k in keywords):
            return port.device

    return ports[0].device


def finger_is_open(tip: int, pip: int, landmarks) -> bool:
    """Dedo está aberto quando a ponta está acima da articulação PIP."""
    return landmarks[tip].y < landmarks[pip].y


def thumb_is_open(landmarks) -> bool:
    """
    Funciona bem para a mão direita na imagem espelhada.
    Se o dedão responder invertido, troque '<' por '>'.
    """
    return landmarks[THUMB_TIP].x < landmarks[THUMB_IP].x


def angle_for_finger(is_open: bool) -> int:
    return ANGLE_OPEN if is_open else ANGLE_CLOSED


def open_serial():
    port = find_serial_port()

    if port is None:
        print("Nenhuma porta serial encontrada.")
        print("Portas disponíveis:")

        for p in list_ports.comports():
            print(f"- {p.device} | {p.description}")

        sys.exit(1)

    try:
        ser = serial.Serial(port, BAUD, timeout=1)
    except serial.SerialException as e:
        print(f"Erro ao abrir a porta serial {port}: {e}")
        sys.exit(1)

    print(f"Serial conectada em: {port}")
    time.sleep(2)  # tempo para o ESP32 reiniciar após abrir a porta

    return ser


def main():
    if not MODEL_PATH.exists():
        print(f"Modelo não encontrado: {MODEL_PATH.resolve()}")
        print("Baixe hand_landmarker.task e coloque na mesma pasta deste script.")
        sys.exit(1)

    ser = open_serial()

    cap = cv2.VideoCapture(CAMERA_INDEX)
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)

    if not cap.isOpened():
        ser.close()
        print("Não foi possível abrir a webcam.")
        sys.exit(1)

    base_options = mp_python.BaseOptions(model_asset_path=str(MODEL_PATH))

    options = vision.HandLandmarkerOptions(
        base_options=base_options,
        running_mode=vision.RunningMode.VIDEO,
        num_hands=1,
        min_hand_detection_confidence=0.6,
        min_hand_presence_confidence=0.6,
        min_tracking_confidence=0.6,
    )

    last_send = 0.0
    last_angles = None

    with vision.HandLandmarker.create_from_options(options) as landmarker:
        while True:
            ok, frame = cap.read()

            if not ok:
                print("Falha ao ler frame da webcam.")
                break

            # Espelha a imagem para controle mais natural
            frame = cv2.flip(frame, 1)

            rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
            mp_image = mp.Image(image_format=mp.ImageFormat.SRGB, data=rgb)

            timestamp_ms = int(time.perf_counter() * 1000)
            result = landmarker.detect_for_video(mp_image, timestamp_ms)

            status = "Mao nao detectada"

            if result.hand_landmarks:
                landmarks = result.hand_landmarks[0]

                middle = angle_for_finger(
                    finger_is_open(MIDDLE_TIP, MIDDLE_PIP, landmarks)
                )
                pinky = angle_for_finger(
                    finger_is_open(PINKY_TIP, PINKY_PIP, landmarks)
                )
                ring = angle_for_finger(
                    finger_is_open(RING_TIP, RING_PIP, landmarks)
                )
                index = angle_for_finger(
                    finger_is_open(INDEX_TIP, INDEX_PIP, landmarks)
                )
                thumb = angle_for_finger(thumb_is_open(landmarks))

                last_angles = (middle, pinky, ring, index, thumb)

                # Protocolo esperado pelo ESP32:
                # M,middle,pinky,ring,index,thumb\n
                command = f"M,{middle},{pinky},{ring},{index},{thumb}\n"

                now = time.perf_counter()

                if now - last_send >= 1 / SEND_HZ:
                    try:
                        ser.write(command.encode("ascii"))
                        last_send = now
                        status = f"Enviado: {command.strip()}"
                    except serial.SerialException as e:
                        status = f"Erro serial: {e}"

                # Desenha os pontos e conexões da mão
                hand_landmarks_proto = mp.solutions.hands.HandLandmark
                drawing = mp.solutions.drawing_utils
                drawing.draw_landmarks(
                    frame,
                    landmarks,
                    hand_landmarks_proto.HAND_CONNECTIONS,
                    drawing.get_default_hand_landmarks_style(),
                    drawing.get_default_hand_connections_style(),
                )

            if last_angles:
                middle, pinky, ring, index, thumb = last_angles
                cv2.putText(
                    frame,
                    f"M:{middle} P:{pinky} R:{ring} I:{index} T:{thumb}",
                    (10, 30),
                    cv2.FONT_HERSHEY_SIMPLEX,
                    0.6,
                    (0, 255, 0),
                    2,
                )

            cv2.putText(
                frame,
                status,
                (10, 60),
                cv2.FONT_HERSHEY_SIMPLEX,
                0.55,
                (255, 255, 0),
                2,
            )

            cv2.imshow("Controle da Mao Robotica", frame)

            key = cv2.waitKey(1) & 0xFF

            if key in (27, ord("q")):
                break

    cap.release()
    cv2.destroyAllWindows()

    if ser.is_open:
        ser.close()

    print("Encerrado.")


if __name__ == "__main__":
    main()