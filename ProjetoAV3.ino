#include <WiFi.h>
#include <esp_now.h>
#include <DHT.h>
#include <ArduinoJson.h>

// === CONFIGURAÇÕES ===
#define PINO_DHT 4
#define TIPO_DHT DHT11
#define PINO_LED 2
#define INTERVALO_ENVIO_MS 10000  // 10 segundos entre envios

DHT dht(PINO_DHT, TIPO_DHT);

uint8_t enderecoBroadcast[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
const char* TURMA = "T169";
int numeroNo = 3; // exemplo: 1 coordenador, 2, 3 outros nós
bool ehCoordenador = false;

unsigned long ultimoEnvio = 0;
unsigned long intervalo = INTERVALO_ENVIO_MS;

int contadorMensagens = 0;

std::vector<String> mensagensRecebidas; // armazena IDs para evitar duplicatas no coordenador

// Função para piscar LED como confirmação
void piscarLED(int duracao = 50) {
  digitalWrite(PINO_LED, HIGH);
  delay(duracao);
  digitalWrite(PINO_LED, LOW);
}

// Função para imprimir MAC no Serial
void imprimirMAC(const uint8_t *mac) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr),
           "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2],
           mac[3], mac[4], mac[5]);
  Serial.println(macStr);
}

// Função para enviar dados do sensor em JSON
void enviarDadosSensor() {
  float temperatura = dht.readTemperature();
  float umidade = dht.readHumidity();

  if (isnan(temperatura) || isnan(umidade)) {
    Serial.println("Falha ao ler DHT!");
    return;
  }

  contadorMensagens++;

  char id[15];
  snprintf(id, sizeof(id), "%s-%d", TURMA, numeroNo);

  StaticJsonDocument<128> doc;
  doc["id"] = id;
  doc["contador"] = contadorMensagens;
  doc["temperatura"] = temperatura;
  doc["umidade"] = umidade;

  char buffer[128];
  size_t len = serializeJson(doc, buffer);

  esp_err_t resultado = esp_now_send(enderecoBroadcast, (uint8_t *)buffer, len);

  if (resultado == ESP_OK) {
    Serial.print("Enviado: ");
    serializeJsonPretty(doc, Serial);
    Serial.println();
    piscarLED(100); // pisca LED confirmando envio
  } else {
    Serial.println("Erro no envio ESP-NOW");
  }
}

// Callback ao receber dados
void aoReceberDados(const esp_now_recv_info_t *info, const uint8_t *dados, int len) {
  StaticJsonDocument<128> doc;
  DeserializationError erro = deserializeJson(doc, dados, len);

  if (erro) {
    Serial.print("Erro decodificar JSON: ");
    Serial.println(erro.c_str());
    return;
  }

  String idMensagem = doc["id"].as<String>() + "-" + String(doc["contador"].as<int>());

  if (ehCoordenador) {
    // Coordenador ignora duplicatas sem printar
    if (std::find(mensagensRecebidas.begin(), mensagensRecebidas.end(), idMensagem) == mensagensRecebidas.end()) {
      mensagensRecebidas.push_back(idMensagem);

      Serial.print("Recebido de ");
      imprimirMAC(info->src_addr);
      serializeJsonPretty(doc, Serial);
      Serial.println();
    }
    // Se duplicado, não faz nada
  } else {
    static String ultimaMensagem = "";
    static unsigned long ultimoTempo = 0;
    unsigned long agora = millis();

    if (idMensagem != ultimaMensagem || (agora - ultimoTempo) > 5000) {
      Serial.print("Recebido de ");
      imprimirMAC(info->src_addr);
      serializeJsonPretty(doc, Serial);
      Serial.println();

      // Reencaminha para a rede para coordenador receber
      esp_now_send(enderecoBroadcast, dados, len);

      ultimaMensagem = idMensagem;
      ultimoTempo = agora;
    }
  }
}

// Configura ESP-NOW
void configurarESPNow() {
  if (esp_now_init() != ESP_OK) {
    Serial.println("Erro ao iniciar ESP-NOW");
    return;
  }

  esp_now_register_recv_cb(aoReceberDados);

  esp_now_peer_info_t infoPeer = {};
  memcpy(infoPeer.peer_addr, enderecoBroadcast, 6);
  infoPeer.channel = 1;
  infoPeer.encrypt = false;

  if (!esp_now_is_peer_exist(enderecoBroadcast)) {
    if (esp_now_add_peer(&infoPeer) != ESP_OK) {
      Serial.println("Erro ao adicionar peer");
      return;
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(PINO_LED, OUTPUT);
  dht.begin();

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  WiFi.setChannel(1);

  Serial.print("Meu MAC: ");
  uint8_t mac[6];
  WiFi.macAddress(mac);
  imprimirMAC(mac);

  configurarESPNow();
  ultimoEnvio = millis();
}

void loop() {
  unsigned long agora = millis();
  if (agora - ultimoEnvio > intervalo) {
    ultimoEnvio = agora;
    enviarDadosSensor();
  }
  delay(100);
}
