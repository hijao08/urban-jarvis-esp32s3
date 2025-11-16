#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <TFT_Touch.h>
#include <Adafruit_NeoPixel.h>
#include <time.h>

#define BTN_Y 180
#define BTN_H 70
#define PIN_RGB 48
#define NUM_PIXELS 1
Adafruit_NeoPixel rgbLed(NUM_PIXELS, PIN_RGB, NEO_GRB + NEO_KHZ800);
#define DOUT 41
#define DIN   2
#define DCS   1
#define DCLK 42
TFT_Touch touch(DCS, DCLK, DIN, DOUT);
TFT_eSPI tft;

const char* WIFI_SSID = "Memes1337";
const char* WIFI_PASS = "hxqg-sdpa-f5g1";
const char* URL_URBS =
  "https://candis-unscrutinised-dioicously.ngrok-free.dev/api/urbs/horarios-linha/db/dia-ponto?dia=1&ponto=TERMINAL%20CENTENARIO";

bool ledState = false;

// ----- Árvore Binária -----
struct HorarioNode {
  String hora, ponto;
  HorarioNode *left, *right;
  HorarioNode(String h, String p) : hora(h), ponto(p), left(NULL), right(NULL) {}
};
HorarioNode* root = NULL;

// --- WiFi/Web ---
WebServer server(80);

// --- Utilidades Hora/Árvore ---
int horaStrToMin(const String& hora) {
  int h = hora.substring(0,2).toInt();
  int m = hora.substring(3,5).toInt();
  return h*60 + m;
}
HorarioNode* insertHorario(HorarioNode* node, String h, String p) {
  if (!node) return new HorarioNode(h, p);
  if (horaStrToMin(h) < horaStrToMin(node->hora)) node->left = insertHorario(node->left, h, p);
  else node->right = insertHorario(node->right, h, p);
  return node;
}
void printInOrderWeb(HorarioNode* node, String &html) {
  if (!node) return;
  printInOrderWeb(node->left, html);
  html += "<li><b>"+node->hora+"</b> - "+node->ponto+"</li>\n";
  printInOrderWeb(node->right, html);
}
// Busca próximo horário >= atual
HorarioNode* buscarProximo(HorarioNode* node, String atual) {
  HorarioNode* succ = NULL; int minAtual = horaStrToMin(atual);
  while (node) {
    int minNode = horaStrToMin(node->hora);
    if (minNode >= minAtual) { succ = node; node = node->left; }
    else node = node->right;
  }
  return succ;
}
// Libera toda a árvore
void liberaArvore(HorarioNode* node) {
  if (!node) return;
  liberaArvore(node->left);
  liberaArvore(node->right);
  delete node;
}

// --------- Funções LED/TFT ---------
void ligaLed() { rgbLed.setPixelColor(0, rgbLed.Color(255, 0, 0)); rgbLed.show(); }
void desligaLed() { rgbLed.setPixelColor(0, 0); rgbLed.show(); }
void desenhaBotao() {
  tft.fillRect(40, BTN_Y, 160, BTN_H, ledState ? TFT_GREEN : TFT_RED);
  tft.drawRect(40, BTN_Y, 160, BTN_H, TFT_WHITE);
  tft.setTextColor(TFT_WHITE, ledState ? TFT_GREEN : TFT_RED);
  tft.setTextFont(2); tft.setCursor(70, BTN_Y + 25);
  tft.println(ledState ? "Desligar LED" : "Ligar LED");
}

