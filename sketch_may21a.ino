#include <WiFi.h>
#include <WebServer.h>
#include <FastLED.h>

// --- CONFIGURAÇÕES WI-FI ---
const char* ssid = "Inteli.Iot";
const char* password = "@Intelix10T#";

WebServer server(80);

// --- LED e SENSOR ---
#define LED_PIN     25
#define NUM_LEDS    4
#define SENSOR_PIN  35
CRGB leds[NUM_LEDS];

// --- VARIÁVEIS DE MEDIÇÃO ---
unsigned long startTime = 0;
unsigned long measureDuration = 10000; // 10 segundos
bool isMeasuring = true;
int bpmSum = 0;
int bpmCount = 0;
int bpmFinal = 0;
int ultimoBPM = 0;
unsigned long lastBeatTime = 0;
int threshold = 515;

// --- BUFFER DE BPMs PARA GRÁFICO ---
#define MAX_POINTS 50
int bpmBuffer[MAX_POINTS] = {0};
int bpmIndex = 0;

// --- SETUP ---
void setup() {
  Serial.begin(115200);
  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.clear();
  pinMode(SENSOR_PIN, INPUT);

  WiFi.begin(ssid, password);
  Serial.print("Conectando ao Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
Serial.print("\nConectado com IP: ");
Serial.println(WiFi.localIP());


  // Rota HTML principal
  server.on("/", []() {
    server.send(200, "text/html", getHTMLPage());
  });

  // Rota que envia o último BPM detectado
  server.on("/bpm", []() {
    server.send(200, "text/plain", String(ultimoBPM));
  });

  // Rota que envia os dados para o gráfico
  server.on("/bpmdata", []() {
    String json = "[";
    for (int i = 0; i < MAX_POINTS; i++) {
      json += String(bpmBuffer[(bpmIndex + i) % MAX_POINTS]);
      if (i < MAX_POINTS - 1) json += ",";
    }
    json += "]";
    server.send(200, "application/json", json);
  });

  server.begin();
  startTime = millis();
}

// --- LOOP PRINCIPAL ---
void loop() {
  server.handleClient();

  int sensorValue = analogRead(SENSOR_PIN);
  
  if (sensorValue > threshold && millis() - lastBeatTime > 300) {
    unsigned long currentTime = millis();
    int beatInterval = currentTime - lastBeatTime;
    lastBeatTime = currentTime;
    int bpm = 60000 / beatInterval;

    bpmSum += bpm;
    bpmCount++;
    ultimoBPM = bpm;

    bpmBuffer[bpmIndex] = bpm;
    bpmIndex = (bpmIndex + 1) % MAX_POINTS;

    Serial.print("BPM detectado: ");
    Serial.println(bpm);
  }

  if (isMeasuring && millis() - startTime >= measureDuration) {
    bpmFinal = bpmCount > 0 ? bpmSum / bpmCount : 0;
    isMeasuring = false;
    Serial.print("BPM Final (média): ");
    Serial.println(bpmFinal);
  }

  if (!isMeasuring) {
    smoothBlinkLeds(bpmFinal);
  }
}

// --- PISCAR LED ---
void smoothBlinkLeds(int bpm) {
  if (bpm == 0) return;
  int duration = 60000 / bpm / 2;

    unsigned long currentTime = millis();
    int beatInterval = currentTime - lastBeatTime;
    lastBeatTime = currentTime;

  for (int b = 0; b <= 255; b += 5) {
    fill_solid(leds, NUM_LEDS, CRGB(b, 0, 0));
    FastLED.show();
    delay((duration / 51) * 1.7);
  }

  for (int b = 255; b >= 0; b -= 5) {
    fill_solid(leds, NUM_LEDS, CRGB(b, 0, 0));
    FastLED.show();
    delay((duration / 51) * 1.7);
  }

    delay(beatInterval * 5);
}

// --- HTML DA PÁGINA ---
String getHTMLPage() {
  return R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <title>Monitor Cardíaco</title>
  <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
  <style>
    body { font-family: Arial; text-align: center; background: #111; color: #fff; }
    h1 { margin-top: 20px; }
    #bpmValue { font-size: 2.5em; color: #0f0; }
    canvas { background: #000; border: 1px solid #444; }
  </style>
</head>
<body>
  <h1>Monitor de Batimentos Cardíacos</h1>
  <p>BPM Atual: <span id="bpmValue">--</span></p>
  <canvas id="ecgChart" width="600" height="200"></canvas>

  <script>
    const ctx = document.getElementById('ecgChart').getContext('2d');
    const ecgChart = new Chart(ctx, {
      type: 'line',
      data: {
        labels: Array.from({length: 50}, (_, i) => ''),
        datasets: [{
          label: 'ECG BPM',
          data: Array(50).fill(0),
          borderColor: 'lime',
          borderWidth: 2,
          pointRadius: 0,
          tension: 0.3
        }]
      },
      options: {
        animation: false,
        responsive: false,
        scales: {
          y: { min: 40, max: 180 }
        },
        plugins: {
          legend: { display: false }
        }
      }
    });

    function atualizarBPM() {
      fetch('/bpm')
        .then(res => res.text())
        .then(data => {
          document.getElementById('bpmValue').textContent = data;
        });
    }

    function atualizarGrafico() {
      fetch('/bpmdata')
        .then(res => res.json())
        .then(data => {
          ecgChart.data.datasets[0].data = data;
          ecgChart.update();
        });
    }

    setInterval(() => {
      atualizarBPM();
      atualizarGrafico();
    }, 1000);
  </script>
</body>
</html>
  )rawliteral";
}

