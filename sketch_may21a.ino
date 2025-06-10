#include <WiFi.h>
#include <WebServer.h>
#include <FastLED.h>

// --- CONFIGURAÇÕES WI-FI ---
const char* ssid = "Inteli.Iot";
const char* password = "@Intelix10T#";

WebServer server(80);

// --- LED, SENSOR e BOTÃO ---
#define LED_PIN     25
#define NUM_LEDS    4
#define SENSOR_PIN  35
#define BTN_PIN     32
#define BTN_PIN2    27
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

// --- VARIÁVEIS DO BOTÃO ---
unsigned long lastButtonPress = 0;
const unsigned long debounceDelay = 800;
bool ledsOn = true;
int estado = 0;

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
  pinMode(BTN_PIN, INPUT_PULLUP);  // Alterado para INPUT_PULLUP
  pinMode(BTN_PIN2, INPUT_PULLUP);  // Alterado para INPUT_PULLUP


  WiFi.begin(ssid, password);
  Serial.print("Conectando ao Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.print("\nConectado com IP: ");
  Serial.println(WiFi.localIP());

  // Rotas do servidor (mantidas iguais)
  server.on("/", []() { server.send(200, "text/html", getHTMLPage()); });
  server.on("/bpm", []() { server.send(200, "text/plain", String(ultimoBPM)); });
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

  // Leitura do botão com debounce
    if (digitalRead(BTN_PIN) == LOW && millis() - lastButtonPress > debounceDelay) {
    lastButtonPress = millis();
    estado++;
    
    if (estado == 1) {
      ledsEstaticos();
    }
    else if (estado == 2) {
      turnOffLeds();
    }
    else if (estado>2){
      estado = 0;
    }
  }

  if (digitalRead(BTN_PIN2) == LOW && millis() - lastButtonPress > debounceDelay) {
    lastButtonPress = millis();
      ESP.restart();
  }

  // Restante da lógica original (sensor, medição, etc.)
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

  if (!isMeasuring && estado == 0) {  // Só anima se os LEDs estiverem ligados
    smoothBlinkLeds(bpmFinal);
  }
}

void turnOffLeds() {
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();
}

void ledsEstaticos() {
  fill_solid(leds, NUM_LEDS, CRGB::Red);
  FastLED.show();
}
// --- PISCAR LED ---
void smoothBlinkLeds(int bpm) {
  if (bpm == 0 || estado != 0) return;

  int duration = 60000 / bpm / 2;
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
}

// HTML (mantido igual)
String getHTMLPage() {
  return R"rawliteral(
    <!DOCTYPE html>
<html lang="pt-BR">
<head>
    <meta charset="UTF-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1.0"/>
    <title>Luminária Med-In</title>
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
    <link rel="preconnect" href="https://fonts.googleapis.com">
    <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
    <link href="https://fonts.googleapis.com/css2?family=Milonga&family=Neuton:ital,wght@0,200;0,300;0,400;0,700;0,800;1,400&display=swap" rel="stylesheet">
    <link href="https://fonts.googleapis.com/css2?family=Montserrat:wght@100..900&display=swap" rel="stylesheet">
    <style>
        body {
            margin: 0;
            font-family: "Neuton", serif;
            background-color: #ffffff;
            color: #000000;
        }
        header {
            background-color: #00B1B1;
            padding: 15px 20px;
            display: flex;
            align-items: center;
            justify-content: space-between;
            font-family: "Milonga", serif;
        }
        header h1 {
            font-size: 0.8rem;
            color: #000;
            margin: 0;
            font-family: "Milonga", serif;
        }
        h2 {
            font-size: 2.5rem;
            color: #000;
            margin: 0;
            font-family: "Arial", normal;
        }
        nav button {
            background-color: #D9D9D9;
            border: none;
            margin-left: 10px;
            padding: 5px 10px;
            font-weight: bold;
            cursor: pointer;
        }
        footer {
            background-color: #00B1B1;
            height: 200px;
            margin-top: 50px;
        }
        #bpmSection {
            font-family: 'Montserrat', sans-serif;
            text-align: left;
            background: #ffffff;
            color: #000000;
            padding-bottom: 50px;
        }
        #bpmValue { 
            font-size: 2.5em; 
            color: #0f0; 
        }
        #bpmSection canvas {
            background: #000;
            border: 1px solid #444;
            display: block;
            margin: 0 auto;
            border-radius: 10px;
        }
        #bpmSection p {
            width: 600px;
            margin: 20px auto;
            color: #02494d;
            background-color: #ebf3f3;
            padding: 20px 10px;
            border-radius: 10px;
            box-shadow: 5px 5px 40px #000;
        }
        #bpmSection h3 {
            font-weight: bold;
        }
        .canvas {
            display: flex;             
            justify-content: flex-start; 
            align-items: flex-start;     
            margin-top: 50px;
        }
        .grafico-logo {
            display: flex;
            justify-content: center;
            align-items: center;
            gap: 40px; 
            margin-bottom: 30px;
        }
        #historiaSection {
            display: none;
        }
        .hero {
            background-color: #a8c3cf;
            height: 200px;
            display: flex;
            align-items: flex-end;
            justify-content: center;
            padding: 20px;
        }
        .section {
            padding: 40px 20px;
        }
        .section h2 {
            font-size: 3rem;
            font-family: Arial, sans-serif;
        }
        .section p {
            font-size: 1.2rem;
            line-height: 1.6;
            width: 80%;
            margin: 10px auto;
            text-align: center;
        }
        .media {
            background-color: #A4C1CD;
            padding: 80px 10px;
            text-align: center;
            font-size: 1.2rem;
            width: 60%;
            margin: 0 auto;
        }
        .grid {
            display: grid;
            grid-template-columns: repeat(2, 1fr);
            gap: 20px;
            padding: 20px 0;
            max-width: 700px;
            margin: 0 auto;
        }
        .card {
            background-color: #ddd;
            height: 200px;
        }
        .caption {
            text-align: center;
            font-weight: bold;
            margin-top: 5px;
        }
        .repo p {
            font-size: 1.5rem;
            width: 80%;
            margin: 30px auto;
            text-align: left;
        }
        .repo a {
            font-size: 1rem;
            color: #000;
            text-decoration: none;
            font-weight: bold;
            display: block;
            width: 80%;
            margin: 30px auto;
            text-align: left;
        }
        .collab h2 {
            font-size: 1.8rem;
            width: 80%;
            margin: 30px auto;
            text-align: center;
        }
        .collab-group {
            text-align: center;
            margin-bottom: 20px;
        }
        .avatars {
            display: flex;
            justify-content: center;
            flex-wrap: wrap;
            gap: 20px;
            margin-top: 10px;
        }
        .avatar {
            display: flex;
            flex-direction: column;
            align-items: center;
            margin: 0 15px;
        }
        .avatar div {
            width: 60px;
            height: 60px;
            border-radius: 50%;
            background-color: #D9D9D9;
            margin-bottom: 5px;
        }
        .avatar span {
            display: block;
            white-space: nowrap;
        }
        hr {
            width: 80%;
            margin: 30px auto;
            background-color: #ccc;
        }
    </style>
