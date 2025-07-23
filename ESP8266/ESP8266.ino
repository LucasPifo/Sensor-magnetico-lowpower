#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>

// --- Pines ---
#define LED_BUILTIN_PIN 2   // D4 - LED interno (activo en LOW)
#define ESP_OFF 15          // D8 - Control de apagado del ESP
#define RESET 5             // D1 - Botón para borrar configuración WiFi
#define SENSOR 13           // D7 - Reed switch
#define BATERIA_PIN A0      // A0 - Pin lectura batería

// --- AP por defecto ---
const char* AP_SSID = "Sensor_magnetico_lowpower";
const char* AP_PASSWORD = "12345678";
const unsigned long AP_TIMEOUT_MS = 3 * 60 * 1000UL; // 3 minutos
unsigned long apStartTime = 0;

// --- Archivo de configuración WiFi ---
const char* WIFI_CONFIG_FILE = "/wifi.json";
const char* FINGERPRINT_FILE = "/fingerprint.json";

// --- Variables de estado ---
ESP8266WebServer server(80);
bool enModoAP = false;
unsigned long ultimoBlink = 0;
bool estadoLED = false;

// --- Estructura para almacenar configuración WiFi ---
struct WifiConfig {
  String ssid;
  String password;
  IPAddress ip;
  IPAddress gateway;
  IPAddress subnet;
  IPAddress dns1;
  IPAddress dns2;
  bool usarIPFija = false;
};


const char* HOST = ""; // ***** AGREGA TU DOMINIO o IP
const char* URL_SENSOR = "/api/sensorMagnetico"; // La ruta base de tu API sensor magnetico
const char* URL_FINGERPRINT = "/api/fingerprint"; // La ruta base de tu API huella
const char* apiKey = "api-key-secret"; // ***** (CAMBIA TU API KEY Y ACTUALIZA EN EL ENDPOINT)
const uint16_t PORTHTTPS = 4430;

// Combinando para las URLs completas
String API_URL_SENSOR = String("https://") + HOST + String(":") + PORTHTTPS + URL_SENSOR;
String API_URL_FINGERPRINT = String("https://") + HOST + String(":") + PORTHTTPS + URL_FINGERPRINT;

struct EventoSensor {
  bool estado;
  unsigned long tiempoRelativo; // en ms
};

const int MAX_EVENTOS = 20;
volatile EventoSensor eventos[MAX_EVENTOS];
volatile int indiceEvento = 0;
volatile unsigned long tiempoInicio = 0;
// Ultimo evento enviado realmente al servidor
int ultimoEventoEnviado = 0;
bool banderaEnvioPosterior = false;
String idEnvio = "";

// --- ISR para sensor con antirrebote simple (usa millis) ---
volatile unsigned long tiempoUltimoEventoMs = 0;
const uint16_t DEBOUNCE_MS = 200;

void IRAM_ATTR sensorCambio() {
  noInterrupts();
  unsigned long ahora = millis();
  bool estatusActual = !digitalRead(SENSOR);
  if ((ahora - tiempoUltimoEventoMs) < DEBOUNCE_MS) return;
  if (indiceEvento >= MAX_EVENTOS) return;
  if (estatusActual == eventos[indiceEvento-1].estado) return;

  eventos[indiceEvento].estado = estatusActual;
  eventos[indiceEvento].tiempoRelativo = ahora - tiempoUltimoEventoMs;
  tiempoUltimoEventoMs = ahora;
  indiceEvento++;
  interrupts();
}

// ----------------------------------------------------------------
// Inicializa LittleFS
// ----------------------------------------------------------------
void iniciarLittleFS() {
  if (!LittleFS.begin()) {
    Serial.println("❌ Error al montar LittleFS");
  } else {
    Serial.println("✅ LittleFS montado");
  }
}

// ----------------------------------------------------------------
// Verifica si se mantiene presionado el botón RESET
// ----------------------------------------------------------------
void verificarBotonReset() {
  pinMode(RESET, INPUT_PULLUP);

  if (digitalRead(RESET) == LOW) {
    Serial.println("⏳ Botón RESET presionado. Esperando 5 segundos...");

    unsigned long inicio = millis();
    while (millis() - inicio < 5000) {
      if (digitalRead(RESET) == HIGH) {
        Serial.println("❌ Se soltó el botón, cancelando borrado.");
        return;
      }
      delay(50);
    }

    Serial.println("✅ Borrando configuración WiFi...");
    if (LittleFS.exists(WIFI_CONFIG_FILE)) {
      LittleFS.remove(WIFI_CONFIG_FILE);
      Serial.println("🗑 Configuración eliminada.");
    } else {
      Serial.println("⚠️ No se encontró archivo de configuración.");
    }

    delay(1000);
    ESP.restart();
  }
}

