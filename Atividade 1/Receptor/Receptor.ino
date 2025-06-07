#include <WiFi.h>
#include <esp_now.h>

void aoReceber(const uint8_t *mac, const uint8_t *dados, int len) {
  Serial.print("Mensagem recebida de ");
  for (int i = 0; i < 6; i++) {
    Serial.printf("%02X", mac[i]);
    if (i < 5) Serial.print(":");
  }
  Serial.print(" -> ");

  char msg[250];
  memcpy(msg, dados, len);
  msg[len] = '\0'; // garante que é string
  Serial.println(msg);
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("Erro ao iniciar ESP-NOW");
    return;
  }

  esp_now_register_recv_cb(aoReceber);
  Serial.println("Receptor pronto!");
}

void loop() {
  // Nada necessário aqui
}
