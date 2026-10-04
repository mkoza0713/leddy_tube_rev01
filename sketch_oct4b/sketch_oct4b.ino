#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include <time.h>

#define RELAY_PIN 17

const unsigned long WIFI_CONNECT_TIMEOUT_MS = 15000;
const byte DNS_PORT = 53;
const char *CONFIG_AP_PASSWORD = "AkwariumSetup";

WebServer webServer(80);
DNSServer dnsServer;
Preferences preferences;

bool apMode = false;
bool timeConfigured = false;

String savedSsid;
String savedPassword;

// 0=OFF, 1=RANO, 2=DZIEN, 3=WIECZOR, 4=NOC
const char *MODE_NAMES[] = {
  "OFF", "RANO", "DZIEN", "WIECZOR", "NOC"
};

// Domyslne godziny
int scheduleMinutes[5] = {
  23 * 60, // OFF
  8 * 60,  // RANO
  10 * 60, // DZIEN
  18 * 60, // WIECZOR
  21 * 60  // NOC
};

bool automaticMode = true;

byte desiredLightMode = 0;
byte activeLightMode = 255;


// ============================================================
// DEKLARACJE
// ============================================================

void loadSettings();
void saveSchedule();
bool connectSavedWiFi();
void startConfigAccessPoint();
void startHttpServer();
void configureTimeFromNtp();
void maintainWiFi();

void application();

void requestLightMode(byte mode);
void updateLightController();


// ============================================================
// SETUP
// ============================================================

void setup() {

  Serial.begin(115200);
  delay(300);

  pinMode(RELAY_PIN, OUTPUT);

  // Bezpieczny stan po starcie
  digitalWrite(RELAY_PIN, LOW);

  preferences.begin("aquarium", false);

  loadSettings();

  // Jezeli mamy zapisane WiFi, probujemy sie polaczyc
  if (!savedSsid.isEmpty() && connectSavedWiFi()) {

    apMode = false;

    configureTimeFromNtp();

    // Dzieki temu mozna wejsc:
    // http://akwarium.local/
    if (MDNS.begin("akwarium")) {
      Serial.println("mDNS uruchomione");
      Serial.println("Panel: http://akwarium.local/");
    }

  } else {

    // Brak WiFi albo nie udalo sie polaczyc
    startConfigAccessPoint();
  }

  startHttpServer();
}


// ============================================================
// LOOP
// ============================================================

void loop() {

  if (apMode) {
    dnsServer.processNextRequest();
  }

  webServer.handleClient();

  updateLightController();

  maintainWiFi();

  if (!apMode) {
    application();
  }

  delay(2);
}


// ============================================================
// PAMIEC USTAWIEN
// ============================================================

void loadSettings() {

  savedSsid =
      preferences.getString("ssid", "");

  savedPassword =
      preferences.getString("pass", "");

  automaticMode =
      preferences.getBool("auto", true);

  scheduleMinutes[0] =
      preferences.getInt("t_off", 23 * 60);

  scheduleMinutes[1] =
      preferences.getInt("t_morn", 8 * 60);

  scheduleMinutes[2] =
      preferences.getInt("t_day", 10 * 60);

  scheduleMinutes[3] =
      preferences.getInt("t_even", 18 * 60);

  scheduleMinutes[4] =
      preferences.getInt("t_night", 21 * 60);
}


void saveSchedule() {

  preferences.putBool(
      "auto",
      automaticMode
  );

  preferences.putInt(
      "t_off",
      scheduleMinutes[0]
  );

  preferences.putInt(
      "t_morn",
      scheduleMinutes[1]
  );

  preferences.putInt(
      "t_day",
      scheduleMinutes[2]
  );

  preferences.putInt(
      "t_even",
      scheduleMinutes[3]
  );

  preferences.putInt(
      "t_night",
      scheduleMinutes[4]
  );
}


// ============================================================
// POLACZENIE Z WIFI
// ============================================================