// --------- Busca Horários, monta BST ---------
bool pegaHorarios() {
  if (root != NULL) {
    liberaArvore(root);
    root = NULL;
  }
  WiFiClientSecure client; client.setInsecure();
  HTTPClient https;
  if (!https.begin(client, URL_URBS)) return false;
  https.addHeader("Accept-Encoding", "identity");
  https.addHeader("Connection", "close"); https.setTimeout(15000);
  int code = https.GET();
  Serial.printf("HTTP code: %d\n", code);
  if (code != HTTP_CODE_OK) { https.end(); return false; }
  String payload = https.getString(); https.end();

  DynamicJsonDocument doc(65536); // Aumentado para 64KB
  DeserializationError err = deserializeJson(doc, payload);
  if (err) { Serial.print("Erro JSON array: "); Serial.println(err.f_str()); return false; }
  
  // Filtra apenas horários futuros antes de inserir na árvore
  String hAtual = horaAtualStr();
  int minAtual = horaStrToMin(hAtual);
  JsonArray arr = doc.as<JsonArray>();
  int inseridos = 0;
  
  for (JsonObject obj : arr) {
    String hora  = obj["hora"]  | "";
    String ponto = obj["ponto"] | "";
    if (hora.length() == 5 && hora.charAt(2) == ':') {
      int minHora = horaStrToMin(hora);
      // Insere se for futuro ou passou da meia-noite (horários > 00:00 e < 04:00 são considerados do próximo dia)
      if (minHora >= minAtual || minHora < 240) {
        root = insertHorario(root, hora, ponto);
        inseridos++;
      }
    }
  }
  Serial.printf("Inseridos %d de %d horários\n", inseridos, arr.size());
  return arr.size() > 0;
}

// --------- Horário Atual (NTP) ---------
void setupTime() { configTime(-3 * 3600, 0, "pool.ntp.org", "time.nist.gov"); while (time(nullptr) < 1650000000) delay(100); }
String horaAtualStr() {
  struct tm t; if (!getLocalTime(&t)) return "";
  char buf[6]; sprintf(buf, "%02d:%02d", t.tm_hour, t.tm_min); return String(buf);
}

// --------- TFT Exibe próximo horário ---------
void mostraProximoHorario() {
  String hAtual = horaAtualStr();
  HorarioNode* prox = buscarProximo(root, hAtual);

  tft.fillScreen(TFT_BLACK);

  // Título
  tft.setTextFont(2);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  String titulo = "URBAN JARVIS";
  int tw = tft.textWidth(titulo, 2);
  tft.setCursor((tft.width() - tw) / 2, 5);
  tft.print(titulo);

  // Horário Atual
  String txtAtual = "Atual: " + hAtual;
  tft.setTextFont(2);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tw = tft.textWidth(txtAtual, 2);
  tft.setCursor((tft.width() - tw) / 2, 40);
  tft.print(txtAtual);

  // Próximo horário destacado em caixa
  int cx = 18, cy = 70, cw = tft.width() - 36, ch = 60;
  tft.fillRoundRect(cx, cy, cw, ch, 12, TFT_BLUE);
  tft.drawRoundRect(cx, cy, cw, ch, 12, TFT_WHITE);

  tft.setTextColor(TFT_WHITE, TFT_BLUE);
  tft.setTextFont(1);
  tft.setCursor(cx + 10, cy + 10);
  tft.print("Proximo: ");

  if (prox) {
    tft.setTextFont(2);
    tft.setTextColor(TFT_GREEN, TFT_BLUE);
    tft.setCursor(cx + 140, cy + 7);
    tft.print(prox->hora);

    tft.setTextFont(1);
    tft.setTextColor(TFT_CYAN, TFT_BLUE);
    tft.setCursor(cx + 10, cy + 35);
    tft.print(prox->ponto);
  } else {
    tft.setTextFont(1);
    tft.setTextColor(TFT_RED, TFT_BLUE);
    tft.setCursor(cx + 10, cy + 35);
    tft.print("Nenhum horario a frente!");
  }

  // Botão LED centralizado
  int btnW = 160, btnH = BTN_H, btnX = (tft.width() - btnW) / 2, btnY = 145;
  int btnColor = ledState ? TFT_GREEN : TFT_RED;
  tft.fillRoundRect(btnX, btnY, btnW, btnH, 16, btnColor);
  tft.drawRoundRect(btnX, btnY, btnW, btnH, 16, TFT_WHITE);
  tft.setTextColor(TFT_WHITE, btnColor);
  tft.setTextFont(2);
  tw = tft.textWidth(ledState ? "Desligar LED" : "Ligar LED", 2);
  tft.setCursor(btnX + (btnW - tw) / 2, btnY + 21);
  tft.println(ledState ? "Desligar LED" : "Ligar LED");
}
// --------- WebServer Handlers ---------
void fillArrayHorario(HorarioNode* n, String &html, bool &first) {
  if (!n) return;
  fillArrayHorario(n->left, html, first);
  if (!first) html += ",";
  html += "{h:'" + n->hora + "', p:'" + n->ponto + "'}";
  first = false;
  fillArrayHorario(n->right, html, first);
}

