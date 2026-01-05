#include "ics_challenge.h"
#include "secrets.h"

const char* mqtt_server = "192.168.1.244";

namespace challenges {
namespace ics {

WiFiClientSecure espClient;
PubSubClient* client = nullptr;
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE	(50)
char msg[MSG_BUFFER_SIZE];
int value = 0;

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Switch on the LED if an 1 was received as first character
  if ((char)payload[0] == '1') {
    digitalWrite(BUILTIN_LED, LOW);   // Turn the LED on (Note that LOW is the voltage level
    // but actually the LED is on; this is because
    // it is active low on the ESP-01)
  } else {
    digitalWrite(BUILTIN_LED, HIGH);  // Turn the LED off by making the voltage HIGH
  }

}

void reconnect() {
  // Loop until we're reconnected
  while (!client->connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "NSEC-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client->connect(clientId.c_str(), "esp_user", "esp_pass")) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client->publish("outTopic", "hello world");
      // ... and resubscribe
      client->subscribe("inTopic");
      client->subscribe("outTopic");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client->state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void init() {

  Serial.begin(115200);
  while (!Serial) { }

  Serial.print("Attempting to connect to SSID: ");
  Serial.println(SSID);

  WiFi.useStaticBuffers(true);
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("Connected to WiFi");

  espClient.setInsecure();
  client = new PubSubClient(espClient);
  client->setServer(mqtt_server, 8883);
  client->setCallback(callback);

  reconnect();
}

void tick() {
  client->loop();

  unsigned long now = millis();
  if (now - lastMsg > 2000) {
    lastMsg = now;
    ++value;
    snprintf (msg, MSG_BUFFER_SIZE, "hello world #%ld", value);
    Serial.print("Publish message: ");
    Serial.println(msg);
    client->publish("outTopic", msg);
  }
}

} // namespace ics
} // namespace challenges
