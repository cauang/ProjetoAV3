#include <WiFi.h>
#include <esp_now.h>
#include <ArduinoJson.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"

// === Configurações da rede Wi-Fi ===
#define WLAN_SSID       "Cauan"
#define WLAN_PASS       "0713170206"

// Hardware
#define PINO_LED        2

// === Configurações Adafruit IO ===
#define AIO_SERVER      "io.adafruit.com"
#define AIO_SERVERPORT  1883
#define AIO_USERNAME    "acanu"
#define AIO_KEY         "aio_DGna88g6F1nTcR0D5zzgOTTqaypn"  // VERIFIQUE ESTA CHAVE!

// === Cliente MQTT ===
WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

// === Feeds Adafruit IO ===
Adafruit_MQTT_Publish feed_id     = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/atividade2.idav3");
Adafruit_MQTT_Publish feed_temp   = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/atividade2.temperaturaav3");
Adafruit_MQTT_Publish feed_umid   = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/atividade2.umidadeav3");
Adafruit_MQTT_Publish feed_count  = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/atividade2.contadorav3");  

// ESP-NOW
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Buffer circular para mensagens recentes
#define MAX_MENSAGENS 20
String mensagensRecentes[MAX_MENSAGENS];
int indiceMensagem = 0;

// === Funções auxiliares === //
void piscarLED(int tempo = 100) {
  digitalWrite(PINO_LED, HIGH);
  delay(tempo);
  digitalWrite(PINO_LED, LOW);
}

bool mensagemJaRecebida(const String& idMensagem) {
  for (int i = 0; i < MAX_MENSAGENS; i++) {
    if (mensagensRecentes[i] == idMensagem) return true;
  }
  return false;
}

void registrarMensagem(const String& idMensagem) {
  mensagensRecentes[indiceMensagem] = idMensagem;
  indiceMensagem = (indiceMensagem + 1) % MAX_MENSAGENS;
}

// === Conexão MQTT Melhorada === //
void conectarMQTT() {
  static int tentativas = 0;
  unsigned long inicio = millis();
  
  Serial.print("🔄 Conectando ao Adafruit IO...");

  // Tentativa de conexão
  int8_t ret = mqtt.connect();
  
  while (ret != 0) {
    Serial.print(".");
    Serial.println(mqtt.connectErrorString(ret)); // Mostra o erro
    
    // Se falhar após 30 segundos, reinicia o WiFi
    if (millis() - inicio > 30000) {
      Serial.println("\n⏱ Timeout na conexão MQTT. Reiniciando WiFi...");
      WiFi.disconnect();
      WiFi.begin(WLAN_SSID, WLAN_PASS);
      inicio = millis();
      tentativas = 0;
    }
    
    delay(1000 * (tentativas + 1)); // Backoff exponencial
    ret = mqtt.connect();
    tentativas++;
    
    if (tentativas > 5) {
      Serial.println("\n🔁 Reiniciando ESP...");
      ESP.restart();
    }
  }

  tentativas = 0;
  Serial.println("\n✅ MQTT conectado!");
}

// === Verificação de Conexão WiFi === //
void verificarWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi desconectado. Reconectando...");
    WiFi.disconnect();
    WiFi.begin(WLAN_SSID, WLAN_PASS);
    
    unsigned long inicio = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - inicio < 10000) {
      delay(500);
      Serial.print(".");
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\n✅ WiFi reconectado!");
    } else {
      Serial.println("\n❌ Falha ao reconectar WiFi!");
    }
  }
}

// === Callback ESP-NOW === //
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *dados, int len) {
  StaticJsonDocument<200> doc;
  DeserializationError erro = deserializeJson(doc, dados, len);
  
  if (erro) {
    Serial.print("❌ Erro JSON: ");
    Serial.println(erro.c_str());
    return;
  }

  String id        = doc["id"].as<String>();
  int contador     = doc["contador"].as<int>();
  float temperatura = doc["temperatura"].as<float>();
  float umidade     = doc["umidade"].as<float>();
  String idMensagem = id + "-" + String(contador);

  if (mensagemJaRecebida(idMensagem)) {
    Serial.print("⚠️ Duplicada ignorada: ");
    Serial.println(idMensagem);
    return;
  }

  registrarMensagem(idMensagem);

  Serial.printf("📥 De %02X:%02X:%02X:%02X:%02X:%02X\n",
                info->src_addr[0], info->src_addr[1], info->src_addr[2],
                info->src_addr[3], info->src_addr[4], info->src_addr[5]);
  Serial.printf("📊 ID: %s | Cont: %d | Temp: %.2f°C | Umid: %.2f%%\n",
                id.c_str(), contador, temperatura, umidade);

  // Enviar ao Adafruit
  if (!mqtt.connected()) {
    Serial.println("🔁 MQTT desconectado. Reconectando...");
    conectarMQTT();
  }

  bool sucesso = true;
  sucesso &= feed_id.publish(id.c_str());
  sucesso &= feed_count.publish((int32_t)contador);
  sucesso &= feed_temp.publish(temperatura);
  sucesso &= feed_umid.publish(umidade);

  if (sucesso) {
    Serial.println("✅ Dados enviados ao Adafruit IO!");
    piscarLED();
  } else {
    Serial.println("❌ Erro ao enviar ao Adafruit IO.");
    mqtt.disconnect();  // força reconexão no próximo loop
  }
}

// === Setup === //
void setup() {
  Serial.begin(115200);
  pinMode(PINO_LED, OUTPUT);

  // Conecta Wi-Fi
  Serial.printf("🔌 Conectando ao Wi-Fi %s", WLAN_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WLAN_SSID, WLAN_PASS);
  
  unsigned long inicio = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - inicio < 15000) {
    delay(500);
    Serial.print(".");
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("\n✅ WiFi conectado! IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n❌ Falha ao conectar WiFi!");
    ESP.restart();
  }

  // Inicia ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("❌ Erro ao iniciar ESP-NOW");
    ESP.restart();
  }

  // Adiciona peer broadcast
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 1;
  peerInfo.encrypt = false;

  if (!esp_now_is_peer_exist(broadcastAddress)) {
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
      Serial.println("❌ Erro ao adicionar peer");
      ESP.restart();
    }
  }

  esp_now_register_recv_cb(OnDataRecv);
  Serial.println("✅ ESP-NOW pronto");

  // Conecta ao MQTT
  conectarMQTT();
}

// === Loop === //
void loop() {
  // Verifica e mantém conexões
  verificarWiFi();
  
  if (!mqtt.connected()) {
    conectarMQTT();
  }

  // Processa pacotes MQTT
  mqtt.processPackets(1000);
  
  // Mantém a conexão ativa
  static unsigned long ultimoPing = 0;
  if (millis() - ultimoPing > 15000) {
    if (!mqtt.ping()) {
      mqtt.disconnect();
    }
    ultimoPing = millis();
  }

  delay(100);
}