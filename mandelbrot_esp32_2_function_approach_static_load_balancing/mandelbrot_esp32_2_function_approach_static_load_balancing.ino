
// --- 1. CONFIGURATION ---
const int IMAGE_WIDTH = 320;
const int IMAGE_HEIGHT = 240;
const int MAX_ITER = 100;

const float X_MIN = -2.0;
const float X_MAX = 1.0;
const float Y_MIN = -1.2;
const float Y_MAX = 1.2;

// --- 2. DATA STRUCTURES ---
typedef struct {
  uint16_t x;
  uint16_t y;
  uint16_t iterations;
} PixelData;

QueueHandle_t resultQueue;

// --- 3. TASK A: TOP HALF (Runs on Core 0) ---
void calculateTopHalf(void *parameter) {
  // Define boundaries: Row 0 to Row 119
  int start_y = 0;
  int end_y = IMAGE_HEIGHT / 2;

  for (int y = start_y; y < end_y; y++) {
    for (int x = 0; x < IMAGE_WIDTH; x++) {
      
      float c_r = X_MIN + (X_MAX - X_MIN) * x / IMAGE_WIDTH;
      float c_i = Y_MIN + (Y_MAX - Y_MIN) * y / IMAGE_HEIGHT;
      float z_r = 0.0;
      float z_i = 0.0;
      int iter = 0;
      
      while (z_r * z_r + z_i * z_i <= 4.0 && iter < MAX_ITER) {
        float temp_z_r = z_r * z_r - z_i * z_i + c_r;
        z_i = 2.0 * z_r * z_i + c_i;
        z_r = temp_z_r;
        iter++;
      }
      
      PixelData p = { (uint16_t)x, (uint16_t)y, (uint16_t)iter };
      xQueueSend(resultQueue, &p, portMAX_DELAY);
    }
    // Yield to the FreeRTOS watchdog timer so the core doesn't crash
    vTaskDelay(pdMS_TO_TICKS(1)); 
  }
  
  // The top half is fully calculated. Safely delete this task.
  vTaskDelete(NULL);
}

// --- 4. TASK B: BOTTOM HALF (Runs on Core 1) ---
void calculateBottomHalf(void *parameter) {
  // Define boundaries: Row 120 to Row 240
  int start_y = IMAGE_HEIGHT / 2;
  int end_y = IMAGE_HEIGHT;

  for (int y = start_y; y < end_y; y++) {
    for (int x = 0; x < IMAGE_WIDTH; x++) {
      
      float c_r = X_MIN + (X_MAX - X_MIN) * x / IMAGE_WIDTH;
      float c_i = Y_MIN + (Y_MAX - Y_MIN) * y / IMAGE_HEIGHT;
      float z_r = 0.0;
      float z_i = 0.0;
      int iter = 0;
      
      while (z_r * z_r + z_i * z_i <= 4.0 && iter < MAX_ITER) {
        float temp_z_r = z_r * z_r - z_i * z_i + c_r;
        z_i = 2.0 * z_r * z_i + c_i;
        z_r = temp_z_r;
        iter++;
      }
      
      PixelData p = { (uint16_t)x, (uint16_t)y, (uint16_t)iter };
      xQueueSend(resultQueue, &p, portMAX_DELAY);
    }
    // Yield to the FreeRTOS watchdog timer
    vTaskDelay(pdMS_TO_TICKS(1)); 
  }

  // The bottom half is fully calculated. Safely delete this task.
  vTaskDelete(NULL);
}

// --- 5. SYSTEM SETUP ---
void setup() {
  Serial.begin(500000); 
  
  // Create the queue
  resultQueue = xQueueCreate(500, sizeof(PixelData));

  // Pin Task A to Core 0
  xTaskCreatePinnedToCore(calculateTopHalf, "TopHalfTask", 10000, NULL, 1, NULL, 0);
  
  // Pin Task B to Core 1
  xTaskCreatePinnedToCore(calculateBottomHalf, "BottomHalfTask", 10000, NULL, 1, NULL, 1);
}

// --- 6. DATA STREAMING ---
void loop() {
  PixelData receivedPixel;
  
  // Continuously drain the queue and send it to Python
  if (xQueueReceive(resultQueue, &receivedPixel, 0) == pdTRUE) {
    Serial.printf("%d,%d,%d\n", receivedPixel.x, receivedPixel.y, receivedPixel.iterations);
  }
}