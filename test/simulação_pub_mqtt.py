"""
Simulador MQTT para dispositivo de saÃºde
Publica:
  health/{patient_id}/vitals
  health/{patient_id}/alerts
  health/{patient_id}/status

Assina:
  health/{patient_id}/command
  health/{patient_id}/config

Requisitos:
pip install paho-mqtt
"""

import json
import time
import random
import threading
from datetime import datetime
import paho.mqtt.client as mqtt

# =====================================================
# CONFIGURAÃ‡Ã•ES
# =====================================================

BROKER = "broker.hivemq.com"
PORT = 1883

PATIENT_ID = "patient_001"

BASE_TOPIC = f"VitalsWatch/{PATIENT_ID}"

TOPIC_VITALS = f"{BASE_TOPIC}/vitals"
TOPIC_ALERTS = f"{BASE_TOPIC}/alerts"
TOPIC_STATUS = f"{BASE_TOPIC}/status"

TOPIC_COMMAND = f"{BASE_TOPIC}/command"
TOPIC_CONFIG = f"{BASE_TOPIC}/config"

# =====================================================
# ESTADO DO SISTEMA
# =====================================================

running = True
device_online = True
battery = 100

sampling_interval = 5  # segundos

thresholds = {
    "low_spo2": 92,
    "high_heart_rate": 120
}

wifi = {
    "ssid": "MinhaRede",
    "rssi": -55
}

# =====================================================
# FUNÃ‡Ã•ES AUXILIARES
# =====================================================

def now():
    return datetime.utcnow().isoformat() + "Z"


def publish_json(client, topic, payload):
    client.publish(topic, json.dumps(payload), qos=1)
    print(f"[PUB] {topic}")
    print(json.dumps(payload, indent=2))
    print("-" * 50)


# =====================================================
# CALLBACKS MQTT
# =====================================================

def on_connect(client, userdata, flags, rc):
    print("Conectado ao broker!")

    client.subscribe(TOPIC_COMMAND)
    client.subscribe(TOPIC_CONFIG)

    print(f"[SUB] {TOPIC_COMMAND}")
    print(f"[SUB] {TOPIC_CONFIG}")


def on_message(client, userdata, msg):
    global running
    global sampling_interval
    global thresholds
    global wifi

    topic = msg.topic
    payload = msg.payload.decode()

    print(f"\n[RX] {topic}")
    print(payload)

    try:
        data = json.loads(payload)
    except:
        data = payload

    # ==========================================
    # COMMAND
    # ==========================================
    if topic == TOPIC_COMMAND:

        if data == "DEVICE_START":
            running = True
            print("Comando START recebido")

        elif data == "DEVICE_STOP":
            running = False
            print("Comando STOP recebido")

        elif data == "DEVICE_REBOOT":
            print("Comando REBOOT recebido")
            running = False
            time.sleep(2)
            running = True

    # ==========================================
    # CONFIG
    # ==========================================
    elif topic == TOPIC_CONFIG:

        if isinstance(data, dict):

            if "sampling" in data:
                sampling_interval = data["sampling"]

            if "thresholds" in data:
                thresholds.update(data["thresholds"])

            if "wifi" in data:
                wifi.update(data["wifi"])

            print("ConfiguraÃ§Ã£o atualizada")


# =====================================================
# PUBLICAÃ‡ÃƒO DE VITAIS
# =====================================================

def vitals_loop(client):
    global battery

    while True:

        if running:

            heart_rate = random.randint(55, 140)
            spo2 = random.randint(88, 100)

            payload = {
                "heart_rate": heart_rate,
                "spo2": spo2,
                "timestamp": now()
            }

            publish_json(client, TOPIC_VITALS, payload)

            # Alertas automÃ¡ticos
            if spo2 < thresholds["low_spo2"]:
                publish_json(client, TOPIC_ALERTS, {
                    "event": "low_spo2",
                    "value": spo2,
                    "timestamp": now()
                })

            if heart_rate > thresholds["high_heart_rate"]:
                publish_json(client, TOPIC_ALERTS, {
                    "event": "high_heart_rate",
                    "value": heart_rate,
                    "timestamp": now()
                })

            # Eventos aleatÃ³rios
            chance = random.randint(1, 20)

            if chance == 1:
                publish_json(client, TOPIC_ALERTS, {
                    "event": "fall",
                    "timestamp": now()
                })

            elif chance == 2:
                publish_json(client, TOPIC_ALERTS, {
                    "event": "abnormal_movement",
                    "timestamp": now()
                })

            battery -= random.uniform(0.1, 0.5)
            if battery < 0:
                battery = 100

        time.sleep(sampling_interval)


# =====================================================
# PUBLICAÃ‡ÃƒO STATUS
# =====================================================

def status_loop(client):

    while True:

        payload = {
            "online": device_online,
            "battery": round(battery, 1),
            "timestamp": now()
        }

        publish_json(client, TOPIC_STATUS, payload)

        time.sleep(15)


# =====================================================
# MAIN
# =====================================================

client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message

client.connect(BROKER, PORT, 60)

threading.Thread(target=vitals_loop, args=(client,), daemon=True).start()
threading.Thread(target=status_loop, args=(client,), daemon=True).start()

client.loop_forever()