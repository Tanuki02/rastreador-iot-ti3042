# Sistema de Rastreo Vehicular IoT Seguro (ESP32 + MQTT TLS + NoSQL + Android)

Proyecto desarrollado para la **Evaluación Sumativa Unidad 3 — Aplicaciones Móviles para IoT (TI3042)**.

## Arquitectura del Sistema

```
[Sensor GPS + ESP32] 
       │  (MQTT sobre TLS/SSL - Puerto 8883 + Checksum XOR)
       ▼
[Broker Público MQTT (EMQX)]
       ├──────────────────────────────────────┐
       ▼                                      ▼
[Servicio Puente Python]             [Aplicación Móvil Android (Kotlin)]
       │                                      │
       ▼                                      ├── Recepción Telemetría en Tiempo Real
[Base de Datos NoSQL (MongoDB)]               ├── Almacenamiento Off-line (Room/SQLite)
                                              └── Envío de Comandos a Actuadores (LED / Modo Eco)
```

## Cumplimiento de Criterios de Evaluación (ISO 27000)

1. **Comunicación Segura (3.1.1.1 / 3.1.4.11):** Uso de `WiFiClientSecure` en ESP32 y `SSLSocketFactory` en Android sobre el puerto `8883` (TLS/SSL), reemplazando el envío HTTP en texto plano.
2. **Integridad de Datos (3.1.1.2):** Cálculo de `checksum` XOR en cada trama JSON transmitida y verificada en el backend antes de persistir en MongoDB.
3. **Reconexión Automática (3.1.1.3):** Implementación de reconexión automática tanto en el bucle principal del ESP32 como mediante `isAutomaticReconnect = true` en el cliente Paho de Android.
4. **Almacenamiento Temporal Off-line y Sincronización (3.1.1.4 / 3.1.2.7):**
   - **ESP32:** Guarda los registros en la memoria flash interna (`SPIFFS`) cuando se pierde la red Wi-Fi y los publica automáticamente al reconectarse.
   - **Android:** Almacena las coordenadas localmente mediante **Room (SQLite)** con bandera `sincronizado = false` hasta restablecer la conexión.
5. **Control Bidireccional de Actuadores (3.1.2.5 / 3.1.3.9):** La app envía comandos JSON (`LED_ON`, `LED_OFF`, `MODE_ECO`, `MODE_NORMAL`) al tópico de suscripción del ESP32.
6. **Optimización Energética (3.1.3.10):** Soporte de modo ahorro (`MODE_ECO`) que espacia el intervalo de transmisión a 10 segundos y suspende envíos innecesarios.

## Estructura del Repositorio

- `/esp32_firmware/rastreador_iot_u3.ino`: Código fuente C++ para ESP32 (compatible con Wokwi y hardware físico).
- `/esp32_firmware/diagram.json`: Esquema de conexiones para simulación en Wokwi.
- `/backend_mqtt_nosql/mqtt_to_mongodb.py`: Suscriptor MQTT seguro que valida integridad y guarda registros en MongoDB.
- `/android_app/src/SecureMqttService.kt`: Cliente MQTT TLS bidireccional en Kotlin.
- `/android_app/src/LocalStorageRoom.kt`: Persistencia local off-line en Android con Room.