</head>
<body>
    <header>
        <h1>Luminária Med-In</h1>
        <nav>
            <button onclick="showSection('bpmSection')">BPM</button>
            <button onclick="showSection('historiaSection')">História</button>
        </nav>
    </header>
    <main>
        <section id="bpmSection">
            <div class="canvas">
                <canvas id="ecgChart" width="600" height="300"></canvas>
            </div>
        </section>
        <script>
            document.addEventListener("DOMContentLoaded", () => {
                const ctx = document.getElementById('ecgChart').getContext('2d');
                const ecgChart = new Chart(ctx, {
                    type: 'line',
                    data: {
                        labels: Array.from({ length: 50 }, (_, i) => ''),
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
                            const bpmElement = document.getElementById('bpmValue');
                            if (bpmElement) bpmElement.textContent = data;
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
            });
        </script>
        <section id="historiaSection">
            <div class="hero">
                <h2>História da Luminária</h2>
            </div>
            <section class="section">
                <p>Inicialmente, ela foi desenvolvida pelo time do med-in em 2024, como um projeto para o Inteli Day. É um projeto que une tecnologia, estética e biomedicina ao mostrar como um batimento cardíaco pode se transformar em informações para um led! Em 2025, ela foi renovada por duas novas integrantes do time med-in, sendo aprimorada em diversos aspectos. Hoje em dia, a luminária representa criatividade, aprendizado e inovação para os desenvolvedores.</p>
            </section>
            <div class="media">
                esse espaço vai conter um vídeo da luminária
                <br><br>
                <button onclick="alert('Aqui irá um vídeo!')">▶</button>
            </div>
            <div class="grid">
                <div>
                    <div class="card"></div>
                    <div class="caption">Inteli day (2024)</div>
                </div>
                <div>
                    <div class="card"></div>
                    <div class="caption">Inteli day (2024)</div>
                </div>
                <div>
                    <div class="card"></div>
                    <div class="caption">Refatoração (2025)</div>
                </div>
                <div>
                    <div class="card"></div>
                    <div class="caption">Refatoração (2025)</div>
                </div>
            </div>
            <hr>
            <section class="collab">
                <h2>Colaboradores</h2>
                <div class="collab-group">
                    <h3>Primeira versão da luminária (2024) e Refatoração da Luminária (2025)</h3>
                    <div class="avatars">
                        <div class="avatar">
                            <div></div>
                            <span>Kethlen Martins</span>
                            <span>(2024)</span>
                        </div>
                        <div class="avatar">
                            <div></div>
                            <span>Caio Santos</span>
                            <span>(2024)</span>
                        </div>
                        <div class="avatar">
                            <div></div>
                            <span>Will</span>
                            <span>(2024)</span>
                        </div>
                        <div class="avatar">
                            <div></div>
                            <span>Lívia Tavares</span>
                            <span>(2025)</span>
                        </div>
                        <div class="avatar">
                            <div></div>
                            <span>Nicolli Venino</span>
                            <span>(2025)</span>
                        </div>
                    </div>
                    <hr>
                <section class="repo">
                <p><strong>Repositório do Projeto:</strong></p>
                <a href="https://github.com/MedIn-Inteli/luminaria_coracao.git" target="_blank">https://github.com/MedIn-Inteli/luminaria_coracao.git</a>
            </section>
                </div>
            </section>
        </section>
    </main>
    <footer></footer>
    <script>
        function showSection(sectionId) {
            document.getElementById('bpmSection').style.display = sectionId === 'bpmSection' ? 'block' : 'none';
            document.getElementById('historiaSection').style.display = sectionId === 'historiaSection' ? 'block' : 'none';
        }
        showSection('bpmSection');
        const ctx = document.getElementById('ecgChart').getContext('2d');
        const ecgChart = new Chart(ctx, {
            type: 'line',
            data: {
                labels: Array.from({length: 50}, () => ''),
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
        function atualizarGrafico() {
            fetch('/bpmdata')
                .then(res => res.json())
                .then(data => {
                    ecgChart.data.datasets[0].data = data;
                    ecgChart.update();
                });
        }
        setInterval(() => {
            atualizarGrafico();
        }, 1000);
    </script>
</body>
</html>
)rawliteral";
}
