#include <Arduino.h>
#include <ESP32Servo.h>
#include <XboxController.h>

Servo TurnServo;
XboxController Controller;

// 定义控制 Pin
#define PIN_MOVE 17    // 移动控制
#define PIN_MOVE_R 16  // 倒车控制
#define CHANNEL_MOVE 4 // 马达驱动 pwm 通道
#define PIN_TURN 18    // 转向控制

#define PIN_HEADLIGHT 21       // 大灯
#define PIN_STOPLIGHT 27       // 刹车灯
#define PIN_STATUSLIGHT 23     // 状态灯
#define PIN_REVERSING_LIGHT 26 // 倒车灯
#define PIN_L_LIGHT 25         // 左转向灯
#define PIN_R_LIGHT 33         // 右转向灯

// PWM 通道
#define CHANNEL_LIGHT_L 2         // 左转向灯
#define CHANNEL_LIGHT_R 3         // 右转向灯
#define CHANNEL_HEADLIGHT 6       // 大灯PWM通道
#define CHANNEL_STOPLIGHT 7       // 刹车灯PWM通道
#define CHANNEL_STATUSLIGHT 1     // 状态灯
#define CHANNEL_REVERSING_LIGHT 8 // 倒车灯PWM通道

bool DISTANT_LIGHT = false; // 远光
bool LOW_BEAM = false;      // 近光
bool WIDTH_LAMP = false;    // 示宽灯
int REVERSING_LIGHT = 0;    // 倒车灯
int HeadLight = 0;          // 大灯   亮度0~255
int StopLight = 0;          // 刹车灯 亮度0~255
bool BRAKE = false;         // 刹车
bool LightTurnL = false;    // 左转灯
bool LightTurnR = false;    // 右转灯
bool HazardLight = false;   // 危险报警灯

/* 设置 pwm 输出引脚 */
void setPWMPin(int pin, int pwmChannel)
{
  pinMode(pin, OUTPUT);
  ledcSetup(pwmChannel, 2000, 8);
  ledcAttachPin(pin, pwmChannel);
}

// 移动任务
void TaskMove(void *pt)
{
  bool HOLD_RT = false;
  bool HOLD_LT = false;
  bool changed = false;

  while (true)
  {
    int LT = Controller.data.trigLT;
    int RT = Controller.data.trigRT;
    // 确定前进方向;
    if (LT || RT)
    {
      if (!changed)
      {
        HOLD_RT = (bool)RT;
        HOLD_LT = !HOLD_RT ? (bool)LT : false;
        changed = true;
      }
    }
    else
    {
      HOLD_RT = false;
      HOLD_LT = false;
      changed = false;
    }
    // 当所控制按钮的值发生改变时，重设change以在下个循环重新计算前进方向
    if (HOLD_LT != (bool)LT && HOLD_RT != (bool)RT)
    {
      changed = false;
    }

    BRAKE = LT && RT;                    // 两个扳机键同时按下开启刹车
    REVERSING_LIGHT = HOLD_LT ? 150 : 0; // 倒车灯

    if (BRAKE) // 刹车激活时对电机施加一个较小的反向电压
    {
      digitalWrite(PIN_MOVE_R, HOLD_RT);
      ledcWrite(CHANNEL_MOVE, 30);
    }
    else
    {
      int value = round((HOLD_RT ? RT : LT) / 4);
      digitalWrite(PIN_MOVE_R, (bool)REVERSING_LIGHT); // R/F
      ledcWrite(CHANNEL_MOVE, value);                  // motor
    }

    vTaskDelay(1);
    // if (Controller.connected)
    // {
    //   Serial.print("Move : ");
    //   Serial.print(round((LT ? LT : RT) / 4));
    //   Serial.print(", R : ");
    //   Serial.print(digitalRead(PIN_MOVE_R));
    //   Serial.print(", fq : ");
    //   Serial.print(ledcRead(CHANNEL_MOVE));
    //   Serial.print(", H RT ");
    //   Serial.print(HOLD_RT);
    //   Serial.print(", H LT ");
    //   Serial.println(HOLD_LT);
    // }
  }
}

