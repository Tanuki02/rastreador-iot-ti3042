#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <TinyGPS++.h>

// ==========================================
// CONFIGURACIÓN DE RED Y BROKER MQTT (TLS)
// ==========================================
const char* ssid = "Wokwi-GUEST"; // Cambiar por red Wi-Fi real si aplica
const char* password = "";

// Broker MQTT Público con soporte TLS/SSL (Puerto 8883 - Estándar ISO 27000)
const char* mqtt_server = "broker.emqx.io";
const int mqtt_port = 8883;

const char* mqtt_topic_pub = "inacap/ti3042/rastreador/telemetria";
const char* mqtt_topic_sub = "inacap/ti3042/rastreador/comandos";

// Certificado CA raíz para validación TLS (EMQX Public Broker)
const char* ca_cert = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDrzCCApegAwIBAgIQCDvgVpBCRrGhdWrJWZHHSjANBgkqhkiG9w0BAQUFADBh
MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3
d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBD
QTAeFw0wNjExMTAwMDAwMDBaFw0zMTExMTAwMDAwMDBaMGExCzAJBgNVBAYTAlVT
MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j
b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IENBMIIBIjANBgkqhkiG
9w0BAQEFAAOCAQ8AMIIBCgKCAQEA4jvhEXLeqKTTo1eqUKKPC3eQyaKl7hLOllsB
CSDMAZOnTjC3U/dDxGkAV53ijSLdhwZAAIEJzs4bg7/fzTtxRuLWZscFs3YnFo97
nh6Vfe63SKMI2tavegw5BmV/Sl0fvBf4q77uKNd0f3p4mVmFaG5cIzJLv07A6Fpt
43C/dxC//AH2hdmoRBBYMql1GNXRor5H4idq9Joz+EkIYIvUX7Q6hL+hqkpMfT7P
T19sdl6gSzeRntwi5m3OFBqOasv+zbMUZBfHWymeMr/y7vrTC0LUq7dBMtoM1O/4
gdW7jVg/tRvoSSiicNoxBN33shbyTApOB6jtSj1etX+jkMOvJwIDAQABo2MwYTAO
BgNVHQ8BAf8EBAMCAYYwDwYDVR0TAQH/BAUwAwEB/zAdBgNVHQ4EFgQUA95QNVbR
TLtm8KPiGxvDl7I90VUwHwYDVR0jBBgwFoAUA95QNVbRTLtm8KPiGxvDl7I90VUw
DQYJKoZIhvcNAQEFBQADggEBAMucN6pIExIK+t1EnE9SsPTfrgT1eXkIoyQY/Esr
hMAtudXH/vTBH1jLuG2cenTnmCmrEbXjcKChzUyImZOMkXDiqw8cvpOp/2PV5Adg
06O/nVsJ8dWO41P0jmP6P6fbtGbfYmbW0W5BjfIttep3Sp+dWOIrWcBAI+0tKIJF
PnlUkiaY4IBIqDfv8NZ5YBberOgOzW6sRBc4L0na4UU+Krk2U886UAb3LujEV0ls
YSEY1QSteDwsOoBrp+uvFRTp2InBuThs4pFsiv9kuXclVzDAGySj4dzp30d8tbQk
CAUw7C29C79Fv1C5qfPrmAESrciIxpg0X40KPMbp1ZWVbd4=
-----END CERTIFICATE-----
)EOF";

WiFiClientSecure espClient;
PubSubClient client(espClient);
TinyGPSPlus gps;

// Pines Hardware
const int LED_PIN = 2;       // Actuador LED de estado / alerta
const int GPS_RX_PIN = 16;
const int GPS_TX_PIN = 17;

bool ledState = false;
bool highFrequencyMode = false; // Controlado remotamente desde App Android
unsigned long sendInterval = 3000;
unsigned long lastSendTime = 0;

// Variables de simulación GPS (Curicó, Chile) en caso de entorno Wokwi sin señal satelital
double simLat = -34.985400;
double simLng = -71.239400;
float simSpeed = 25.0;

