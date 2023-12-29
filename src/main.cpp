#include <Arduino.h>

#include <VehicleLight.h>
#include <VehicleControl.h>
#include <XboxController.h>

XboxController Controller;

// 定义控制 Pin
#define PIN_MOVE 32   // 移动控制
#define PIN_MOVE_R 33 // 倒车控制
#define PIN_TURN 12   // 转向控制

#define PIN_STATUSLIGHT 25 // 状态灯
#define CHANNEL_STATUSLIGHT 1

#define PIN_HEADLIGHT 21    // 大灯
#define CHANNEL_HEADLIGHT 6 // 大灯PWM通道

#define PIN_STOPLIGHT 22    // 刹车灯
#define CHANNEL_STOPLIGHT 7 // 刹车灯PWM通道

#define PIN_L_LIGHT 25 // 左转向灯
#define PIN_R_LIGHT 26 // 右转向灯
#define CHANNEL_LIGHT_L 2
#define CHANNEL_LIGHT_R 3

bool DISTANT_LIGHT = false; // 远光
bool LOW_BEAM = false;      // 近光
bool WIDTH_LAMP = false;    // 示宽灯
int HeadLight = 0;          // 大灯亮度
int StopLight = 0;          // 大灯亮度

struct ButtonListenData
{
  bool *value;
  bool *button;
};

/* 灯光任务 */
void TaskLight(void *pt)
{
  while (true)
  {
    ledcWrite(CHANNEL_HEADLIGHT, HeadLight);
    ledcWrite(CHANNEL_STOPLIGHT, StopLight);
    vTaskDelay(20);
  }
}

/* 灯光控制任务 */
void TaskLightControll(void *pt)
{
  bool changed = false;
  int counter = 0;
  bool *DirUp = &Controller.data.btnDirUp;
  bool *DirDown = &Controller.data.btnDirDown;
  bool *BtnY = &Controller.data.btnY; // 按下临时打开远光灯
  bool *BtnA = &Controller.data.btnA; // 按下打开远光灯

  bool KEEP_DISTANT_LIGHT = false; // 保持远光开启

  while (true)
  {
    vTaskDelay(20);
    if ((*DirUp || *DirDown || *BtnY) && Controller.connected)
    {
      if (!changed)
      {
        *DirUp     ? counter++
        : *DirDown ? counter--
                   : counter;

        counter = counter > 2   ? 2
                  : counter < 0 ? 0
                                : counter;

        // 按下 Y 按键切换远/近光
        KEEP_DISTANT_LIGHT = !changed && *BtnY ? !KEEP_DISTANT_LIGHT : KEEP_DISTANT_LIGHT;

        changed = true;
      }
    }
    else
    {
      changed = false;
    }

    if (!*BtnA) // 仅在 A 按钮按下时开启远光
    {
      switch (counter)
      {
      case 0: // 关闭灯光
        LOW_BEAM = false;
        WIDTH_LAMP = false;
        DISTANT_LIGHT = false;
        KEEP_DISTANT_LIGHT = false;
        break;
      case 1: // 示宽灯
        WIDTH_LAMP = true;
        // 关闭远/近光
        KEEP_DISTANT_LIGHT = false;
        DISTANT_LIGHT = false;
        LOW_BEAM = false;
        break;
      case 2: // 示宽灯 近光
        LOW_BEAM = true;
        WIDTH_LAMP = true;
        break;
      }
      DISTANT_LIGHT = KEEP_DISTANT_LIGHT;
    }
    else
    {
      DISTANT_LIGHT = true;
      KEEP_DISTANT_LIGHT = false;
    }

    HeadLight = DISTANT_LIGHT ? 255
                : LOW_BEAM    ? 100
                : WIDTH_LAMP  ? 30
                              : 0;

    StopLight = WIDTH_LAMP ? 30 : 0;
  }
}

/* 设置大灯 */
void LightSetup()
{

  pinMode(PIN_HEADLIGHT, OUTPUT);
  ledcSetup(CHANNEL_HEADLIGHT, 2000, 8);
  ledcAttachPin(PIN_HEADLIGHT, CHANNEL_HEADLIGHT);

  pinMode(PIN_STOPLIGHT, OUTPUT);
  ledcSetup(CHANNEL_STOPLIGHT, 2000, 8);
  ledcAttachPin(PIN_STOPLIGHT, CHANNEL_STOPLIGHT);

  xTaskCreate(TaskLight, "TaskHeadlight", 1024, NULL, 3, NULL);
  xTaskCreate(TaskLightControll, "TaskHeadlightControll", 1024, NULL, 3, NULL);
}

void setup()
{
  Serial.begin(115200);
  Controller.connect(NimBLEAddress("XXX"));

  // 初始化针脚
  pinMode(PIN_MOVE, OUTPUT);
  pinMode(PIN_MOVE_R, OUTPUT);
  pinMode(PIN_TURN, OUTPUT);

  StatusLightSetup(PIN_STATUSLIGHT, CHANNEL_STATUSLIGHT);
  IndicatorLightSetup(&Controller, 12, 12);
  LightSetup();
  VehicleControlSetup(&Controller, 3, PIN_TURN, 4, PIN_MOVE, PIN_MOVE_R);
}

void loop()
{
}