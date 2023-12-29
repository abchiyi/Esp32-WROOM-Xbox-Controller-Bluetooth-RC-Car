#include <Arduino.h>

#include <VehicleLight.h>
#include <VehicleControl.h>
#include <XboxController.h>

XboxController xboxController;

// 定义控制 Pin
#define PIN_MOVE 32   // 移动控制
#define PIN_MOVE_R 33 // 倒车控制
#define PIN_TURN 12   // 转向控制

#define PIN_STATUSLIGHT 25 // 状态灯
#define CHANNEL_STATUSLIGHT 1

#define PIN_HEADLIGHT 21    // 大灯
#define CHANNEL_HEADLIGHT 6 // 大灯PWM通道

#define PIN_L_LIGHT 25 // 左转向灯
#define PIN_R_LIGHT 26 // 右转向灯
#define CHANNEL_LIGHT_L 2
#define CHANNEL_LIGHT_R 3

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

void setup()
{
  Serial.begin(115200);
  xboxController.connect(NimBLEAddress("XXX"));

  // 初始化针脚
  pinMode(PIN_MOVE, OUTPUT);
  pinMode(PIN_MOVE_R, OUTPUT);
  pinMode(PIN_TURN, OUTPUT);

  HeadlightSetup(PIN_HEADLIGHT, CHANNEL_HEADLIGHT);
  StatusLightSetup(PIN_STATUSLIGHT, CHANNEL_STATUSLIGHT);
  IndicatorLightSetup(&xboxController, 12, 12);
  VehicleControlSetup(&xboxController, 3, PIN_TURN, 4, PIN_MOVE, PIN_MOVE_R);
}

void loop()
{
}