void printHorarioArrayWeb(HorarioNode* node, String &html) {
  html += "<script>var slides=[";
  bool first = true;
  fillArrayHorario(node, html, first);
  html += "];</script>";
}
void contaBST(HorarioNode* n, int &total) {
  if (!n) return;
  total++;
  contaBST(n->left, total);
  contaBST(n->right, total);
}
void handleRoot() {
  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
  html += "<title>URBAN JARVIS</title>";
  html += "<style>";
  html += "body{background:#212e24;color:#fff;font-family:Arial,sans-serif;margin:0;padding:0;}";
  html += ".box{max-width:460px;margin:38px auto;padding:26px 29px;background:#206a35;border-radius:14px;box-shadow:0 4px 24px #18381db0;}";
  html += ".header{text-align:center;}";
  html += "h1{color:#ffd700;text-shadow:1px 2px 0 #48c7f0, 0 0 4px #fff;margin-top:6px;}";
  html += "h2{color:#fff;margin-bottom:18px;font-weight:600;}";
  html += ".carousel{position:relative;padding:12px 0;min-height:110px;}";
  html += ".slide{display:none;text-align:center;}";
  html += ".slide.active{display:block;animation:fade 0.7s;}";
  html += "@keyframes fade{from{opacity:0;}to{opacity:1;}}";
  html += ".hora{font-size:37px;font-weight:900;color:#ffd700;text-shadow:2px 3px 0 #212e24, 0 0 5px #fff;}";
  html += ".ponto{font-size:18px;font-weight:700;color:#f2f2f2;margin-top:10px;text-shadow:0 0 4px #000;}";
  html += ".prev,.next{cursor:pointer;position:absolute;top:50%;width:35px;height:35px;margin-top:-19px;background:#ffd700;border:none;border-radius:50%;color:#206a35;font-size:23px;line-height:35px;text-align:center;box-shadow:0 2px 6px #18381d;transition:background 0.2s;}";
  html += ".prev:hover,.next:hover{background:#48c7f0;color:#fff;}";
  html += ".prev{left:10px;}.next{right:10px;}";
  html += ".dots{display:flex;justify-content:center;margin-top:9px;gap:7px}";
  html += ".dot{height:10px;width:10px;background:#ffd70055;border-radius:50%;display:inline-block;transition:background 0.2s;}";
  html += ".dot.active{background:#48c7f0;}";
  html += "footer{margin-top:30px;padding-bottom:15px;text-align:center;color:#ffd700;font-size:1em;text-shadow:0 0 1px #48c7f0;}";
  html += "footer b{color: #48c7f0;}";
  html += "footer a{color:#48c7f0;text-decoration:none;font-weight:bold;}";
  html += "footer a:hover{color:#ffd700;text-decoration:underline;}";
  html += "</style></head><body>";

  html += "<div class='box'><div class='header'>";
  html += "<h1>URBAN JARVIS</h1>";

  // =========== LOG DO SISTEMA ===========
  String info = "";
  info += "<div style='background:#282e24;border-radius:8px;padding:14px;color:#ffd700;margin:18px 0;font-size:1em;box-shadow:0 2px 8px #18381db0;'>";
  info += "<h3 style='color:#48c7f0;margin-top:0;'>Log do Sistema</h3>";
  info += "<b>IP LOCAL:</b> " + WiFi.localIP().toString() + "<br>";
  info += "<b>WiFi SSID:</b> " + String(WIFI_SSID) + "<br>";
  info += "<b>Horários Carregados:</b> " + String(root ? "OK" : "Falha") + "<br>";
  int totalHorarios = 0;
  contaBST(root, totalHorarios);
  info += "<b>Total de horários:</b> " + String(totalHorarios) + "<br>";
  info += "<b>LED:</b> " + String(ledState ? "Ligado" : "Desligado") + "<br>";
  info += "<b>Tempo conectado:</b> " + String(millis()/1000) + " s<br>";
  info += "</div>";
  html += info;
  // =========== FIM DO LOG ===========

  html += "<div style='text-align:center;font-weight:bold;font-size:18px;color:#ffd700;margin-bottom:8px;text-shadow:0 0 3px #fff, 1px 2px #206a35;'>";
  html += "<span id='datahora'></span></div>";

  html += "<script>";
  html += "function updateDateHora(){";
  html += "var d=new Date();";
  html += "var str=d.toLocaleDateString()+' '+d.toLocaleTimeString();";
  html += "document.getElementById('datahora').textContent = str;";
  html += "}";
  html += "setInterval(updateDateHora,1000);updateDateHora();";
  html += "</script>";

  html += "<h2>Horários Carregados:</h2></div>";

  html += "<div class='carousel'><button class='prev' onclick='showSlide(currSlide-1)'>&#8592;</button>";
  html += "<div id='slides'></div><button class='next' onclick='showSlide(currSlide+1)'>&#8594;</button>";
  html += "<div class='dots' id='dots'></div></div>";

  printHorarioArrayWeb(root, html);

  html += R"rawliteral(
    <script>
      var currSlide = 0;
      function showSlide(n){
        var slides = document.getElementsByClassName('slide');
        var dots = document.getElementsByClassName('dot');
        if(slides.length==0) return;
        currSlide = (n+slides.length)%slides.length;
        for(var i=0;i<slides.length;i++){
          slides[i].classList.remove('active');
          dots[i].classList.remove('active');
        }
        slides[currSlide].classList.add('active');
        dots[currSlide].classList.add('active');
      }
      function loadSlides(){
        var el=document.getElementById('slides');
        var d=document.getElementById('dots');
        if(slides.length==0){
          el.innerHTML="<div class='slide active'><div class='hora'>--:--</div><div class='ponto'>Sem horários</div></div>";
          return;
        }
        el.innerHTML = '';
        d.innerHTML = '';
        for(var i=0; i<slides.length; i++){
          el.innerHTML += "<div class='slide"+(i==0?" active":"")+"'><div class='hora'>"+slides[i].h+"</div><div class='ponto'>"+slides[i].p+"</div></div>";
          d.innerHTML += "<span class='dot"+(i==0?" active":"")+"' onclick='showSlide("+i+")'></span>";
        }
        currSlide=0;
      }
      window.onload=loadSlides;
    </script>
  )rawliteral";

  html += "<footer>";
  html += "Projeto acadêmico desenvolvido por <br><b>Joao Soares, Matheus Leite, Leonardo Bora, Luan Contancio</b><br> ";
  html += "<a href='http://wa.me/41991036003' target='_blank'>Contato</a> | ";
  html += "<a href='https://github.com/mathelico' target='_blank'>GitHub</a>";
  html += "<br><span style='color:#48c7f0;'>Powered by ESP32 S3 - Urban Jarvis/Curitiba</span>";
  html += "</footer>";

  html += "</div></body></html>";
  server.send(200, "text/html", html);
}


