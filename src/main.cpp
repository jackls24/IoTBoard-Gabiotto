#include <Arduino.h>
#include <iot_board.h>
#include <WiFi.h>
#include <Preferences.h>
#include "Crypto/CryptoUtils.h"
#include "LoRaMesh/LoRaMesh.h"
#include "LoRaMesh/state_t.h"
#include "BackendService.h"
#include "esp32-hal.h"
#include <iot_board.h>
#include <queue>
#include <WebServer.h>

using std::queue;

// Coda di messaggi
queue<LoRaMesh_message_t> coda;

// timer per la nuova fetch
unsigned long nextFetch = 0;
// intervallo per ogni fetch
int fetchInterval = 10 * 1000;
// lista di barche a cui bisogna cambiare lo stato
queue<barca> codaBarche;

// Dichiarazione oggetto Preferences
Preferences preferences;

// Dichiarazione del servizio backend
BackendService backendService;

// Configurazione WiFi
String ssid = "";
String password = "";

// Access Point
const char *apSSID = "ESP32-AP";
const char *apPassword = "123456789";

// Identificativo della barca/gabbiotto
char targa_gabbiotto[8] = "AB123XY";

// Funzione per gestire i messaggi ricevuti
void onReceive(LoRaMesh_message_t message);

// Funzione per creare e inviare messaggi di test
void inviaMessaggiTest()
{
    Serial.println("\n=== Invio messaggi di test al backend ===");

    LoRaMesh_message_t messageOrmeggiata;

    strncpy(messageOrmeggiata.targa_destinatario, "EM2023", 7);
    strncpy(messageOrmeggiata.targa_mittente, "EM2023", 7);

    messageOrmeggiata.message_id = 1001;
    messageOrmeggiata.payload.stato = st_ormeggio;
    /*messageOrmeggiata.payload.livello_batteria = 85;*/
    messageOrmeggiata.payload.pos_x = 43.7102;
    messageOrmeggiata.payload.pos_y = 10.4135;
    messageOrmeggiata.payload.direzione = 45.5;

    bool success;

    Serial.println("Invio messaggio di barca ormeggiata...");
    // success = backendService.sendMessageToBackend(messageOrmeggiata);
    Serial.println("Risultato invio: " + String(success ? "Successo" : "Fallimento"));

    /*
    Serial.println("Invio notifica cambio stato...");
    success = backendService.sendStateChangeNotification(messageOrmeggiata);
    Serial.println("Risultato invio notifica: " + String(success ? "Successo" : "Fallimento"));
     */

    Serial.println("=== Fine test invio messaggi ===\n");
}

WebServer server(80);

void handleRoot()
{
    String html;
    if (WiFi.status() == WL_CONNECTED)
    { // Connessione Wi-Fi attiva
        html = R"rawliteral(
       <html lang=en><meta charset=UTF-8><meta content="width=device-width,initial-scale=1"name=viewport><title>Boatguard</title><link crossorigin=anonymous href=https://cdn.jsdelivr.net/npm/bootstrap@5.3.5/dist/css/bootstrap.min.css integrity=sha384-SgOJa3DmI69IUzQ2PVdRZhwQ+dy64/BUtbMJw1MZ8t5HZApcHrRKUc4W0kG879m7 rel=stylesheet><h1>Connesso al Wi-Fi</h1>
    )rawliteral";
    }
    else
    { // Connessione Wi-Fi assente
        html = R"rawliteral(
        <!doctypehtml><html lang=en><meta charset=UTF-8><meta content="width=device-width,initial-scale=1"name=viewport><title>Gabbiotto IoT</title><style>*{box-sizing:border-box}body{width:100%;height:100vh;background-color:#1e2229;color:#fcfcfc;display:flex;justify-content:center;align-items:center}form{background-color:#fafafa;border-radius:20px;color:#2d2d2d;padding:100px}.input-text{width:100%;padding:5px;border-radius:5px;border:1px solid #000}.input-submit{margin-top:50px;padding-inline:15px;padding-block:10px;font-size:18px;border-radius:10px;background-color:#9d9d9d;color:#fff;border:1px solid transparent}.input-submit:hover{background-color:grey}.input-submit:active{transform:scale(.95)}label{margin-top:15px;font-size:22px}</style><form action=/wifi method=post><h1>Inserisci i dati della rete Wi-Fi</h1><div class=form-div><label for=ssid>SSID</label><br><input class=input-text id=ssid name=ssid placeholder=SSID><br><label for=password>Password</label><br><input class=input-text id=password name=password placeholder=password type=password><br><div style=display:flex;justify-content:center><input class=input-submit type=submit value=Invio></div></div></form>
        )rawliteral";
    }

    server.send(200, "text/html", html);
}

