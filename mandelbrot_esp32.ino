#include <Arduino.h>
#include <atomic>

// --- 1. CONFIGURATION ---
const int IMAGE_WIDTH = 320;
const int IMAGE_HEIGHT = 240;
const int MAX_ITER = 100; // Higher = more detail, but slower

// The mathematical bounds of the Mandelbrot set to draw
const float X_MIN = -2.0;
const float X_MAX = 1.0;
const float Y_MIN = -1.2;
const float Y_MAX = 1.2;

// --- 2. DATA STRUCTURES ---
// Structure to package a calculated pixel
typedef struct {
  uint16_t x;
  uint16_t y;
  uint16_t iterations;
} PixelData;

// The Queue to safely pass data from the cores to the Serial printer
QueueHandle_t resultQueue;

// Atomic variable for dynamic load balancing
// Both cores will safely read and increment this without colliding
std::atomic<int> currentRow(0); 

// --- 3. THE MATH TASK (Runs on BOTH Cores) ---
void calculateMandelbrotTask(void *parameter) {
  for(;;) {
    // 3a. Grab the next available row to calculate
    int y = currentRow.fetch_add(1);
    
    // If all rows are done, wait a bit and restart (looping the animation)
    if (y >= IMAGE_HEIGHT) {
      vTaskDelay(pdMS_TO_TICKS(1000));
      // Optional: Reset currentRow to 0 here if you want it to compute over and over
      continue; 
    }

    // 3b. Calculate the entire row pixel by pixel
    for (int x = 0; x < IMAGE_WIDTH; x++) {
      
      // Map pixel coordinate to the complex plane
      float c_r = X_MIN + (X_MAX - X_MIN) * x / IMAGE_WIDTH;
      float c_i = Y_MIN + (Y_MAX - Y_MIN) * y / IMAGE_HEIGHT;
      
      float z_r = 0.0;
      float z_i = 0.0;
      int iter = 0;
      
      // The Mandelbrot escape time algorithm
      while (z_r * z_r + z_i * z_i <= 4.0 && iter < MAX_ITER) {
        float temp_z_r = z_r * z_r - z_i * z_i + c_r;
        z_i = 2.0 * z_r * z_i + c_i;
        z_r = temp_z_r;
        iter++;
      }
      
      // 3c. Package and send the result
      PixelData p = { (uint16_t)x, (uint16_t)y, (uint16_t)iter };
      
      // Send to queue. portMAX_DELAY means wait if the queue is temporarily full
      xQueueSend(resultQueue, &p, portMAX_DELAY);
    }
    
    // Tiny yield to reset the Watchdog Timer and prevent RTOS crashes
    vTaskDelay(pdMS_TO_TICKS(1)); 
  }
}

// --- 4. SYSTEM SETUP ---
void setup() {
  // Use a fast baud rate since we are sending a LOT of pixel data
  Serial.begin(500000); 
  
  // Create a queue large enough to hold 500 pixels at a time
  resultQueue = xQueueCreate(500, sizeof(PixelData));

  // Pin the exact same task to Core 0 and Core 1
  xTaskCreatePinnedToCore(calculateMandelbrotTask, "FractalCore0", 10000, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(calculateMandelbrotTask, "FractalCore1", 10000, NULL, 1, NULL, 1);
}

// --- 5. DATA STREAMING (Runs on Core 1 by default) ---
void loop() {
  PixelData receivedPixel;
  
  // Read from the queue as fast as possible without blocking
  if (xQueueReceive(resultQueue, &receivedPixel, 0) == pdTRUE) {
    // Stream data in a simple CSV format: X,Y,ITERATIONS
    // This makes it incredibly easy for Python to parse on the other end
    Serial.printf("%d,%d,%d\n", receivedPixel.x, receivedPixel.y, receivedPixel.iterations);
  }
}