// ----------------------------------------------------------------
// Obtiene configuración WiFi desde LittleFS
// ----------------------------------------------------------------
WifiConfig obtenerConfiguracionWiFi() {
  WifiConfig config;

  if (!LittleFS.exists(WIFI_CONFIG_FILE)) {
    Serial.println("⚠️ No hay configuración WiFi guardada.");
    return config;
  }

  File file = LittleFS.open(WIFI_CONFIG_FILE, "r");
  if (!file) {
    Serial.println("❌ No se pudo abrir el archivo de configuración.");
    return config;
  }

  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error) {
    Serial.print("❌ Error al leer JSON: ");
    Serial.println(error.c_str());
    return config;
  }

  config.ssid = doc["ssid"] | "";
  config.password = doc["password"] | "";

  String ipStr      = doc["ip"] | "";
  String gatewayStr = doc["gateway"] | "";
  String subnetStr  = doc["subnet"] | "";
  String dns1Str    = doc["dns1"] | "";
  String dns2Str    = doc["dns2"] | "";

  config.usarIPFija = config.ip.fromString(ipStr) &&
                      config.gateway.fromString(gatewayStr) &&
                      config.subnet.fromString(subnetStr);

  config.dns1.fromString(dns1Str);
  config.dns2.fromString(dns2Str);

  Serial.println("📡 Configuración WiFi cargada:");
  Serial.printf("SSID: %s\n", config.ssid.c_str());
  Serial.printf("Modo IP: %s\n", config.usarIPFija ? "Fija" : "DHCP");

  return config;
}

// ----------------------------------------------------------------
// Sirve la página HTML desde LittleFS
// ----------------------------------------------------------------
void servirPaginaWeb() {
  File file = LittleFS.open("/index.html", "r");
  if (!file) {
    server.send(500, "text/plain", "Error al abrir config.html");
    return;
  }

  server.streamFile(file, "text/html");
  file.close();
}

// ----------------------------------------------------------------
// Guarda configuración enviada por el formulario (AJAX)
// ----------------------------------------------------------------
void manejarFormularioGuardar() {
  if (server.method() != HTTP_POST) {
    server.send(405, "text/plain", "Método no permitido");
    return;
  }

  String body = server.arg("plain");
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, body);

  if (error) {
    server.send(400, "application/json", "{\"error\":\"JSON inválido\"}");
    return;
  }

  File file = LittleFS.open(WIFI_CONFIG_FILE, "w");
  if (!file) {
    server.send(500, "application/json", "{\"error\":\"No se pudo guardar\"}");
    return;
  }

  serializeJson(doc, file);
  file.close();

  server.send(200, "application/json", "{\"message\":\"Configuración guardada\"}");
  delay(1000);
  ESP.restart();
}

// ----------------------------------------------------------------
// Inicia modo configuración Access Point
// --------------------------------------------
void modoConfiguracionAP() {
  Serial.println("📶 Entrando en modo configuración (AP)");

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);

  IPAddress ip = WiFi.softAPIP();
  Serial.print("🔌 IP del AP: ");
  Serial.println(ip);

  server.on("/", HTTP_GET, servirPaginaWeb);
  server.on("/guardar-config", HTTP_POST, manejarFormularioGuardar);
  server.begin();

  Serial.println("🌐 Servidor HTTP iniciado");
  enModoAP = true;

  apStartTime = millis();  // Marca el inicio del timeout
}

// ----------------------------------------------------------------
// Conectarse a Wifi utilizando las credenciales de la memoria interna
// ----------------------------------------------------------------
bool conectarAWiFi(const WifiConfig& config, unsigned long timeoutMs = 15000) {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);  // Desconecta cualquier red anterior

  if (config.usarIPFija) {
    Serial.println("🌐 Conectando con IP fija...");
    if (!WiFi.config(config.ip, config.gateway, config.subnet, config.dns1, config.dns2)) {
      Serial.println("❌ Error configurando IP fija");
      return false;
    }
  } else {
    Serial.println("🌐 Conectando con DHCP...");
  }

  WiFi.begin(config.ssid.c_str(), config.password.c_str());
  Serial.printf("🔌 Intentando conectar a SSID: %s\n", config.ssid.c_str());

  unsigned long startAttemptTime = millis();

  while (WiFi.status() != WL_CONNECTED && (millis() - startAttemptTime) < timeoutMs) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ Conectado a WiFi!");
    Serial.print("📶 IP: ");
    Serial.println(WiFi.localIP());
    return true;
  } else {
    Serial.println("\n❌ No se pudo conectar a WiFi.");
    return false;
  }
}

