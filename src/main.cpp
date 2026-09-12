#include <WiFi.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <WebSocketsClient.h>
#include <JPEGDEC.h>
#include <SPI.h>
#include <TFT_eSPI.h>


const char* ssid = "Home";
const char* pass = "353Arm52@89";

WebSocketsClient webSocket;
JPEGDEC jpeg;

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define DRAW_BUFSIZE ((SCREEN_HEIGHT * SCREEN_WIDTH) / 10)

TFT_eSPI tft = TFT_eSPI();


unsigned long lastMillis = 0;
char buf[50];


int drawMCU(JPEGDRAW* pDraw){
  int iCount;
  iCount = pDraw->iWidth * pDraw->iHeight;
  //tft.pushImageDMA(x, y, w, h, bitmap); 
  tft.pushImage(pDraw->x,pDraw->y,pDraw->iWidth,pDraw->iHeight,pDraw->pPixels);

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
   

      if(jpeg.openRAM(payload,length,drawMCU)){

        //tft.setSwapBytes(true);
        jpeg.setPixelType(RGB565_BIG_ENDIAN);
        jpeg.decode(0,0,0);
        jpeg.close();
      }

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
  
 // Start WebSocket Client

  
  webSocket.begin("192.168.0.109",82,"/");
  webSocket.onEvent(webSocketEvent);
}

void loop() {
  webSocket.loop();
}