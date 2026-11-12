package cl.inacap.ti3042.rastreador.network

import android.content.Context
import android.util.Log
import org.eclipse.paho.client.mqttv3.*
import org.eclipse.paho.client.mqttv3.persist.MemoryPersistence
import org.json.JSONObject
import javax.net.ssl.SSLSocketFactory

class SecureMqttService(
    private val context: Context,
    private val onTelemetryReceived: (String) -> Unit
) {
    private val serverUri = "ssl://broker.emqx.io:8883"
    private val topicTelemetry = "inacap/ti3042/rastreador/telemetria"
    private val topicCommands = "inacap/ti3042/rastreador/comandos"
    private var client: MqttAsyncClient? = null

    fun connectAndSubscribe() {
        val clientId = "AndroidApp-${System.currentTimeMillis()}"
        client = MqttAsyncClient(serverUri, clientId, MemoryPersistence())

        val options = MqttConnectOptions().apply {
            isAutomaticReconnect = true // Criterio 3.1.1.3: Reconexión automática
            isCleanSession = false
            connectionTimeout = 15
            keepAliveInterval = 30
            socketFactory = SSLSocketFactory.getDefault() // Criterio 3.1.4.11: Seguridad TLS/ISO 27000
        }

        client?.setCallback(object : MqttCallbackExtended {
            override fun connectComplete(reconnect: Boolean, serverURI: String?) {
                Log.d("MQTT_TLS", "Conectado (Reconexión: $reconnect)")
                client?.subscribe(topicTelemetry, 1)
            }

            override fun connectionLost(cause: Throwable?) {
                Log.w("MQTT_TLS", "Conexión perdida. Reintentando automáticamente...")
            }

            override fun messageArrived(topic: String?, message: MqttMessage?) {
                message?.let {
                    onTelemetryReceived(String(it.payload))
                }
            }

            override fun deliveryComplete(token: IMqttDeliveryToken?) {}
        })

        client?.connect(options)
    }

    // Criterio 3.1.2.5: Envío de instrucciones hacia el microcontrolador
    fun sendActuatorCommand(command: String) {
        val payload = JSONObject().apply {
            put("command", command) // Ej: "LED_ON", "LED_OFF", "MODE_ECO", "MODE_NORMAL"
            put("timestamp", System.currentTimeMillis())
        }
        val message = MqttMessage(payload.toString().toByteArray()).apply {
            qos = 1
        }
        if (client?.isConnected == true) {
            client?.publish(topicCommands, message)
        }
    }
}
