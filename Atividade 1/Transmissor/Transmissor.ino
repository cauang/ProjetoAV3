#include <WiFi.h>
#include <esp_now.h>

uint8_t enderecoReceptor[] = {0x88, 0x13, 0xBF, 0x51, 0xCF, 0x48};
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

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Erro ao adicionar peer");
    return;
  }

  Serial.println("Transmissor pronto!");
}

void loop() {
  const char* msg = "Mensagem simples do transmissor";
  esp_err_t resultado = esp_now_send(enderecoReceptor, (uint8_t *)msg, strlen(msg) + 1);

  if (resultado == ESP_OK) {
    Serial.println("Mensagem enviada com sucesso!");
  } else {
    Serial.println("Falha no envio.");
  }

  delay(5000); // envia a cada 5 segundos
}
