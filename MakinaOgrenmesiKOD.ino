#define WIFI_SSID "TurkTelekom_ZTT3AK"
#define WIFI_PASS "Kfa9ahaEk4x3"
#define HOSTNAME "esp32cam"


#include <eloquent_esp32cam.h>
#include <eloquent_esp32cam/extra/esp32/wifi/sta.h>
#include <eloquent_esp32cam/viz/image_collection.h>

using eloq::camera;
using eloq::wifi;
using eloq::viz::collectionServer;


void setup() {
    delay(3000);
    Serial.begin(115200);
    Serial.println("___IMAGE COLLECTION SERVER___");

    // camera settings
    // replace with your own model!
    camera.pinout.aithinker();
    camera.brownout.disable();
    // Edge Impulse models work on square images
    // face resolution is 240x240
    camera.resolution.face();
    camera.quality.high();

    // init camera
    while (!camera.begin().isOk())
        Serial.println(camera.exception.toString());

    // connect to WiFi
    while (!wifi.connect().isOk())
      Serial.println(wifi.exception.toString());

    // init face detection http server
    while (!collectionServer.begin().isOk())
        Serial.println(collectionServer.exception.toString());

    Serial.println("Camera OK");
    Serial.println("WiFi OK");
    Serial.println("Image Collection Server OK");
    Serial.println(collectionServer.address());
}


void loop() {
    // server runs in a separate thread, no need to do anything here
}