// ----------------------------------------------------------------
// Función para leer porcentaje de batería
// ----------------------------------------------------------------
int leerBateria() {
  int lectura = analogRead(BATERIA_PIN);  // Lee el voltaje (0 - 1023)
  
  // Convertir la lectura del ADC al voltaje real de la batería (float)
  float voltaje = (lectura / 1023.0) * 4.2;  

  // Convertir voltaje a entero en centivoltios para map
  int voltajeCenti = (int)(voltaje * 100);

  // Convertir el voltaje a porcentaje (basado en el rango 3.0V - 4.2V)
  int porcentaje = map(voltajeCenti, 300, 420, 0, 100);

  // Limitar porcentaje a 0-100%
  if (porcentaje > 100) porcentaje = 100;
  if (porcentaje < 0) porcentaje = 0;

  return porcentaje;
}

// ----------------------------------------------------------------
// Función para generar el json del envio de datos
// ----------------------------------------------------------------
String generarPayload() {
  StaticJsonDocument<768> doc;
  JsonArray arr = doc.createNestedArray("eventos");

  int porcentaje = leerBateria();
  String mac = WiFi.macAddress();
  int tiempoEncendido = millis() / 1000;

  for (int i = ultimoEventoEnviado; i < indiceEvento; i++) {
    JsonObject evento = arr.createNestedObject();
    evento["mac"] = mac;
    evento["bateria"] = porcentaje;
    evento["estatus"] = eventos[i].estado;
    evento["tiempoEncendido"] = tiempoEncendido;
    evento["segundosExtra"] = eventos[i].tiempoRelativo / 1000; // en segundos
  }

  String payload;
  serializeJson(doc, payload);
  return payload;
}

// ----------------------------------------------------------------
// Envía los datos al servidor con JSON y API Key en headers
// ----------------------------------------------------------------
void enviarDatosAlServidor() {
  BearSSL::WiFiClientSecure client;
  HTTPClient https;

  String fingerprint = obtenerFingerprintGuardada();

  client.setFingerprint(fingerprint.c_str());

  if (!https.begin(client, API_URL_SENSOR)) {
    Serial.println("❌ Error iniciando conexión HTTPS");
    return;
  }

  https.addHeader("Content-Type", "application/json");
  https.addHeader("x-api-key", apiKey);  // Cambia si quieres

  String payload = generarPayload();
  // Marcar hasta dónde se enviaron eventos
  ultimoEventoEnviado = indiceEvento;

  Serial.println("📤 Enviando JSON:");
  Serial.println(payload);

  int httpCode = https.POST(payload);

  if (httpCode > 0) {
    Serial.printf("✅ Código HTTP: %d\n", httpCode);
    Serial.println("Respuesta: " + https.getString());

  } else {
    Serial.printf("❌ Error HTTP: %s\n", https.errorToString(httpCode).c_str());
  }

  https.end();
  client.stop();

  // Verifica despues del envio si hubo un cambio, por si se abrio o cerro exactamente cuando se quizo enviar la informacion
  verificarCambioPostEnvio();
}

// ----------------------------------------------------------------
// Obtiene la huella de la memoria interna
// ----------------------------------------------------------------
String obtenerFingerprintGuardada() {
  if (!LittleFS.exists(FINGERPRINT_FILE)) {
    Serial.println("❌ No hay huella guardada.");
    return "";
  }

  File file = LittleFS.open(FINGERPRINT_FILE, "r");
  if (!file) {
    Serial.println("❌ Error abriendo archivo de huella.");
    return "";
  }

  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error) {
    Serial.println("❌ Error deserializando huella.");
    return "";
  }

  String fp = doc["fingerprint"] | "";
  Serial.print("🔒 Huella cargada: ");
  Serial.println(fp);
  return fp;
}

// ----------------------------------------------------------------
// Guarda la huella obtenida en la memoria interna
// ----------------------------------------------------------------
void guardarFingerprint(String nuevaHuella) {
  File file = LittleFS.open(FINGERPRINT_FILE, "w");
  if (!file) {
    Serial.println("❌ Error al guardar huella.");
    return;
  }

  StaticJsonDocument<256> doc;
  doc["fingerprint"] = nuevaHuella;
  serializeJson(doc, file);
  file.close();
  Serial.println("✅ Huella actualizada y guardada.");
}

// ----------------------------------------------------------------
// Realiza una peticion al servidor para obtener la huella actualizada
// ----------------------------------------------------------------
String obtenerFingerprintDesdeServidor() {
  BearSSL::WiFiClientSecure client;
  client.setInsecure();  // Permitir conexión sin validar huella

  HTTPClient https;
  String nuevaHuella = "";

  if (!https.begin(client, API_URL_FINGERPRINT)) {
    Serial.println("❌ Error iniciando conexión al endpoint de la huella");
    return "";
  }

  Serial.println(API_URL_FINGERPRINT);
  int httpCode = https.GET();

  if (httpCode > 0) {
    StaticJsonDocument<512> doc;
    DeserializationError err = deserializeJson(doc, https.getString());

    if (!err && !doc["error"]) {
      nuevaHuella = doc["data"]["fingerprint"].as<String>();
      Serial.println("🆕 Nueva huella obtenida:");
      Serial.println(nuevaHuella);
    } else {
      Serial.println("❌ Error al parsear JSON de huella");
    }
  } else {
    Serial.printf("❌ Error HTTP: %s\n", https.errorToString(httpCode).c_str());
  }

  https.end();
  client.stop();

  return nuevaHuella;
}

