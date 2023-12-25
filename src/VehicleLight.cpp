#include <Arduino.h>
#include <XboxController.h>

void TaskStatusLight(void *pt)
{
  int pwmChannel = *(int *)pt;
  int resolution = 100;
  double step = (double)255 / (double)resolution;
  while (true)
  {
    if (XboxController::connected)
    {
      ledcWrite(pwmChannel, 255);
      vTaskDelay(100);
      ledcWrite(pwmChannel, 0);
      vTaskDelay(100);
      ledcWrite(pwmChannel, 255);
      vTaskDelay(100);
      ledcWrite(pwmChannel, 0);
      vTaskDelay(2000);
    }
    else
    {
      for (int i = 0; i <= 100; i = i + 5)
      {
        ledcWrite(pwmChannel, round((double)i * step));
        vTaskDelay(3);
      }
      vTaskDelay(100);

      for (int i = 100; i >= 0; i = i - 5)
      {
        ledcWrite(pwmChannel, round((double)i * step));
        vTaskDelay(3);
      }
      vTaskDelay(100);
    }
  }
}

void StatusLightSetup(int pin, int pwmChannel)
{
  pinMode(pin, OUTPUT);
  ledcSetup(pwmChannel, 2000, 8);
  ledcAttachPin(pin, pwmChannel);
  xTaskCreate(TaskStatusLight, "status light", 1024, (void *)&pwmChannel, 3, NULL);
}