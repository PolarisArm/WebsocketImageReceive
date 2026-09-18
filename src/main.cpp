#define DEBUG_WEBSOCKETS_PORT Serial
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <WebSocketsClient.h>
#include <JPEGDEC.h>
#include <SPI.h>
#include <TFT_eSPI.h>

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define CIRCULAR_BUFFER_SIZE 3
#define FRAME_BUFFER_SIZE 24000
#define NUM_RECORDS 100

const char *ssid = "Home";
const char *pass = "353Arm52@89";

WebSocketsClient webSocket;
JPEGDEC jpeg;

TFT_eSPI tft = TFT_eSPI();

TaskHandle_t SocketTaskHandle;
TaskHandle_t VideoTaskHandle;
portMUX_TYPE bufMux = portMUX_INITIALIZER_UNLOCKED;

unsigned long lastMillis = 0;
char buf[50];

uint16_t *dmaBuffer1;
uint16_t *dmaBuffer2;
uint16_t *dmaBufferPtr;

uint8_t dmaBufferSel = 0;

typedef struct Frame
{
  uint8_t VideoBuffer[FRAME_BUFFER_SIZE];
  uint32_t VideoDataSize;
} Frame;

typedef struct CircularBuffer
{
  uint8_t writer;
  uint8_t reader;
  uint8_t count;
  Frame *frame;
} CircularBuffer;

CircularBuffer *dataBuffer;

uint8_t isEmpty(CircularBuffer *cbuf)
{

  portENTER_CRITICAL(&bufMux);
  //uint8_t empty = cbuf->count == cbuf->writer;
  uint8_t empty = cbuf->count == 0;
  portEXIT_CRITICAL(&bufMux);

  return empty;
}

uint8_t isFull(CircularBuffer *cbuf)
{

  portENTER_CRITICAL(&bufMux);
  //uint8_t full = ((cbuf->writer + 1) % CIRCULAR_BUFFER_SIZE) == cbuf->reader;
  uint8_t full = cbuf->count == CIRCULAR_BUFFER_SIZE;

  portEXIT_CRITICAL(&bufMux);

  return full;
}

void ReadBuffer(uint8_t *video_buffer, uint32_t *videoBufferSize, CircularBuffer *cbuf)
{
  if (isEmpty(cbuf) == 0)
  {
    portENTER_CRITICAL(&bufMux);
    Frame *video = &cbuf->frame[cbuf->reader];
    *videoBufferSize = video->VideoDataSize;
    portEXIT_CRITICAL(&bufMux);

    memcpy(video_buffer, video->VideoBuffer, *videoBufferSize);

    portENTER_CRITICAL(&bufMux);
    cbuf->count--;
    cbuf->reader = (cbuf->reader + 1) % CIRCULAR_BUFFER_SIZE;
    portEXIT_CRITICAL(&bufMux);
  }
}

int drawMCU(JPEGDRAW *pDraw)
{
  int iCount;
  iCount = pDraw->iWidth * pDraw->iHeight;
  // tft.pushImage(pDraw->x,pDraw->y,pDraw->iWidth,pDraw->iHeight,pDraw->pPixels);

  tft.dmaWait();

  if (dmaBufferSel)
  {
    dmaBufferPtr = dmaBuffer2;
  }
  else
  {
    dmaBufferPtr = dmaBuffer1;
  }
  dmaBufferSel = !dmaBufferSel;
  memcpy(dmaBufferPtr, pDraw->pPixels, iCount * sizeof(uint16_t));
  tft.pushImageDMA(pDraw->x, pDraw->y, pDraw->iWidth, pDraw->iHeight, dmaBufferPtr);
  return 1;
}

void webSocketEvent(WStype_t type, uint8_t *payload, size_t length)
{
  switch (type)
  {
  case WStype_DISCONNECTED:

    Serial.println("DISCONNECTED");
    break;

  case WStype_CONNECTED:
  {
    Serial.println("CONNECTED");
    break;
  }

  case WStype_TEXT:
    Serial.printf("Received text: %s\n", payload);
    break;

  case WStype_BIN:
  {
    Serial.printf("Received binary data, length: %u, free Heap: %u\n", length, ESP.getFreeHeap());
    if (length > FRAME_BUFFER_SIZE)
    {
      Serial.printf("Frame too large (%u > %u), dropping\n", length, FRAME_BUFFER_SIZE);
      break;
    }

    if (!isFull(dataBuffer))
    {
      portENTER_CRITICAL(&bufMux);
      Frame *frame = &dataBuffer->frame[dataBuffer->writer];
      frame->VideoDataSize = length;
      portEXIT_CRITICAL(&bufMux);

      memcpy(frame->VideoBuffer, payload, length); // inside critical

      portENTER_CRITICAL(&bufMux);
      dataBuffer->count++;
      dataBuffer->writer = (dataBuffer->writer + 1) % CIRCULAR_BUFFER_SIZE;
      portEXIT_CRITICAL(&bufMux);

      xTaskNotifyGive(VideoTaskHandle);
    }
    else
    {
      Serial.println("Buffer is full");
    }

    break;
  }
  }
}