bool connectSavedWiFi() {

  WiFi.mode(WIFI_STA);

  WiFi.setAutoReconnect(true);

  WiFi.persistent(false);

  WiFi.begin(
      savedSsid.c_str(),
      savedPassword.c_str()
  );

  Serial.print("Laczenie z WiFi: ");
  Serial.println(savedSsid);

  unsigned long started = millis();

  while (
      WiFi.status() != WL_CONNECTED &&
      millis() - started < WIFI_CONNECT_TIMEOUT_MS
  ) {

    delay(250);
    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {

    Serial.println("Polaczono z WiFi!");

    Serial.print("Adres IP: http://");
    Serial.println(WiFi.localIP());

    return true;
  }

  Serial.println(
      "Nie udalo sie polaczyc z zapisanym WiFi."
  );

  WiFi.disconnect(true, false);

  return false;
}


// ============================================================
// AWARYJNY ACCESS POINT
// ============================================================

void startConfigAccessPoint() {

  apMode = true;

  WiFi.mode(WIFI_AP_STA);

  IPAddress apIp(
      192,
      168,
      4,
      1
  );

  IPAddress gateway(
      192,
      168,
      4,
      1
  );

  IPAddress subnet(
      255,
      255,
      255,
      0
  );

  WiFi.softAPConfig(
      apIp,
      gateway,
      subnet
  );

  String apSsid =
      "Akwarium-192.168.4.1";

  WiFi.softAP(
      apSsid.c_str(),
      CONFIG_AP_PASSWORD
  );

  // Captive portal
  dnsServer.start(
      DNS_PORT,
      "*",
      apIp
  );

  Serial.println();
  Serial.println("========================");
  Serial.println("TRYB KONFIGURACJI WIFI");
  Serial.println("========================");

  Serial.print("SSID: ");
  Serial.println(apSsid);

  Serial.print("Haslo: ");
  Serial.println(CONFIG_AP_PASSWORD);

  Serial.println(
      "Adres: http://192.168.4.1/"
  );
}


// ============================================================
// CZAS NTP
// ============================================================

void configureTimeFromNtp() {

  // Polska
  // CET + automatyczna zmiana czasu letni/zimowy
  const char *tz =
      "CET-1CEST,M3.5.0,M10.5.0/3";

  configTzTime(
      tz,
      "pool.ntp.org",
      "time.google.com",
      "time.cloudflare.com"
  );

  timeConfigured = true;

  Serial.println(
      "Uruchomiono synchronizacje czasu NTP."
  );
}


// ============================================================
// KONTROLA UTRATY WIFI
// ============================================================

void maintainWiFi() {

  static unsigned long disconnectedSince = 0;

  if (apMode)
    return;

  if (WiFi.status() == WL_CONNECTED) {

    disconnectedSince = 0;

    return;
  }

  if (disconnectedSince == 0) {

    disconnectedSince = millis();

    return;
  }

  // Jezeli WiFi nie dziala przez 30 sekund,
  // uruchamiamy siec konfiguracyjna.
  if (
      millis() - disconnectedSince >= 30000
  ) {

    Serial.println(
        "Brak WiFi przez 30 sekund."
    );

    Serial.println(
        "Uruchamiam portal awaryjny."
    );

    startConfigAccessPoint();

    disconnectedSince = 0;
  }
}


// ============================================================
// CZAS / HARMONOGRAM
// ============================================================

int parseTimeToMinutes(
    const String &value
) {

  if (
      value.length() != 5 ||
      value.charAt(2) != ':'
  )
    return -1;

  int hour =
      value.substring(0, 2).toInt();

  int minute =
      value.substring(3, 5).toInt();

  if (
      hour < 0 ||
      hour > 23 ||
      minute < 0 ||
      minute > 59
  )
    return -1;

  return hour * 60 + minute;
}


String minutesToTime(
    int value
) {

  value =
      ((value % 1440) + 1440) % 1440;

  int hour =
      value / 60;

  int minute =
      value % 60;

  char buffer[6];

  snprintf(
      buffer,
      sizeof(buffer),
      "%02d:%02d",
      hour,
      minute
  );

  return String(buffer);
}


bool getCurrentLocalTime(
    struct tm &timeInfo
) {

  return getLocalTime(
      &timeInfo,
      10
  );
}


// ============================================================
// WYBOR TRYBU NA PODSTAWIE GODZINY
// ============================================================

byte scheduledModeForMinute(
    int nowMinutes
) {

  int bestAge = 1441;

  byte bestMode = 0;

  for (
      byte mode = 0;
      mode < 5;
      mode++
  ) {

    int age =
        nowMinutes -
        scheduleMinutes[mode];

    if (age < 0)
      age += 1440;

    if (age < bestAge) {

      bestAge = age;

      bestMode = mode;
    }
  }

  return bestMode;
}


// ============================================================
// AUTOMATYCZNE STEROWANIE
// ============================================================

void application() {

  static unsigned long lastCheck = 0;

  if (
      millis() - lastCheck < 1000
  )
    return;

  lastCheck = millis();

  if (!automaticMode)
    return;

  struct tm timeInfo;

  if (!getCurrentLocalTime(timeInfo)) {

    // Jeszcze nie pobrano czasu
    return;
  }

  int nowMinutes =
      timeInfo.tm_hour * 60 +
      timeInfo.tm_min;

  byte mode =
      scheduledModeForMinute(
          nowMinutes
      );

  if (
      mode != desiredLightMode ||
      activeLightMode == 255
  ) {

    requestLightMode(mode);
  }
}


// ============================================================
// HTML
// ============================================================

String htmlHeader(
    const String &title
) {

  String html =
      F("<!doctype html>"
        "<html lang='pl'>"
        "<head>"
        "<meta charset='utf-8'>");

  html +=
      F("<meta name='viewport' "
        "content='width=device-width,"
        "initial-scale=1'>");

  html +=
      "<title>" +
      title +
      "</title>";

  html += F(
      "<style>"

      "body{"
      "font-family:system-ui,Arial,sans-serif;"
      "background:#eef6f7;"
      "color:#17313a;"
      "margin:0;"
      "padding:18px;"
      "}"

      ".card{"
      "max-width:760px;"
      "margin:20px auto;"
      "background:#fff;"
      "border-radius:18px;"
      "padding:22px;"
      "box-shadow:0 8px 28px #0002;"
      "}"

      "h1{margin-top:0;}"

      "label{"
      "display:block;"
      "margin:13px 0 5px;"
      "font-weight:650;"
      "}"

      "input,select,button{"
      "font:inherit;"
      "}"

      "input,select{"
      "width:100%;"
      "box-sizing:border-box;"
      "padding:11px;"
      "border:1px solid #b8c9ce;"
      "border-radius:10px;"
      "}"

      "button,.btn{"
      "display:inline-block;"
      "margin-top:16px;"
      "padding:11px 16px;"
      "border:0;"
      "border-radius:10px;"
      "background:#176b78;"
      "color:white;"
      "text-decoration:none;"
      "cursor:pointer;"
      "}"

      ".danger{"
      "background:#a93434;"
      "}"

      ".muted{"
      "color:#60767d;"
      "}"

      ".grid{"
      "display:grid;"
      "grid-template-columns:"
      "repeat(auto-fit,minmax(140px,1fr));"
      "gap:12px;"
      "}"

      ".pill{"
      "display:inline-block;"
      "padding:5px 9px;"
      "border-radius:999px;"
      "background:#e2f0f2;"
      "margin:3px;"
      "}"

      "</style>"
      "</head>"
      "<body>"
      "<div class='card'>"
  );

  return html;
}


String htmlFooter() {

  return F(
      "</div>"
      "</body>"
      "</html>"
  );
}


// ============================================================
// SKANOWANIE WIFI
// ============================================================

String scanNetworksOptions() {

  String options;

  int count =
      WiFi.scanNetworks(
          false,
          true
      );

  if (count <= 0) {

    return F(
        "<option value=''>"
        "Nie znaleziono sieci"
        "</option>"
    );
  }

  for (
      int i = 0;
      i < count;
      i++
  ) {

    String name =
        WiFi.SSID(i);

    name.replace(
        "&",
        "&amp;"
    );

    name.replace(
        "<",
        "&lt;"
    );

    name.replace(
        ">",
        "&gt;"
    );

    options +=
        "<option value='" +
        name +
        "'>" +
        name +
        " (" +
        String(WiFi.RSSI(i)) +
        " dBm)"
        "</option>";
  }

  WiFi.scanDelete();

  return options;
}


// ============================================================
// STRONA KONFIGURACJI WIFI
// ============================================================

void sendWifiSetupPage() {

  String html =
      htmlHeader(
          "Akwarium - WiFi"
      );

  html +=
      F("<h1>Konfiguracja Wi-Fi</h1>");

  html +=
      F("<p>"
        "Wybierz domowa siec Wi-Fi "
        "i wpisz haslo."
        "</p>");

  html +=
      F("<form method='post' "
        "action='/wifi'>");

  html +=
      F("<label>"
        "Wykryte sieci"
        "</label>");

  html +=
      F("<select id='networks' "
        "onchange=\""
        "document.getElementById('ssid').value="
        "this.value\">");

  html +=
      scanNetworksOptions();

  html +=
      F("</select>");

  html +=
      F("<label>SSID</label>");

  html +=
      F("<input "
        "id='ssid' "
        "name='ssid' "
        "maxlength='32' "
        "required>");

  html +=
      F("<label>Haslo</label>");

  html +=
      F("<input "
        "name='password' "
        "type='password' "
        "maxlength='64'>");

  html +=
      F("<button type='submit'>"
        "Zapisz i polacz"
        "</button>"
        "</form>");

  html +=
      F("<p class='muted'>"
        "Adres: http://192.168.4.1/"
        "<br>"
        "Haslo AP: "
        "<strong>AkwariumSetup</strong>"
        "</p>");

  html +=
      htmlFooter();

  webServer.send(
      200,
      "text/html; charset=utf-8",
      html
  );
}


// ============================================================
// AKTUALNY CZAS
// ============================================================

String currentTimeText() {

  struct tm timeInfo;

  if (!getCurrentLocalTime(timeInfo)) {

    return "oczekiwanie na NTP";
  }

  char buffer[32];

  strftime(
      buffer,
      sizeof(buffer),
      "%Y-%m-%d %H:%M:%S",
      &timeInfo
  );

  return String(buffer);
}


// ============================================================
// GLOWNA STRONA
// ============================================================

void sendMainPage() {

  String html =
      htmlHeader(
          "Akwarium ESP32"
      );

  html +=
      F("<h1>Akwarium ESP32</h1>");

  html +=
      "<p>"
      "<span class='pill'>Wi-Fi: " +
      WiFi.SSID() +
      "</span>";

  html +=
      "<span class='pill'>IP: " +
      WiFi.localIP().toString() +
      "</span>";

  html +=
      "<span class='pill'>Czas: " +
      currentTimeText() +
      "</span>"
      "</p>";


  // ==========================
  // HARMONOGRAM
  // ==========================

  html +=
      F("<h2>Harmonogram</h2>");

  html +=
      F("<p class='muted'>"
        "Ustaw godziny zmiany "
        "trybu lampy."
        "</p>");

  html +=
      F("<form method='post' "
        "action='/schedule'>");

  html +=
      F("<div class='grid'>");

  const char *labels[] = {
    "OFF",
    "Rano",
    "Dzien",
    "Wieczor",
    "Noc"
  };

  const char *fields[] = {
    "off",
    "morning",
    "day",
    "evening",
    "night"
  };

  for (
      byte i = 0;
      i < 5;
      i++
  ) {

    html +=
        "<div>"
        "<label>" +
        String(labels[i]) +
        "</label>";

    html +=
        "<input "
        "type='time' "
        "name='" +
        String(fields[i]) +
        "' value='" +
        minutesToTime(
            scheduleMinutes[i]
        ) +
        "' required>";

    html +=
        "</div>";
  }

  html +=
      F("</div>");

  html +=
      F("<button type='submit'>"
        "Zapisz harmonogram"
        "</button>");

  html +=
      F("</form>");


  // ==========================
  // STEROWANIE RECZNE
  // ==========================

  html +=
      F("<h2>Sterowanie</h2>");

  html +=
      "<p>Praca: <strong>" +
      String(
          automaticMode
              ? "AUTO"
              : "RECZNA"
      ) +
      "</strong><br>";

  if (activeLightMode <= 4) {

    html +=
        "Aktualny tryb: <strong>" +
        String(
            MODE_NAMES[
                activeLightMode
            ]
        ) +
        "</strong>";

  } else {

    html +=
        "Trwa zmiana trybu lampy...";
  }

  html += "</p>";

  html +=
      F("<form method='post' "
        "action='/mode'>");

  html +=
      F("<label>Tryb</label>");

  html +=
      F("<select name='mode'>");

  html +=
      F("<option value='auto'>"
        "AUTO - harmonogram"
        "</option>");

  for (
      byte i = 0;
      i < 5;
      i++
  ) {

    html +=
        "<option value='" +
        String(i) +
        "'>"
        "Recznie: " +
        String(MODE_NAMES[i]) +
        "</option>";
  }

  html +=
      F("</select>");

  html +=
      F("<button type='submit'>"
        "Ustaw tryb"
        "</button>");

  html +=
      F("</form>");


  // ==========================
  // WIFI
  // ==========================

  html +=
      F("<h2>Siec</h2>");

  html +=
      F("<p>"
        "Panel lokalny:<br>"
        "<strong>"
        "http://akwarium.local/"
        "</strong>"
        "</p>");

  html +=
      F("<form "
        "method='post' "
        "action='/forget' "
        "onsubmit=\""
        "return confirm("
        "'Usunac zapisane WiFi?'"
        ")\""
        ">");

  html +=
      F("<button "
        "class='danger' "
        "type='submit'>"
        "Zmien / zapomnij Wi-Fi"
        "</button>");

  html +=
      F("</form>");

  html +=
      htmlFooter();

  webServer.send(
      200,
      "text/html; charset=utf-8",
      html
  );
}


// ============================================================
// ROOT
// ============================================================

void handleRoot() {

  if (apMode) {

    sendWifiSetupPage();

  } else {

    sendMainPage();
  }
}


// ============================================================
// ZAPIS WIFI
// ============================================================

void handleWifiSave() {

  String ssid =
      webServer.arg("ssid");

  String password =
      webServer.arg("password");

  ssid.trim();

  if (ssid.isEmpty()) {

    webServer.send(
        400,
        "text/plain; charset=utf-8",
        "SSID nie moze byc puste."
    );

    return;
  }

  preferences.putString(
      "ssid",
      ssid
  );

  preferences.putString(
      "pass",
      password
  );

  String html =
      htmlHeader(
          "Zapisano WiFi"
      );

  html +=
      F("<h1>Zapisano</h1>");

  html +=
      F("<p>"
        "ESP32 uruchomi sie ponownie "
        "i polaczy z domowa siecia."
        "</p>");

  html +=
      F("<p>"
        "Pozniej wejdz na:<br>"
        "<strong>"
        "http://akwarium.local/"
        "</strong>"
        "</p>");

  html +=
      htmlFooter();

  webServer.send(
      200,
      "text/html; charset=utf-8",
      html
  );

  delay(1000);

  ESP.restart();
}


// ============================================================
// ZAPIS HARMONOGRAMU
// ============================================================

void handleScheduleSave() {

  const char *fields[] = {
    "off",
    "morning",
    "day",
    "evening",
    "night"
  };

  int newTimes[5];

  for (
      byte i = 0;
      i < 5;
      i++
  ) {

    newTimes[i] =
        parseTimeToMinutes(
            webServer.arg(
                fields[i]
            )
        );

    if (newTimes[i] < 0) {

      webServer.send(
          400,
          "text/plain",
          "Nieprawidlowa godzina."
      );

      return;
    }
  }

  for (
      byte i = 0;
      i < 5;
      i++
  ) {

    scheduleMinutes[i] =
        newTimes[i];
  }

  automaticMode = true;

  saveSchedule();

  webServer.sendHeader(
      "Location",
      "/",
      true
  );

  webServer.send(
      303,
      "text/plain",
      ""
  );
}


// ============================================================
// TRYB RECZNY / AUTO
// ============================================================

void handleModeSet() {

  String value =
      webServer.arg("mode");

  if (value == "auto") {

    automaticMode = true;

    preferences.putBool(
        "auto",
        true
    );

  } else {

    int mode =
        value.toInt();

    if (
        mode < 0 ||
        mode > 4
    ) {

      webServer.send(
          400,
          "text/plain",
          "Nieprawidlowy tryb."
      );

      return;
    }

    automaticMode = false;

    preferences.putBool(
        "auto",
        false
    );

    requestLightMode(
        (byte)mode
    );
  }

  webServer.sendHeader(
      "Location",
      "/",
      true
  );

  webServer.send(
      303,
      "text/plain",
      ""
  );
}


// ============================================================
// USUWANIE WIFI
// ============================================================

void handleForgetWifi() {

  preferences.remove("ssid");
  preferences.remove("pass");

  String html =
      htmlHeader(
          "WiFi usuniete"
      );

  html +=
      F("<h1>WiFi usuniete</h1>"
        "<p>Restart...</p>");

  html +=
      htmlFooter();

  webServer.send(
      200,
      "text/html; charset=utf-8",
      html
  );

  delay(800);

  ESP.restart();
}


// ============================================================
// CAPTIVE PORTAL
// ============================================================

void captiveRedirect() {

  if (apMode) {

    webServer.sendHeader(
        "Location",
        "http://192.168.4.1/",
        true
    );

    webServer.send(
        302,
        "text/plain",
        ""
    );

  } else {

    webServer.send(
        404,
        "text/plain",
        "Not found"
    );
  }
}


// ============================================================
// SERWER WWW
// ============================================================

void startHttpServer() {

  webServer.on(
      "/",
      HTTP_GET,
      handleRoot
  );

  webServer.on(
      "/wifi",
      HTTP_POST,
      handleWifiSave
  );

  webServer.on(
      "/schedule",
      HTTP_POST,
      handleScheduleSave
  );

  webServer.on(
      "/mode",
      HTTP_POST,
      handleModeSet
  );

  webServer.on(
      "/forget",
      HTTP_POST,
      handleForgetWifi
  );


  // Captive portal Android
  webServer.on(
      "/generate_204",
      HTTP_ANY,
      captiveRedirect
  );

  // Apple
  webServer.on(
      "/hotspot-detect.html",
      HTTP_ANY,
      captiveRedirect
  );

  // Windows
  webServer.on(
      "/ncsi.txt",
      HTTP_ANY,
      captiveRedirect
  );

  webServer.on(
      "/connecttest.txt",
      HTTP_ANY,
      captiveRedirect
  );

  webServer.onNotFound(
      captiveRedirect
  );

  webServer.begin();

  Serial.println(
      "Serwer HTTP uruchomiony."
  );
}


// ============================================================
// STEROWANIE LAMPA
// ============================================================
//
// Tutaj zachowujemy Twoj sposob sterowania:
// lampa zmienia tryb przez chwilowe odciecie
// i ponowne podanie zasilania.
//
// Sekwencja jest NIEBLOKUJACA.
// ESP32 podczas zmiany trybu nadal obsluguje WWW.
//
// ============================================================

struct RelayStep {

  bool level;

  unsigned long durationMs;
};


RelayStep relaySequence[7];

byte relaySequenceLength = 0;

byte relaySequenceIndex = 0;

unsigned long relayStepStarted = 0;

bool relaySequenceRunning = false;

byte sequenceTargetMode = 0;


// ============================================================
// DODAWANIE KROKU
// ============================================================

void addRelayStep(
    bool level,
    unsigned long durationMs
) {

  if (
      relaySequenceLength >= 7
  )
    return;

  relaySequence[
      relaySequenceLength
  ].level = level;

  relaySequence[
      relaySequenceLength
  ].durationMs = durationMs;

  relaySequenceLength++;
}


// ============================================================
// ZMIANA TRYBU LAMPY
// ============================================================

void requestLightMode(
    byte mode
) {

  if (mode > 4)
    return;


  // Jezeli juz jest ten tryb
  if (
      activeLightMode == mode &&
      !relaySequenceRunning
  ) {

    desiredLightMode = mode;

    return;
  }


  // Jezeli juz trwa ustawianie tego trybu
  if (
      relaySequenceRunning &&
      sequenceTargetMode == mode
  ) {

    desiredLightMode = mode;

    return;
  }


  desiredLightMode =
      mode;

  sequenceTargetMode =
      mode;

  relaySequenceLength =
      0;

  relaySequenceIndex =
      0;


  // Najpierw odcinamy zasilanie
  // na 20 sekund
  addRelayStep(
      LOW,
      20000
  );


  switch (mode) {

    // ======================
    // OFF
    // ======================

    case 0:

      addRelayStep(
          LOW,
          0
      );

      break;


    // ======================
    // RANO
    // ======================

    case 1:

      addRelayStep(
          HIGH,
          1000
      );

      addRelayStep(
          LOW,
          1000
      );

      addRelayStep(
          HIGH,
          0
      );

      break;


    // ======================
    // DZIEN
    // ======================

    case 2:

      addRelayStep(
          HIGH,
          0
      );

      break;


    // ======================
    // WIECZOR
    // ======================

    case 3:

      // W Twoim starym kodzie
      // sekwencja byla taka sama
      // jak dla RANO.

      addRelayStep(
          HIGH,
          1000
      );

      addRelayStep(
          LOW,
          1000
      );

      addRelayStep(
          HIGH,
          0
      );

      break;


    // ======================
    // NOC
    // ======================

    case 4:

      addRelayStep(
          HIGH,
          1000
      );

      addRelayStep(
          LOW,
          1000
      );

      addRelayStep(
          HIGH,
          1000
      );

      addRelayStep(
          LOW,
          1000
      );

      addRelayStep(
          HIGH,
          0
      );

      break;
  }


  activeLightMode =
      255;

  relaySequenceRunning =
      true;

  relayStepStarted =
      millis();


  digitalWrite(
      RELAY_PIN,
      relaySequence[0].level
  );


  Serial.print(
      "Zmiana trybu lampy -> "
  );

  Serial.println(
      MODE_NAMES[mode]
  );
}


// ============================================================
// OBSLUGA SEKWENCJI PRZEKAZNIKA
// ============================================================

void updateLightController() {

  if (!relaySequenceRunning)
    return;


  RelayStep &step =
      relaySequence[
          relaySequenceIndex
      ];


  if (
      step.durationMs > 0 &&
      millis() - relayStepStarted <
          step.durationMs
  ) {

    return;
  }


  relaySequenceIndex++;


  // Koniec sekwencji
  if (
      relaySequenceIndex >=
      relaySequenceLength
  ) {

    relaySequenceRunning =
        false;

    activeLightMode =
        sequenceTargetMode;


    Serial.print(
        "Tryb lampy ustawiony: "
    );

    Serial.println(
        MODE_NAMES[
            activeLightMode
        ]
    );

    return;
  }


  digitalWrite(
      RELAY_PIN,
      relaySequence[
          relaySequenceIndex
      ].level
  );


  relayStepStarted =
      millis();
}