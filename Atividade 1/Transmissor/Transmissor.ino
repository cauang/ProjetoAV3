#include <WiFi.h>
#include <esp_now.h>

uint8_t enderecoReceptor[] = {0x88, 0x13, 0xBF, 0x51, 0xCF, 0x48}; 
String mensagemSerial = "";

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("Erro ao iniciar ESP-NOW");
    return;
  }

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, enderecoReceptor, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (!esp_now_is_peer_exist(enderecoReceptor)) {
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
      Serial.println("Erro ao adicionar peer.");
      return;
    }
  }

  Serial.println("Digite sua mensagem e pressione Enter:");
}

void loop() {
  if (Serial.available()) {
    mensagemSerial = Serial.readStringUntil('\n');
    mensagemSerial.trim(); // remove espaços em branco extras

    if (mensagemSerial.length() > 0) {
      esp_err_t resultado = esp_now_send(enderecoReceptor, (uint8_t*)mensagemSerial.c_str(), mensagemSerial.length() + 1);

      if (resultado == ESP_OK) {
        Serial.println("Mensagem enviada!");
      } else {
        Serial.println("Falha no envio.");
      }
    }
  }
}
