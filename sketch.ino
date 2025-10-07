#include <DHT.h>

// ======================= Pinos =======================
#define PIN_BTN_N   18
#define PIN_BTN_P   19
#define PIN_BTN_K   21
#define PIN_LDR_PH  34   // ADC-only
#define PIN_DHT     15
#define PIN_RELAY    5

// =================== DHT22 Config ====================
#define DHTTYPE DHT22
DHT dht(PIN_DHT, DHTTYPE);

// ============== Limiares de decisão ==================
const float HUMIDITY_MIN = 40.0;   // abaixo disso -> irrigar
const float PH_MIN       = 6.0;    // faixa neutra aproximada
const float PH_MAX       = 7.5;

// Se o seu módulo relé for ativo em LOW, troque para false
const bool RELAY_ACTIVE_HIGH = true;

// =================== Funções úteis ===================
bool lerBotao(int pin) {
  // INPUT_PULLUP: pressionado = LOW (0); solto = HIGH (1)
  return (digitalRead(pin) == LOW); // true = pressionado (nutriente OK)
}

float lerPH() {
  // ESP32 ADC: 0..4095 -> mapeia para 0..14 (simulação de pH)
  int adc = analogRead(PIN_LDR_PH);
  return (adc / 4095.0f) * 14.0f;
}

void acionaRele(bool on) {
  if (RELAY_ACTIVE_HIGH) {
    digitalWrite(PIN_RELAY, on ? HIGH : LOW);
  } else {
    digitalWrite(PIN_RELAY, on ? LOW : HIGH);
  }
}

// ======================== Setup ======================
void setup() {
  Serial.begin(115200);

  pinMode(PIN_BTN_N, INPUT_PULLUP);
  pinMode(PIN_BTN_P, INPUT_PULLUP);
  pinMode(PIN_BTN_K, INPUT_PULLUP);

  pinMode(PIN_LDR_PH, INPUT);
  pinMode(PIN_RELAY, OUTPUT);
  acionaRele(false); // começa desligado

  dht.begin();
  delay(2000); // dá tempo do DHT22 estabilizar

  Serial.println("FarmTech Fase 2 - Irrigacao Inteligente (Wokwi/ESP32)");
  Serial.println("Botoes NPK: pressionado=OK, solto=deficiencia");
}

// ========================= Loop ======================
void loop() {
  // Garante ~2s entre leituras do DHT22 (senão ele falha/NaN)
  static unsigned long lastReadMs = 0;
  if (millis() - lastReadMs < 2000) {
    delay(10);
    return;
  }
  lastReadMs = millis();

  // --- Leitura dos "sensores" ---
  bool n_ok = lerBotao(PIN_BTN_N);
  bool p_ok = lerBotao(PIN_BTN_P);
  bool k_ok = lerBotao(PIN_BTN_K);

  float umidade = dht.readHumidity();     // pode retornar NaN se muito rápido
  // opcional: float temp = dht.readTemperature();

  float ph = lerPH();

  if (isnan(umidade)) {
    Serial.println("DHT22: falha na leitura (NaN). Verifique intervalo e fiação.");
    return; // tenta de novo no próximo ciclo de 2s
  }

  // --- Regras de decisão (simples e explicitas) ---
  bool deficienciaNPK = (!n_ok || !p_ok || !k_ok);
  bool ph_ruim        = (ph < PH_MIN || ph > PH_MAX);

  bool ligar =
      (umidade < HUMIDITY_MIN) ||          // umidade muito baixa
      ((umidade < 55.0) && ph_ruim) ||     // umidade baixa e pH fora da faixa
      (deficienciaNPK && umidade < 55.0);  // falta de nutriente com umidade baixa

  acionaRele(ligar);

  // --- Log no Serial Monitor ---
  Serial.print("N=");  Serial.print(n_ok ? "OK" : "BAIXO");
  Serial.print("  P="); Serial.print(p_ok ? "OK" : "BAIXO");
  Serial.print("  K="); Serial.print(k_ok ? "OK" : "BAIXO");
  Serial.print("  | Umidade="); Serial.print(umidade, 1); Serial.print("%");
  Serial.print("  | pH="); Serial.print(ph, 2);
  Serial.print("  | Bomba="); Serial.println(ligar ? "LIGADA" : "DESLIGADA");
}