void aggiungiCredenziali()
{
    if (server.hasArg("ssid") && server.hasArg("password"))
    {
        String ssidPost = server.arg("ssid");
        String passwordPost = server.arg("password");

        WiFi.begin(ssidPost, passwordPost);
        for (int i = 0; i < 5 && WiFi.status() != WL_CONNECTED; i++)
        {
            delay(1000);
            Serial.print(".");
        }
        if (WiFi.status() == WL_CONNECTED)
        {
            display->println("\nConnesso! IP: " + WiFi.localIP().toString());
            display->display();

            server.send(200, "text/plain", "Accesso al Wi-Fi riuscito!");

            preferences.begin("credenzialiWiFi", false);
            preferences.putString("ssid", ssidPost);
            preferences.putString("password", passwordPost);
            preferences.end();

            delay(2000);

            ESP.restart();
        }
        else
        {
            server.send(400, "text/plain", "Errore: Credenziali Errate!");
        }
    }
    else
    {
        server.send(400, "text/plain", "Errore: dati mancanti!");
    }
}

void setup()
{
    Serial.begin(115200);

    IoTBoard::init_serial();
    IoTBoard::init_display();
    IoTBoard::init_leds();

    LoRaMesh::init(targa_gabbiotto, onReceive);

    WiFi.softAP(apSSID, apPassword);
    display->println("Access Point Attivato");
    display->println("Nome Rete: " + String(apSSID));
    display->println("Password: " + String(apPassword));
    display->display();

    preferences.begin("credenzialiWiFi", false);
    ssid = preferences.getString("ssid");
    password = preferences.getString("password");
    preferences.end();

    if (ssid != "" && password != "")
    {
        WiFi.begin(ssid, password);
        display->println("Connessione a WiFi...");
        for (int i = 0; i < 10 && WiFi.status() != WL_CONNECTED; i++)
        {
            delay(1000);
            Serial.print(".");
        }
        if (WiFi.status() == WL_CONNECTED)
        {
            display->println("\nConnesso! IP: " + WiFi.localIP().toString());
            display->display();
        }
    }
    else
    {
        display->println("\nCredenziali Wi-Fi mancanti");
        display->display();
    }

    server.on("/", handleRoot);
    server.on("/wifi", aggiungiCredenziali);
    server.begin();
}

void loop()
{
    LoRaMesh::update();

    server.handleClient();

    if (WiFi.status() == WL_CONNECTED)
    {
        while (!coda.empty())
        {
            LoRaMesh_message_t message = (LoRaMesh_message_t)coda.front();
            coda.pop();
            /*Serial.print("Destinatario: ");*/
            /*for(int i = 0; i < 7; i++) {*/
            /*    Serial.print(message.targa_destinatario[i]);*/
            /*}*/
            /*Serial.println();*/
            String key = backendService.getKeyFromTarga(message.targa_mittente);
            xorBuffer(&message.payload, sizeof(LoRaMesh_payload_t), (uint8_t *)key.c_str(), KEY_LEN);

            backendService.sendMessageToBackend(message);
            backendService.sendPosition(message);

            /*Serial.println("\n=== Messaggio LoRaMesh ricevuto ===");*/
            /**/
            /*Serial.print("Destinatario: ");*/
            /*for(int i = 0; i < 7; i++) {*/
            /*    Serial.print(message.targa_destinatario[i]);*/
            /*}*/
            /*Serial.println();*/
            /**/
            /*Serial.print("Mittente: ");*/
            /*for(int i = 0; i < 7; i++) {*/
            /*    Serial.print(message.targa_mittente[i]);*/
            /*}*/
            /*Serial.println();*/
            /**/
            /*Serial.println("ID messaggio: " + String(message.message_id));*/
            /*Serial.println("===================================\n");*/
            /**/
            /*String key = backendService.getKeyFromTarga(message.targa_mittente);*/
            /*xorBuffer(&message.payload, sizeof(LoRaMesh_payload_t), (uint8_t*)key.c_str(), KEY_LEN);*/
            /*Serial.println("===================================\n");*/
            /*Serial.println("Posizione: (" + String(message.payload.pos_x, 4) + ", " + String(message.payload.pos_y, 4) + ")");*/
            /*Serial.println("Direzione: " + String(message.payload.direzione) + "°");*/
            /*Serial.println("Stato: " + String(message.payload.stato == st_ormeggio ? "Ormeggiata" : "Rubata"));*/
            /*Serial.println("===================================\n");*/
        }

        if (millis() > nextFetch)
        {
            backendService.getBoatsToChange(codaBarche);

            while (!codaBarche.empty())
            {
                barca boat = codaBarche.front();
                codaBarche.pop();

                LoRaMesh_payload_t payload =
                    {
                        .message_sequence = 0,
                        .pos_x = 0,
                        .pos_y = 0,
                        .direzione = 0,
                    };
                if (boat.stato == "ormeggiata")
                {
                    payload.stato = st_ormeggio;
                }
                else if (boat.stato == "movimento")
                {
                    payload.stato = st_movimento;
                }
                LoRaMesh::sendMessage(boat.targa.c_str(), payload, boat.key.c_str());
                LoRaMesh::update();
            }
            nextFetch = millis() + fetchInterval;
        }
    }
    else
    {
        Serial.println("Nessuna connesione a internet");
    }

    delay(1000);
}

void onReceive(LoRaMesh_message_t message)
{
    coda.push(message);
    return;
}