// ==========================================
// ALMACENAMIENTO TEMPORAL OFF-LINE (SPIFFS)
// ==========================================
void saveOfflineData(String payload) {
  File file = SPIFFS.open("/offline_queue.jsonl", FILE_APPEND);
  if (file) {
    file.println(payload);
    file.close();
    Serial.println("[OFFLINE] Dato guardado en memoria SPIFFS.");
  }
}

void syncOfflineData() {
  if (!SPIFFS.exists("/offline_queue.jsonl")) return;

  Serial.println("[SYNC] Sincronizando datos almacenados localmente...");
  File file = SPIFFS.open("/offline_queue.jsonl", FILE_READ);
  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) {
      if (client.publish(mqtt_topic_pub, line.c_str())) {
        delay(100);
      }
    }
  }
  file.close();
  SPIFFS.remove("/offline_queue.jsonl");
  Serial.println("[SYNC] Sincronización completada y memoria liberada.");
}

// ==========================================
// RECEPCIÓN DE COMANDOS DESDE APP MÓVIL
// ==========================================
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.print("[RX APP] Comando recibido: ");
  Serial.println(message);

  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, message);
  if (error) return;

  String cmd = doc["command"] | "";
  if (cmd == "LED_ON") {
    ledState = true;
    digitalWrite(LED_PIN, HIGH);
  } else if (cmd == "LED_OFF") {
    ledState = false;
    digitalWrite(LED_PIN, LOW);
  } else if (cmd == "MODE_ECO") {
    sendInterval = 10000; // Ahorro energético: envía cada 10s
  } else if (cmd == "MODE_NORMAL") {
    sendInterval = 3000;
  }
}

// ==========================================
// RECONEXIÓN AUTOMÁTICA WI-FI Y MQTT
// ==========================================
void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  WiFi.begin(ssid, password);
  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 10) {
    delay(500);
    retries++;
  }
}

void reconnectMQTT() {
  if (WiFi.status() != WL_CONNECTED) return;
  if (!client.connected()) {
    String clientId = "ESP32-Tracker-" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println("[MQTT] Conectado de forma segura (TLS 8883).");
      client.subscribe(mqtt_topic_sub);
      syncOfflineData(); // Sincronizar datos pendientes al volver conexión
    }
  }
}

// Cálculo simple de integridad (Checksum XOR)
String calculateChecksum(String data) {
  byte crc = 0;
  for (unsigned int i = 0; i < data.length(); i++) {
    crc ^= data[i];
  }
  return String(crc, HEX);
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  pinMode(LED_PIN, OUTPUT);

  if (!SPIFFS.begin(true)) {
    Serial.println("[ERROR] Fallo al inicializar SPIFFS");
  }

  espClient.setCACert(ca_cert);
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(mqttCallback);
}

void loop() {
  connectWiFi();
  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      reconnectMQTT();
    } else {
      client.loop();
    }
  }

  while (Serial2.available() > 0) {
    gps.encode(Serial2.read());
  }

  if (millis() - lastSendTime >= sendInterval) {
    lastSendTime = millis();

    double lat = gps.location.isValid() ? gps.location.lat() : simLat;
    double lng = gps.location.isValid() ? gps.location.lng() : simLng;
    float spd = gps.speed.isValid() ? gps.speed.kmph() : simSpeed;

    // Simular leve desplazamiento en modo prueba
    if (!gps.location.isValid()) {
      simLat += 0.00012;
      simLng += 0.00008;
    }

    // Construcción del paquete JSON con validación de integridad
    StaticJsonDocument<256> doc;
    doc["device_id"] = "ESP32_GPS_01";
    doc["lat"] = lat;
    doc["lng"] = lng;
    doc["speed_kmh"] = spd;
    doc["led_status"] = ledState;
    
    String rawData = String(lat, 6) + "," + String(lng, 6) + "," + String(spd, 1);
    doc["checksum"] = calculateChecksum(rawData);

    String payload;
    serializeJson(doc, payload);

    if (WiFi.status() == WL_CONNECTED && client.connected()) {
      digitalWrite(LED_PIN, HIGH);
      client.publish(mqtt_topic_pub, payload.c_str());
      Serial.println("[TX MQTT] " + payload);
      delay(150);
      if (!ledState) digitalWrite(LED_PIN, LOW);
    } else {
      saveOfflineData(payload);
    }
  }
}
