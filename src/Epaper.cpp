// mathutils.cpp Include your header file
#include "Epaper.h"

// Function definitions
void ePaperSetup()
{
    display.init(115200, true, 2, false); // Waveshare reset circuit timing

  display.setRotation(1);
  display.setFont(&FreeMonoBold9pt7b);
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(10, 30);
    display.print("Ready to display!");
  } while (display.nextPage());

  display.hibernate();
}

int multiply(int a, int b)
{
    return a * b;
}