void SocketTask(void *pvParameter)
{

  while (true)
  {

    webSocket.loop();
    vTaskDelay(2 / portTICK_PERIOD_MS);
  }
}

void VideoTask(void *pvParamter)
{

  uint32_t videoBufferLength;
  static uint8_t videoBuffer[FRAME_BUFFER_SIZE]; // Need to be static
  while (true)
  {

    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    ReadBuffer(videoBuffer, &videoBufferLength, dataBuffer);
    tft.startWrite();
    if (jpeg.openRAM(videoBuffer, videoBufferLength, drawMCU))
    {

      // tft.setSwapBytes(true);
      jpeg.setPixelType(RGB565_BIG_ENDIAN);
      jpeg.decode(0, 0, 0);
      jpeg.close();
    }

    tft.dmaWait();
    tft.endWrite();

    unsigned long time = millis() - lastMillis;
    lastMillis = millis();
    sprintf(buf, "Time: %lu ms", time);
    tft.setTextColor(TFT_WHITE, TFT_TRANSPARENT);
    tft.drawString(buf, 80, 220);
    vTaskDelay(5 / portTICK_PERIOD_MS);

    // UBaseType_t stackMark = uxTaskGetStackHighWaterMark(NULL);
    // Serial.printf("Task stack free space %u \n",stackMark);
  }
}

void setup()
{

  Serial.begin(115200);
  delay(20);

  dataBuffer = (CircularBuffer *)heap_caps_malloc(sizeof(CircularBuffer), MALLOC_CAP_8BIT);
  Serial.printf("Free heap before frame alloc: %u, largest block: %u\n",
                ESP.getFreeHeap(), ESP.getMaxAllocHeap());

  if (dataBuffer == NULL)
  {
    Serial.println("[MEM Failed] DATA Buffer allocation failed");
    return;
  }

  dataBuffer->frame = (Frame *)heap_caps_malloc(sizeof(Frame) * CIRCULAR_BUFFER_SIZE, MALLOC_CAP_8BIT);
  Serial.printf("Free heap before frame alloc: %u, largest block: %u\n", ESP.getFreeHeap(), ESP.getMaxAllocHeap());
  dataBuffer->reader = 0;
  dataBuffer->writer = 0;
  dataBuffer->count  = 0;

  if (dataBuffer->frame == NULL)
  {
    free(dataBuffer);
    Serial.println("[MEM Failed] Frame Buffer allocation failed");
    return;
  }

  dmaBuffer1 = (uint16_t *)heap_caps_malloc(2048 * sizeof(uint16_t), MALLOC_CAP_DMA); // 8192 bytes
  dmaBuffer2 = (uint16_t *)heap_caps_malloc(2048 * sizeof(uint16_t), MALLOC_CAP_DMA);

  if (dmaBuffer1 == NULL)
  {
    free(dataBuffer);
    free(dataBuffer->frame);
    Serial.println("[DMA MEM Failed] DMA Buffer allocation failed");
    return;
  }

  if (dmaBuffer2 == NULL)
  {
    free(dmaBuffer1);
    free(dataBuffer);
    free(dataBuffer->frame);
    Serial.println("[DMA MEM Failed] DMA Buffer 2 allocation failed");
    return;
  }

  dmaBufferPtr = dmaBuffer1;

  WiFi.begin(ssid, pass);
  WiFi.setSleep(false);

  Serial.println("WiFi Connecting ");

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println(" ");
  Serial.println("WiFi Connected");

  tft.begin();
  tft.initDMA();

  tft.setRotation(1);
  tft.setTextColor(0xFFFF, 0x0000);
  tft.fillScreen(TFT_GREENYELLOW);
  tft.setFreeFont(&FreeMono9pt7b);

  // Start WebSocket Client
  webSocket.begin("192.168.0.109", 82, "/");
  webSocket.onEvent(webSocketEvent);

  BaseType_t ok1 = xTaskCreatePinnedToCore(SocketTask, "Loop Handler", 8192, NULL, 2, &SocketTaskHandle, 0);

  if (ok1 == pdTRUE)
  {
    Serial.printf("[Socket] Free heap before frame alloc: %u, largest block: %u\n",
                  ESP.getFreeHeap(), ESP.getMaxAllocHeap());
  }

  uint32_t before = ESP.getFreeHeap();

  BaseType_t ok2 = xTaskCreatePinnedToCore(VideoTask, "Video Handler", 16384, NULL, 1, &VideoTaskHandle, 1);
  if (ok2 == pdTRUE)
  {
    Serial.printf("[Video]Free heap before frame alloc: %u, largest block: %u\n",
                  ESP.getFreeHeap(), ESP.getMaxAllocHeap());
  }
  Serial.printf("[SocketTask] heap consumed by creation: %u bytes (stack=8192 + TCB overhead)\n", before - ESP.getFreeHeap());
}

void loop()
{

  vTaskDelay(5 / portTICK_PERIOD_MS);
}