// --------- SETUP e LOOP ---------
void setup() {
  Serial.begin(115200); rgbLed.begin(); rgbLed.show();
  tft.init(); tft.setRotation(1); tft.fillScreen(TFT_BLACK);
  touch.setCal(481, 3395, 755, 3487, 320, 240, 1); touch.setRotation(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK); tft.setTextSize(2); tft.setCursor(10, 10);
  tft.println("Conectando Wi-Fi...");

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) delay(250);
  if (WiFi.status() != WL_CONNECTED) {
    tft.fillScreen(TFT_RED); tft.setCursor(10, 50); tft.println("Wi-Fi falhou"); return;
  }
  tft.fillScreen(TFT_BLACK); tft.setCursor(10, 10); tft.println("Wi-Fi OK");
  Serial.print("IP LOCAL: "); Serial.println(WiFi.localIP());

  setupTime();
  pegaHorarios(); // monta BST

  // Inicia WebServer
  server.on("/", handleRoot);
  server.begin();
  Serial.println("Servidor web disponível!");

  mostraProximoHorario();
}

void loop() {
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 10000) {
    mostraProximoHorario();
    lastUpdate = millis();
  }
  if (touch.Pressed()) {
    int x = touch.X(); int y = touch.Y();
    if ((x > 40) && (x < 200) && (y > BTN_Y) && (y < BTN_Y + BTN_H)) {
      ledState = !ledState;
      if (ledState) ligaLed(); else desligaLed();
      desenhaBotao(); delay(250);
    }
  }
  server.handleClient();
}
