#include <Arduino.h>
#include <XboxController.h>

bool LightTurnL = false;
bool LightTurnR = false;
bool HazardLight = false;

#define PIN_L_LIGHT 25 // 左转向灯
#define PIN_R_LIGHT 26 // 右转向灯
#define CHANNEL_LIGHT_L 2
#define CHANNEL_LIGHT_R 3

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

void TaskHeadlight(void *pt)
{
  int pwmChannel = *(int *)pt;

  while (true)
  {
    ledcWrite(pwmChannel, 10);
    vTaskDelay(2000);
    ledcWrite(pwmChannel, 100);
    vTaskDelay(2000);
    ledcWrite(pwmChannel, 255);
    vTaskDelay(2000);
  }
}

void HeadlightSetup(int pin, int pwmChannel)
{
  pinMode(pin, OUTPUT);
  ledcSetup(pwmChannel, 2000, 8);
  ledcAttachPin(pin, pwmChannel);

  xTaskCreate(TaskHeadlight, "TaskHeadlight", 1024, (void *)&pwmChannel, 3, NULL);
}

/* 转向灯任务 */
void TaskIndicatorLight(void *pt)
{
  bool increasing = false;
  int lightLevel = 100;

  const int MAX = 100;
  const int MIN = 0;
  const double step = (double)255 / (double)MAX;

  while (true)
  {

    if (LightTurnL || LightTurnR || HazardLight)
    {
      increasing ? lightLevel++ : lightLevel--;
      increasing = lightLevel >= MAX   ? false
                   : lightLevel <= MIN ? true
                                       : increasing;
    }

    int v = lightLevel <= MIN ? MIN : round((double)lightLevel * step);
    LightTurnL || HazardLight // 左转向灯
        ? ledcWrite(CHANNEL_LIGHT_L, v)
        : ledcWrite(CHANNEL_LIGHT_L, 0);

    LightTurnR || HazardLight // 右转向灯
        ? ledcWrite(CHANNEL_LIGHT_R, v)
        : ledcWrite(CHANNEL_LIGHT_R, 0);

    lightLevel == MIN || lightLevel == MAX ? vTaskDelay(200) : vTaskDelay(1);
  }
}

/* 转向灯按钮控制任务 */
void TaskIndicatorLightControl(void *pt)
{
  bool changed = false;
  bool btnLB_old, btnRB_old, btnLB, btnRB;
  XboxController controller = *(XboxController *)pt;

  while (true)
  {

    btnLB = controller.data.btnLB;
    btnRB = controller.data.btnRB;

    // 当按下的按钮有变化时重置 changed
    if (btnLB != btnLB_old || btnRB != btnRB_old)
    {
      changed = false;
    }

    if (btnLB && btnRB)
    {
      LightTurnR = false;
      LightTurnL = false;
      HazardLight = changed ? HazardLight : !HazardLight;
      changed = true;
    }
    else if (btnLB && !HazardLight && btnLB != btnLB_old)
    {
      LightTurnL = changed ? LightTurnL : !LightTurnL;
      LightTurnR = false;
      HazardLight = false;
      changed = true;
    }
    else if (btnRB && !HazardLight && btnRB != btnRB_old)
    {
      LightTurnR = changed ? LightTurnR : !LightTurnR;
      LightTurnL = false;
      HazardLight = false;
      changed = true;
    }

    // 在循环结尾处储存本次循环按钮的值以备在下个循环中比对
    btnLB_old = btnLB;
    btnRB_old = btnRB;

    vTaskDelay(5);
  }
}

void IndicatorLightSetup(void *controller, int pin, int pwmChannel)
{

  pinMode(PIN_L_LIGHT, OUTPUT);
  pinMode(PIN_R_LIGHT, OUTPUT);

  ledcSetup(CHANNEL_LIGHT_L, 2000, 8);
  ledcAttachPin(25, CHANNEL_LIGHT_L);

  ledcSetup(CHANNEL_LIGHT_R, 2000, 8);
  ledcAttachPin(26, CHANNEL_LIGHT_R);

  xTaskCreate(TaskIndicatorLight, "Indicator Light", 1024, (void *)controller, 3, NULL);
  xTaskCreate(TaskIndicatorLightControl, "IndicatorLightControl", 1024, (void *)controller, 3, NULL);
}