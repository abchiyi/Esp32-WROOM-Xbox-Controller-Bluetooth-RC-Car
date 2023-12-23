#include <Arduino.h>

#include <ESP32Servo.h>
#include <NimBLEDevice.h>
#include <XboxController.h>

XboxController xboxController;
Servo servo;

// 定义控制 Pin
#define PIN_MOVE 12    // 移动控制
#define PIN_MOVE_R 27  // 倒车控制
#define PIN_TURN 26    // 转向控制
#define PIN_LIGHT 25   // 状态灯
#define PIN_L_LIGHT 33 // 左转向灯
#define PIN_R_LIGHT 32 // 右转向灯
#define CHANNEL_LIGHT_L 1
#define CHANNEL_LIGHT_R 2

const int width = 65535;
const int deadZone = 4500;
const int LStart = width / 2 - deadZone / 2;
const int RStart = width / 2 + deadZone / 2;
const int JoyLength = 256;

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

    // Serial.print("R :");
    // Serial.print(LightTurnR);
    // Serial.print(" Old R :");
    // Serial.print(btnLB_old);
    // Serial.print(" L :");
    // Serial.print(LightTurnL);
    // Serial.print(" Old L :");
    // Serial.print(btnLB_old);
    // Serial.print(" H :");
    // Serial.print(HazardLight);
    // Serial.print(" B :");
    // Serial.print(btnLB && btnRB);
    // Serial.print(" ch :");
    // Serial.println(changed);
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

/* 将摇杆输入值转换为角度并应用到舵机 */
void Turn(int joy, bool reverse = false)
{

  // 计算打杆量
  int t_joy = joy <= LStart ? joy : joy - deadZone;
  int value = round(double(t_joy) * (double(JoyLength) / double(LStart)));

  // 转换杆量为角度
  double step = 90.000 / double(JoyLength);
  int ang = 180 - value * step;

  // 写入角度
  servo.write(ang);
}

/* 验证要写入的参数是否已经写入过了 */
void ifAnalogWrite(uint8_t pin, int value)
{
  static int oldValue = 0;
  if (value != oldValue)
  {
    analogWrite(pin, value);
    oldValue = value;
  }
}

/* 车辆移动
  @param reverse 后退
  @param stop 停止
  @trig 油门控制
*/
void Move(int trig, bool reverse, bool stop)
{

  if (!stop)
  {
    ifAnalogWrite(PIN_MOVE, round(trig / 4));
    reverse
        ? digitalWrite(PIN_MOVE_R, 1)
        : digitalWrite(PIN_MOVE_R, 0);
  }
  else
  {
    ifAnalogWrite(PIN_MOVE, 0);
    digitalWrite(PIN_MOVE_R, 0);
  }
}

/* 车辆控制 */
void VehicleControl()
{
  if (xboxController.connected)
  {
    auto controller = xboxController.get();

    const int LT = xboxController.get().trigLT;
    const int RT = xboxController.get().trigRT;

    LT   ? Move(LT, true, false)  // 倒车
    : RT ? Move(RT, false, false) // 前进
         : Move(0, false, true);  // 停止

    // 执行转向动作
    const int joy = controller.joyLHori;
    joy <= LStart   ? Turn(joy)     // 从左摇杆起始值计算旋转90°~0°
    : joy >= RStart ? Turn(joy)     // 从右摇杆起始值计算旋转90°~180°
                    : Turn(LStart); // 复位至90°
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
  // 转向设置
  servo.setPeriodHertz(50);
  servo.attach(PIN_TURN, 50, 2500);

  ledcSetup(CHANNEL_LIGHT_L, 2000, 8);
  ledcAttachPin(33, CHANNEL_LIGHT_L);

  ledcSetup(CHANNEL_LIGHT_R, 2000, 8);
  ledcAttachPin(32, CHANNEL_LIGHT_R);

  xTaskCreate(TaskIndicatorLight, "Indicator Light", 1024, NULL, 1, NULL);
  xTaskCreate(TaskIndicatorLightControl, "IndicatorLightControl", 1024, NULL, 1, NULL);

  xTaskCreate(TaskStatusLight, "status light", 1024, NULL, 1, NULL);
}

void loop()
{
  VehicleControl();
}