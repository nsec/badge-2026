#include "ics_challenge.h"

#include "drm.h"
#include "storage/nvs_wifi_creds.h"
#include "tasks/cli.h"

namespace challenges {
namespace ics {

namespace {

const char *mqtt_server = "192.168.4.137";

// RC4-encrypted MQTT password; key = "DEBUG"
static const uint8_t kEncryptedPass[] = {
    0x46, 0x44, 0x9d, 0x40, 0xb5, 0xd4, 0xf8, 0xc1, 0x40, 0x3e, 0x26, 0x3f, 0xa1, 0xfc, 0xff, 0xa5, 0xf3, 0xaa,
    0x0a, 0x8b, 0x21, 0x86, 0x0d, 0x9a, 0x7f, 0xe0, 0xaa, 0xb8, 0xbd, 0xc7, 0x2e, 0xc6, 0xa1, 0x26, 0x7b,
};
static const uint8_t kRc4Key[] = "DEBUG";  // 5 bytes, no null

static char mqttPass[sizeof(kEncryptedPass) + 1];

void rc4(const uint8_t *key, size_t keyLen, const uint8_t *in, uint8_t *out, size_t len) {
  uint8_t S[256];
  for (int i = 0; i < 256; i++)
    S[i] = i;

  uint8_t j = 0;
  for (int i = 0; i < 256; i++) {
    j = (j + S[i] + key[i % keyLen]) & 0xFF;
    uint8_t tmp = S[i];
    S[i] = S[j];
    S[j] = tmp;
  }

  uint8_t x = 0, y = 0;
  for (size_t k = 0; k < len; k++) {
    x = (x + 1) & 0xFF;
    y = (y + S[x]) & 0xFF;
    uint8_t tmp = S[x];
    S[x] = S[y];
    S[y] = tmp;
    out[k] = in[k] ^ S[(S[x] + S[y]) & 0xFF];
  }
}

WiFiClientSecure espClient;
PubSubClient *client = nullptr;
unsigned long lastMsg = 0;
char msg[50];
int value = 0;
bool active = false;

enum class DrmState : uint8_t { S1, S1B, S2, S3, S4, S5 };
DrmState drmState = DrmState::S1;
int64_t drmAccum = 1;

void handleDrmMessage(int val) {
  switch (drmState) {
    case DrmState::S1:
      drmAccum *= val;
      if (drm::passthrough(val > 50)) {
        drmState = DrmState::S2;
        client->publish("drm", "67");
      }
      if (drm::invert(val > 50)) {
        drmState = DrmState::S1B;
        client->publish("drm", "420");
      }
      break;
    case DrmState::S1B:
      drmAccum *= val;
      if (drm::permit(val % 2 != 0) && drm::passthrough(val % 2 != 0))
        client->publish("drm", "42");
      if (drm::invert(val % 2 != 0) || drm::deny(val % 2 != 0))
        client->publish("drm", "9000");
      drmState = DrmState::S3;
      break;
    case DrmState::S2:
      drmAccum *= val;
      if (drm::passthrough(val < 67)) {
        drmState = DrmState::S3;
        client->publish("drm", "10");
      }
      if (drm::invert(val < 67)) {
        drmState = DrmState::S5;
        client->publish("drm", "5");
      }
      break;
    case DrmState::S3:
      drmAccum *= val;
      if (drm::permit(val % 2 == 0) && drm::passthrough(val % 2 == 0))
        drmState = DrmState::S4;
      if (drm::passthrough(val % 2 != 0) || drm::deny(val % 2 != 0))
        drmState = DrmState::S5;
      break;
    case DrmState::S4: {
      char resBuf[32];
      snprintf(resBuf, sizeof(resBuf), "%lld", (long long)drmAccum);
      client->publish("drm", resBuf);
      drmState = DrmState::S5;
      client->publish("water", "open_valve");
      break;
    }
    case DrmState::S5:
      drmState = DrmState::S1;
      break;
  }
}

void callback(char *topic, byte *payload, unsigned int length) {
  Serial.printf("Message arrived [%s] ", topic);
  for (unsigned int i = 0; i < length; i++)
    Serial.print((char)payload[i]);
  Serial.println();

  if ((char)payload[0] == '1')
    digitalWrite(BUILTIN_LED, LOW);
  else
    digitalWrite(BUILTIN_LED, HIGH);

  if (strcmp(topic, "drm") == 0) {
    char numBuf[32] = {};
    size_t copyLen = length < sizeof(numBuf) - 1 ? length : sizeof(numBuf) - 1;
    memcpy(numBuf, payload, copyLen);
    handleDrmMessage(atoi(numBuf));
  }
}

void reconnect() {
  while (!client->connected()) {
    Serial.print("[ics] Attempting MQTT connection...");
    auto creds = core::storage::wifiCredsGet();
    if (client->connect(creds.ssid, "PLANT_SYSTEM", mqttPass)) {
      Serial.println("connected");
      client->subscribe("drm");
    } else {
      Serial.printf("failed, rc=%d — retrying in 5s\r\n", client->state());
      delay(5000);
    }
  }
}

void start(Stream &stream) {
  rc4(kRc4Key, sizeof(kRc4Key) - 1, kEncryptedPass, reinterpret_cast<uint8_t *>(mqttPass), sizeof(kEncryptedPass));
  mqttPass[sizeof(kEncryptedPass)] = '\0';

  auto creds = core::storage::wifiCredsGet();

  WiFi.mode(WIFI_AP);
  WiFi.softAP(creds.ssid, creds.passphrase);

  IPAddress apIP = WiFi.softAPIP();
  stream.printf("[ics] AP started: SSID=\"%s\" pass=\"%s\" @ %s\r\n", creds.ssid, creds.passphrase,
                apIP.toString().c_str());

  espClient.setInsecure();
  client = new PubSubClient(espClient);
  client->setServer(mqtt_server, 8883);
  client->setCallback(callback);

  active = true;
}

void stop(Stream &stream) {
  if (client) {
    if (client->connected())
      client->disconnect();
    delete client;
    client = nullptr;
  }

  WiFi.softAPdisconnect(true);
  drmState = DrmState::S1;
  drmAccum = 1;
  active = false;
  stream.println("[ics] Stopped");
}

}  // namespace

void init() {
  core::cli::registerCommand("ics", "toggle ICS challenge", [](Stream &stream, const std::string &) {
    if (active)
      stop(stream);
    else
      start(stream);
  });

  core::cli::registerCommand("mqtt_pub", "publish to MQTT broker (topic payload)",
                             [](Stream &stream, const std::string &args) {
                               size_t sep = args.find(' ');
                               if (sep == std::string::npos) {
                                 stream.println("[mqtt_pub] usage: mqtt_pub <topic> <payload>");
                                 return;
                               }
                               std::string topic = args.substr(0, sep);
                               std::string payload = args.substr(sep + 1);

                               WiFiClient plainClient;
                               PubSubClient pubClient(plainClient);
                               pubClient.setServer("192.168.4.137", 1337);

                               stream.printf("[mqtt_pub] connecting to 192.168.4.137:1337...\r\n");
                               if (!pubClient.connect("badge-mqtt-pub")) {
                                 stream.printf("[mqtt_pub] connect failed, rc=%d\r\n", pubClient.state());
                                 return;
                               }
                               if (pubClient.publish(topic.c_str(), payload.c_str()))
                                 stream.printf("[mqtt_pub] published '%s' -> '%s'\r\n", payload.c_str(), topic.c_str());
                               else
                                 stream.println("[mqtt_pub] publish failed");
                               pubClient.disconnect();
                             });
}

void tick() {
  if (!active)
    return;

  if (!client->connected())
    reconnect();
  client->loop();
}

}  // namespace ics
}  // namespace challenges