// 转向任务
void TaskTurn(void *pt)
{
  const int width = 65535;
  const int deadZone = 4500;
  const int LStart = width / 2 - deadZone / 2;
  const int RStart = width / 2 + deadZone / 2;
  const int JoyLength = 256;

  int joy = 0;

  while (true)
  {
    joy = Controller.data.joyLHori;

    // joy 的值不在死区范围内时执行转向动作
    if (joy <= LStart || joy >= RStart)
    {
      // 计算打杆量
      int t_joy = joy <= LStart ? joy : joy - deadZone;
      int value = round(double(t_joy) * (double(JoyLength) / double(LStart)));

      // 转换杆量为角度
      double step = 90.000 / double(JoyLength);
      int ang = 180 - value * step;

      // 写入角度
      TurnServo.write(ang);
    }
    else // 在死区内时回正
    {
      TurnServo.write(90);
    }

    vTaskDelay(1);
  }
}

/* 车辆操控设置 转向，移动 */
void VehicleControlSetup()
{

  // 电调设置
  pinMode(PIN_MOVE_R, OUTPUT);       // F/R
  setPWMPin(PIN_MOVE, CHANNEL_MOVE); // motor

  // 设置舵机
  pinMode(PIN_TURN, OUTPUT);
  TurnServo.setPeriodHertz(50);
  TurnServo.attach(PIN_TURN, 50, 2500);

  xTaskCreate(TaskTurn, "Turn", 2048, NULL, 2, NULL);
  xTaskCreate(TaskMove, "Move", 2048, NULL, 2, NULL);
}

/* 灯光任务 大灯，倒车灯，刹车灯*/
void TaskLight(void *pt)
{
  while (true)
  {
    ledcWrite(CHANNEL_HEADLIGHT, HeadLight);
    ledcWrite(CHANNEL_STOPLIGHT, StopLight);
    ledcWrite(CHANNEL_REVERSING_LIGHT, REVERSING_LIGHT);
    vTaskDelay(20);
  }
}

/* 灯光控制任务控制 */
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

    StopLight = BRAKE        ? 255
                : WIDTH_LAMP ? 30
                             : 0;

    // REVERSING_LIGHT = REVERSING_LIGHT;
  }
}

/* 状态灯任务 */
void TaskStatusLight(void *pt)
{
  int pwmChannel = CHANNEL_STATUSLIGHT;
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

    btnLB = Controller.data.btnLB;
    btnRB = Controller.data.btnRB;

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

/* 设置灯光任务 */
void LightSetup()
{

  setPWMPin(PIN_R_LIGHT, CHANNEL_LIGHT_R);
  setPWMPin(PIN_L_LIGHT, CHANNEL_LIGHT_L);
  setPWMPin(PIN_HEADLIGHT, CHANNEL_HEADLIGHT);
  setPWMPin(PIN_STOPLIGHT, CHANNEL_STOPLIGHT);
  setPWMPin(PIN_STATUSLIGHT, CHANNEL_STATUSLIGHT);
  setPWMPin(PIN_REVERSING_LIGHT, CHANNEL_REVERSING_LIGHT);

  xTaskCreate(TaskLight, "TaskHeadlight", 1024, NULL, 3, NULL);
  xTaskCreate(TaskStatusLight, "TaskStatusLight", 1024, NULL, 3, NULL);
  xTaskCreate(TaskLightControll, "TaskHeadlightControll", 1024, NULL, 3, NULL);
  xTaskCreate(TaskIndicatorLight, "Indicator Light", 1024, NULL, 3, NULL);
  xTaskCreate(TaskIndicatorLightControl, "IndicatorLightControl", 1024, NULL, 3, NULL);
}

void setup()
{
  Serial.begin(115200);
  Controller.connect(NimBLEAddress("XXX"));

  LightSetup();
  VehicleControlSetup();
}

void loop()
{
}