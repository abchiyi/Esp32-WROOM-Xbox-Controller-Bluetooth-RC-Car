#include <Arduino.h>
#include <VehicleControl.h>

XboxController xboxController;

// 定义控制 Pin
#define PIN_MOVE 12    // 移动控制
#define PIN_MOVE_R 27  // 倒车控制
#define PIN_TURN 26    // 转向控制
#define PIN_LIGHT 25   // 状态灯
#define PIN_L_LIGHT 33 // 左转向灯
#define PIN_R_LIGHT 32 // 右转向灯
#define CHANNEL_LIGHT_L 1
#define CHANNEL_LIGHT_R 2

bool LightTurnL = false;
bool LightTurnR = false;
bool HazardLight = true;

struct ButtonListenData
{
  bool *value;
  bool *button;
  SemaphoreHandle_t *mutex = nullptr;
};

/* 监听一个按钮按下 */
void TaskButtonListen(void *pt)
{
  ButtonListenData data = *(ButtonListenData *)pt;
  bool changed = false;

  if (data.mutex == nullptr) // 不使用 mutex
  {
    while (true)
    {
      vTaskDelay(20);
      if (*data.button && xboxController.connected)
      {
        *data.value = changed ? *data.value : !*data.value;
        changed = true;
      }
      else
      {
        changed = false;
      }
    }
  }

  while (true)
  {
    vTaskDelay(20);
    if (*data.button && xboxController.connected)
    {

      if (changed) // 按钮未被释放时跳过;
      {
        continue;
      }

      if (xSemaphoreTake(*data.mutex, 1000))
      {
        *data.value = !*data.value;
        changed = true;
        xSemaphoreGive(*data.mutex);
      }
    }
    else
    {
      changed = false;
    }
  }
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

  while (true)
  {

    btnLB = xboxController.data.btnLB;
    btnRB = xboxController.data.btnRB;

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

void lightFast()
{
  analogWrite(PIN_LIGHT, 100);
  vTaskDelay(100);
  analogWrite(PIN_LIGHT, 0);
}

void lightSlow()
{
  for (int i = 0; i <= 100; i = i + 5)
  {
    analogWrite(PIN_LIGHT, i);
    vTaskDelay(3);
  }
  vTaskDelay(100);

  for (int i = 100; i >= 0; i = i - 5)
  {
    analogWrite(PIN_LIGHT, i);
    vTaskDelay(3);
  }
}

/* 灯光控制 使用多线程
   缓慢持续闪烁未连接到手柄
   间隔快速双闪已连接到手柄
*/
void TaskStatusLight(void *pt)
{
  while (true)
  {
    if (xboxController.connected)
    {
      lightFast();
      vTaskDelay(100);
      lightFast();
      vTaskDelay(2000);
    }
    else
    {
      lightSlow();
      vTaskDelay(200);
      lightSlow();
      vTaskDelay(200);
    }
  }
}

void setup()
{
  Serial.begin(115200);
  xboxController.connect(NimBLEAddress("XXX"));

  // 初始化针脚
  pinMode(PIN_LIGHT, OUTPUT);
  pinMode(PIN_MOVE, OUTPUT);
  pinMode(PIN_MOVE_R, OUTPUT);
  pinMode(PIN_TURN, OUTPUT);
  pinMode(PIN_L_LIGHT, OUTPUT);
  pinMode(PIN_R_LIGHT, OUTPUT);

  ledcSetup(CHANNEL_LIGHT_L, 2000, 8);
  ledcAttachPin(33, CHANNEL_LIGHT_L);

  ledcSetup(CHANNEL_LIGHT_R, 2000, 8);
  ledcAttachPin(32, CHANNEL_LIGHT_R);

  xTaskCreate(TaskIndicatorLight, "Indicator Light", 1024, NULL, 1, NULL);
  xTaskCreate(TaskIndicatorLightControl, "IndicatorLightControl", 1024, NULL, 1, NULL);

  xTaskCreate(TaskStatusLight, "status light", 1024, NULL, 1, NULL);

  VehicleControlSetup(&xboxController, 3, PIN_TURN, 4, PIN_MOVE, PIN_MOVE_R);
}

void loop()
{
}