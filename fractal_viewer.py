import serial
import pygame
import colorsys
import sys

# --- 1. CONFIGURATION ---
# These MUST match your ESP32 settings
WIDTH = 320
HEIGHT = 240
MAX_ITER = 100
BAUD_RATE = 500000

# CHANGE THIS to your ESP32's port (e.g., 'COM3' on Windows, '/dev/ttyUSB0' on Linux/Mac)
COM_PORT = 'COM3' 

# --- 2. COLOR MAPPING ---
def get_color(iterations):
    """ Converts the iteration count into a smooth RGB color. """
    if iterations >= MAX_ITER:
        return (0, 0, 0)  # Black for points inside the Mandelbrot set
    
    # Use HSV to create a cool, continuous color gradient based on iterations
    hue = iterations / float(MAX_ITER)
    saturation = 1.0
    value = 1.0 if iterations < MAX_ITER else 0.0
    
    # Convert HSV to RGB (returns values 0.0 to 1.0, so multiply by 255)
    r, g, b = colorsys.hsv_to_rgb(hue, saturation, value)
    return (int(r * 255), int(g * 255), int(b * 255))

# --- 3. INITIALIZATION ---
pygame.init()
screen = pygame.display.set_mode((WIDTH, HEIGHT))
pygame.display.set_caption("ESP32 Dual-Core Fractal Engine")

try:
    ser = serial.Serial(COM_PORT, BAUD_RATE, timeout=1)
    print(f"Connected to ESP32 on {COM_PORT}")
except Exception as e:
    print(f"Failed to connect to {COM_PORT}. Check your port name!\nError: {e}")
    sys.exit()

# --- 4. MAIN INGESTION LOOP ---
running = True
pixels_drawn = 0

print("Waiting for data stream...")

while running:
    # Check if the user clicked the 'X' to close the window
    for event in pygame.event.get():
        if event.type == pygame.QUIT:
            running = False

    # Read a line from the serial port
    if ser.in_waiting > 0:
        try:
            # Read, decode, and strip hidden characters (like \r\n)
            raw_data = ser.readline().decode('utf-8').strip()
            
            # Parse the CSV format: X,Y,ITER
            parts = raw_data.split(',')
            if len(parts) == 3:
                x = int(parts[0])
                y = int(parts[1])
                iterations = int(parts[2])

                # Get the color and draw the pixel
                color = get_color(iterations)
                screen.set_at((x, y), color)
                
                pixels_drawn += 1

                # Batch UI updates: Updating the screen every single pixel is too slow.
                # We update the actual display every 500 pixels to maintain performance.
                if pixels_drawn % 500 == 0:
                    pygame.display.flip()

        except ValueError:
            # Ignore garbled serial data (common right when the ESP32 boots up)
            pass
        except UnicodeDecodeError: # type: ignore
            # Ignore decoding errors from partial bytes
            pass

# Clean up
ser.close()
pygame.quit()