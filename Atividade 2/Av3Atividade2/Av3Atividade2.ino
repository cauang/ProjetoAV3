#include <WiFi.h>
#include <esp_now.h>
#include <DHT.h>
#include <ArduinoJson.h>

// === CONFIGURAÇÕES ===
#define PINO_DHT 4
#define TIPO_DHT DHT11
#define PINO_LED 2
#define INTERVALO_ENVIO_MS 10000

DHT dht(PINO_DHT, TIPO_DHT);

uint8_t enderecoBroadcast[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
const char* TURMA = "T169";
int numeroNo = 3; // Número único para cada nó
bool ehCoordenador = false;
bool ultimoEnvioSucesso = false;

unsigned long ultimoEnvio = 0;
int contadorMensagens = 0;

// Buffer circular para mensagens recentes
#define MAX_MENSAGENS 20
String mensagensRecentes[MAX_MENSAGENS];
int indiceMensagem = 0;

// Função para piscar LED
void piscarLED(int duracao = 50) {
  digitalWrite(PINO_LED, HIGH);
  delay(duracao);
  digitalWrite(PINO_LED, LOW);
}

// Função para imprimir MAC (versão simplificada)
void imprimirMAC() {
  uint8_t mac[6];
  WiFi.macAddress(mac); // Usando o método padrão da biblioteca WiFi
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.println(macStr);
}

// Verifica se mensagem já foi processada
bool mensagemJaRecebida(String idMensagem) {
  for (int i = 0; i < MAX_MENSAGENS; i++) {
    if (mensagensRecentes[i] == idMensagem) {
      return true;
    }
  }
  return false;
}

// Adiciona mensagem ao buffer
void registrarMensagem(String idMensagem) {
  mensagensRecentes[indiceMensagem] = idMensagem;
  indiceMensagem = (indiceMensagem + 1) % MAX_MENSAGENS;
}

// Envia dados do sensor
void enviarDadosSensor() {
  float temperatura = dht.readTemperature();
  float umidade = dht.readHumidity();

  if (isnan(temperatura) || isnan(umidade)) {
    Serial.println("Falha ao ler DHT!");
    ultimoEnvioSucesso = false;
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
    piscarLED(100);
    ultimoEnvioSucesso = true;
  } else {
    Serial.println("Erro no envio ESP-NOW");
    ultimoEnvioSucesso = false;
  }
}

// Callback de recebimento de dados
void aoReceberDados(const esp_now_recv_info_t *info, const uint8_t *dados, int len) {
  StaticJsonDocument<128> doc;
  DeserializationError erro = deserializeJson(doc, dados, len);

  if (erro) {
    Serial.print("Erro decodificar JSON: ");
    Serial.println(erro.c_str());
    return;
  }

  String idMensagem = doc["id"].as<String>() + "-" + String(doc["contador"].as<int>());

  if (!mensagemJaRecebida(idMensagem)) {
    registrarMensagem(idMensagem);

    Serial.print("Recebido de ");
    Serial.printf("%02X:%02X:%02X:%02X:%02X:%02X", 
                 info->src_addr[0], info->src_addr[1], info->src_addr[2],
                 info->src_addr[3], info->src_addr[4], info->src_addr[5]);
    Serial.println();
    serializeJsonPretty(doc, Serial);
    Serial.println();

    // Reenvia a mensagem para a rede
    esp_now_send(enderecoBroadcast, dados, len);
  }
}

// Configura ESP-NOW
void configurarESPNow() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  
  if (esp_now_init() != ESP_OK) {
    Serial.println("Erro ao iniciar ESP-NOW");
    ESP.restart();
  }

  esp_now_register_recv_cb(aoReceberDados);

  esp_now_peer_info_t infoPeer = {};
  memcpy(infoPeer.peer_addr, enderecoBroadcast, 6);
  infoPeer.channel = 1; // Canal fixo
  infoPeer.encrypt = false;

  if (esp_now_add_peer(&infoPeer) != ESP_OK) {
    Serial.println("Erro ao adicionar peer");
    ESP.restart();
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(PINO_LED, OUTPUT);
  dht.begin();

  Serial.print("Meu MAC: ");
  imprimirMAC();

  configurarESPNow();
  ultimoEnvio = millis();
}

void loop() {
  unsigned long agora = millis();
  if (agora - ultimoEnvio > INTERVALO_ENVIO_MS) {
    ultimoEnvio = agora;
    
    for (int tentativa = 0; tentativa < 3; tentativa++) {
      enviarDadosSensor();
      if (ultimoEnvioSucesso) break;
      delay(500);
    }
  }
  
  delay(100);
}