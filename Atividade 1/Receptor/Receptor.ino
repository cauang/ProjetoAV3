#include <WiFi.h>
#include <esp_now.h>

void aoReceber(const esp_now_recv_info_t *info, const uint8_t *dados, int len) {
  char mensagem[250];
  memcpy(mensagem, dados, len);
  mensagem[len] = '\0';

  Serial.print("Recebido de: ");
  char macStr[18];
  sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X",
          info->src_addr[0], info->src_addr[1], info->src_addr[2],
          info->src_addr[3], info->src_addr[4], info->src_addr[5]);
  Serial.println(macStr);

  Serial.print("Mensagem recebida: ");
  Serial.println(mensagem);
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("Erro ao iniciar ESP-NOW");
    return;
  }

  esp_now_register_recv_cb(aoReceber);
  Serial.println("Aguardando mensagens...");
}

void loop() {
  // Nada aqui
}