// ----------------------------------------------------------------
// Funcion para validar la conexion HTTPS con la huella
// ----------------------------------------------------------------
void validarConexionHTTPS(){
  Serial.println("⚠️ Validando conexión al host...");
  Serial.println(HOST);

  String fingerprint = obtenerFingerprintGuardada();
  if(fingerprint == ""){
    fingerprint = obtenerFingerprintDesdeServidor();
    guardarFingerprint(fingerprint);
    return;
  }
  
  BearSSL::WiFiClientSecure client;
  client.setFingerprint(fingerprint.c_str());

  if (!client.connect(HOST, PORTHTTPS)) {
    Serial.println("❌ Falló conexión HTTPS con huella. Intentando obtener nueva...");

    fingerprint = obtenerFingerprintDesdeServidor();
    guardarFingerprint(fingerprint);
  }
  client.stop();
}

// ----------------------------------------------------------------
// Despues de enviar los datos se espera 2 segundos para saber si hubo un cambio mientras se envio la peticion
// ----------------------------------------------------------------
void verificarCambioPostEnvio() {
  Serial.println("⏳ Esperando 2 segundos para verificar nuevos eventos...");

  unsigned long marca = millis();
  while (millis() - marca < 2000) {
    yield();
  }

  if (indiceEvento > ultimoEventoEnviado) {
    banderaEnvioPosterior = true;
    Serial.printf("🆕 Se detectaron %d eventos nuevos después del envío.\n", indiceEvento - ultimoEventoEnviado);
    enviarDatosAlServidor(); // Reenviamos solo los nuevos
  } else {
    Serial.println("✅ No hubo eventos nuevos.");
  }
}

// ----------------------------------------------------------------
// SETUP
// ----------------------------------------------------------------
void setup() {
  Serial.begin(115200);

  // Inicializar pines
  pinMode(LED_BUILTIN_PIN, OUTPUT);
  digitalWrite(LED_BUILTIN_PIN, HIGH); // LED apagado (activo LOW)
  pinMode(ESP_OFF, OUTPUT);
  digitalWrite(ESP_OFF, HIGH);       // Mantener ESP encendido
  digitalWrite(LED_BUILTIN_PIN, LOW);  // LED encendido fijo

  pinMode(SENSOR, INPUT);
  pinMode(RESET, INPUT_PULLUP);

  tiempoInicio = millis();
  tiempoUltimoEventoMs = tiempoInicio;

  delay(200);
  eventos[indiceEvento].estado = !digitalRead(SENSOR);
  eventos[indiceEvento].tiempoRelativo = 0;
  indiceEvento++;

  attachInterrupt(digitalPinToInterrupt(SENSOR), sensorCambio, CHANGE);

  iniciarLittleFS();
  verificarBotonReset();

  WifiConfig config = obtenerConfiguracionWiFi();
  if (config.ssid == "") {
    modoConfiguracionAP();
  } else {
    if (!conectarAWiFi(config)) {
      Serial.println("⚠️ Falló conexión, entrando en modo AP...");
      modoConfiguracionAP();
    } else {
      enModoAP = false;

      validarConexionHTTPS();
      enviarDatosAlServidor();
      digitalWrite(ESP_OFF, LOW);           // Apagar alimentación del ESP

      Serial.println("⚠️ Apagando ESP...");
    }
  }
}

// ----------------------------------------------------------------
// LOOP
// ----------------------------------------------------------------
void loop() {
  server.handleClient();

  if (enModoAP) {
    unsigned long ahora = millis();

    // Blink LED cada 200ms
    if (ahora - ultimoBlink >= 200) {
      ultimoBlink = ahora;
      estadoLED = !estadoLED;
      digitalWrite(LED_BUILTIN_PIN, estadoLED ? LOW : HIGH);
    }

    // Verifica timeout para apagar el ESP
    if (ahora - apStartTime > AP_TIMEOUT_MS) {
      Serial.println("⏰ Timeout alcanzado. Apagando ESP...");

      digitalWrite(LED_BUILTIN_PIN, HIGH); // LED apagado
      digitalWrite(ESP_OFF, LOW);           // Apagar alimentación del ESP

      while (true) {
        delay(1000); // Espera indefinidamente (ESP apagado físicamente)
      }
    }
  }
}