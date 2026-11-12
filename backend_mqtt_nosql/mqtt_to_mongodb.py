import json
import ssl
import hashlib
from datetime import datetime
import paho.mqtt.client as mqtt
from pymongo import MongoClient

# Configuración Broker MQTT TLS (ISO 27000)
BROKER = "broker.emqx.io"
PORT = 8883
TOPIC_TELEMETRY = "inacap/ti3042/rastreador/telemetria"

# Configuración Base de Datos NoSQL (MongoDB)
MONGO_URI = "mongodb://localhost:27017/"
mongo_client = MongoClient(MONGO_URI)
db = mongo_client["ti3042_iot_db"]
collection = db["telemetria_gps"]

def verify_checksum(data: dict) -> bool:
    try:
        raw = f"{data['lat']:.6f},{data['lng']:.6f},{data['speed_kmh']:.1f}"
        crc = 0
        for ch in raw:
            crc ^= ord(ch)
        return hex(crc)[2:] == data.get("checksum", "").lower()
    except Exception:
        return False

def on_connect(client, userdata, flags, rc):
    print(f"[MQTT] Conectado al broker seguro con código: {rc}")
    client.subscribe(TOPIC_TELEMETRY)

def on_message(client, userdata, msg):
    try:
        payload_str = msg.payload.decode("utf-8")
        data = json.loads(payload_str)
        data["received_at"] = datetime.utcnow().isoformat()
        data["integrity_valid"] = verify_checksum(data)

        # Guardar documento en Base de Datos NoSQL
        result = collection.insert_one(data)
        print(f"[NoSQL] Documento guardado ID: {result.inserted_id} | Integridad: {data['integrity_valid']}")
    except Exception as e:
        print(f"[ERROR] Procesando mensaje: {e}")

if __name__ == "__main__":
    client = mqtt.Client()
    client.tls_set(cert_reqs=ssl.CERT_REQUIRED)
    client.on_connect = on_connect
    client.on_message = on_message

    print("Iniciando servicio puente MQTT TLS -> MongoDB NoSQL...")
    client.connect(BROKER, PORT, 60)
    client.loop_forever()
