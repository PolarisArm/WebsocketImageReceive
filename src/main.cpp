#include <WiFi.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <WebSocketsClient.h>

#include <TJpg_Decoder.h>
#include <SPI.h>
#include <TFT_eSPI.h>


const char* ssid = "Home";
const char* pass = "353Arm52@89";

WebSocketsClient webSocket;

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define DRAW_BUFSIZE ((SCREEN_HEIGHT * SCREEN_WIDTH) / 10)

TFT_eSPI tft = TFT_eSPI();


unsigned long lastMillis = 0;
char buf[50];

bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap)
{
  if(y >= 320) {return 0;}
  tft.pushImage(x,y,w,h,bitmap);
  //tft.pushImageDMA(x, y, w, h, bitmap); 

  return 1;
}


void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      //Serial.printf("[%u] Client disconnected\n", num);
      Serial.println("DISCONNECTED");
      break;
      
    case WStype_CONNECTED: {
      Serial.println("CONNECTED");
      //IPAddress ip = webSocket.remoteIP(num);
      //Serial.printf("[%u] Connection from %d.%d.%d.%d\n", num, ip[0], ip[1], ip[2], ip[3]);
      break;
    }

    case WStype_TEXT:
      Serial.printf("Received text: %s\n", payload);
      break;

    case WStype_BIN:
    {
      Serial.printf("Received binary data, length: %u, free Heap: %u\n", length,ESP.getFreeHeap());
      
      uint16_t  widthI = 0;
      uint16_t heightI = 0;
      
      TJpgDec.getJpgSize(&widthI,&heightI,(const uint8_t*)payload,length);
      //tft.startWrite();
      TJpgDec.drawJpg(0,0,(const uint8_t*)payload,length);
      //tft.endWrite();

      unsigned long time = millis()-lastMillis;
      lastMillis = millis();
      sprintf(buf,"Time: %lu ms",time);
      tft.setTextColor(TFT_WHITE, TFT_TRANSPARENT);
      tft.drawString(buf,80,220);
      break;
    }

  }
}

void setup() {
  Serial.begin(115200);
  delay(20);

  
  WiFi.begin(ssid, pass);
  WiFi.setSleep(false);

  Serial.println("WiFi Connecting ");

  while(WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }

  Serial.println(" ");
  Serial.println("WiFi Connected");

  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("");

 

  tft.begin();
  //tft.initDMA();

  tft.setRotation(1);
  tft.setTextColor(0xFFFF,0x0000);
  tft.fillScreen(TFT_GREENYELLOW);
  tft.setFreeFont(&FreeMono9pt7b);
  
  // Image Scaling
  TJpgDec.setJpgScale(1);

 // The byte order can be swapped (set true for TFT_eSPI)
  TJpgDec.setSwapBytes(true);

  TJpgDec.setCallback(tft_output);

  // Start WebSocket Client

  
  webSocket.begin("192.168.0.109",82,"/");
  webSocket.onEvent(webSocketEvent);
}

void loop() {
  webSocket.loop();
}