#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <queue>
#include "LoRaMesh/LoRaMesh.h"

struct barca
{
    String targa;
    String key;
    String stato;
};

class BackendService
{
private:
    const String baseUrl = "http://192.168.4.2:5001";

public:
    BackendService();

    // Invia i dati del messaggio LoRaMesh al backend
    bool sendMessageToBackend(const LoRaMesh_message_t &message);

    // Invia notifica per cambio stato (es. quando una barca viene rubata)
    bool sendStateChangeNotification(const LoRaMesh_message_t &message);

    // Invia aggiornamento posizione
    bool sendPosition(const LoRaMesh_message_t &message);

    String getKeyFromTarga(String targa);

    void getBoatsToChange(std::queue<barca> &);

    String formatTargaString(const char *targa, int length);
};
