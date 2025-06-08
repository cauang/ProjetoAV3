#include <WiFi.h>
#include <esp_now.h>
#include <ArduinoJson.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"

// ========== CONFIGURAÇÕES ========== //
// WiFi
#define WLAN_SSID       "Cauan"
#define WLAN_PASS       "0713170206"

// Adafruit IO
#define AIO_SERVER      "io.adafruit.com"
#define AIO_SERVERPORT  1883
#define AIO_USERNAME    "acanu"
#define AIO_KEY         "aio_DGna88g6F1nTcR0D5zzgOTTqaypn"

// Hardware
#define PINO_LED        2

// ========== OBJETOS GLOBAIS ========== //
WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

// Feeds do Adafruit IO (grupo atividade2)
Adafruit_MQTT_Publish feed_id     = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/atividade2.idav3");
Adafruit_MQTT_Publish feed_temp   = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/atividade2.temperaturaav3");
Adafruit_MQTT_Publish feed_umid   = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/atividade2.umidadeav3");
Adafruit_MQTT_Publish feed_count  = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/atividade2.contadorav3");

// Broadcast ESP-NOW
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Buffer circular para evitar duplicatas
#define MAX_MENSAGENS 20
String mensagensRecentes[MAX_MENSAGENS];
int indiceMensagem = 0;

// ========== FUNÇÕES AUXILIARES ========== //
void piscarLED(int duracao = 100) {
  digitalWrite(PINO_LED, HIGH);
  delay(duracao);
  digitalWrite(PINO_LED, LOW);
}

bool mensagemJaRecebida(String idMensagem) {
  for (int i = 0; i < MAX_MENSAGENS; i++) {
    if (mensagensRecentes[i] == idMensagem) return true;
  }
  return false;
}

void registrarMensagem(String idMensagem) {
  mensagensRecentes[indiceMensagem] = idMensagem;
  indiceMensagem = (indiceMensagem + 1) % MAX_MENSAGENS;
}

// ========== MQTT ========== //
void connectMQTT() {
  Serial.print("Conectando ao Adafruit IO...");
  int8_t ret;
  unsigned long inicio = millis();

  while ((ret = mqtt.connect()) != 0) {
    Serial.print(".");
    delay(1000);
    if (millis() - inicio > 20000) {
      Serial.println("\nTimeout MQTT");
      return;
    }
  }

  delay(1000);  // Estabiliza
  Serial.println("\nMQTT conectado!");
}

// ========== CALLBACK DE RECEBIMENTO ========== //
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, data, len);

  if (error) {
    Serial.print("Erro no JSON: ");
    Serial.println(error.c_str());
    return;
  }

  String id = doc["id"].as<String>();
  int contador = doc["contador"].as<int>();
  float temperatura = doc["temperatura"].as<float>();
  float umidade = doc["umidade"].as<float>();
  String idMensagem = id + "-" + String(contador);

  if (mensagemJaRecebida(idMensagem)) {
    return; // Ignora duplicata
  }

  registrarMensagem(idMensagem);

  // Exibe dados no Serial
  Serial.printf("📥 De %02X:%02X:%02X:%02X:%02X:%02X\n",
                info->src_addr[0], info->src_addr[1], info->src_addr[2],
                info->src_addr[3], info->src_addr[4], info->src_addr[5]);
  Serial.printf("ID: %s | Cont: %d | Temp: %.2f°C | Umid: %.2f%%\n",
                id.c_str(), contador, temperatura, umidade);

  // Envia para o Adafruit
  if (mqtt.connected()) {
    bool enviado = feed_id.publish(id.c_str());
    enviado &= feed_temp.publish(temperatura);
    enviado &= feed_umid.publish(umidade);
    enviado &= feed_count.publish((int32_t)contador);

    if (enviado) {
      Serial.println("✅ Enviado para Adafruit IO!");
      piscarLED();
    } else {
      Serial.println("❌ Falha no envio para Adafruit");
    }
  }
}

// ========== SETUP ========== //
void setup() {
  Serial.begin(115200);
  pinMode(PINO_LED, OUTPUT);

  // Conecta ao WiFi
  Serial.printf("Conectando à rede %s", WLAN_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WLAN_SSID, WLAN_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.print("\nWiFi conectado! IP: ");
  Serial.println(WiFi.localIP());

  // Inicializa ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Erro ao iniciar ESP-NOW");
    ESP.restart();
  }

  // Adiciona peer broadcast
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 1; // Canal fixo
  peerInfo.encrypt = false;

  if (!esp_now_is_peer_exist(broadcastAddress)) {
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
      Serial.println("Erro ao adicionar peer");
      ESP.restart();
    }
  }

  esp_now_register_recv_cb(OnDataRecv);
  Serial.println("ESP-NOW pronto!");

  // Conecta ao Adafruit IO
  connectMQTT();
}

// ========== LOOP ========== //
void loop() {
  // Mantém MQTT vivo
  if (!mqtt.connected()) {
    connectMQTT();
  }

  mqtt.processPackets(10);

  static unsigned long ultimoPing = 0;
  if (millis() - ultimoPing > 15000) {
    if (!mqtt.ping()) mqtt.disconnect();
    ultimoPing = millis();
  }

  delay(100);
}
