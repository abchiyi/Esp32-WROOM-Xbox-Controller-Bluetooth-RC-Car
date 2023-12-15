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

const int width = 65535;
const int deadZone = 4500;
const int LStart = width / 2 - deadZone / 2;
const int RStart = width / 2 + deadZone / 2;
const int JoyLength = 256;

bool LightTurnL = false;
bool LightTurnR = false;
bool HazardLight = false;
SemaphoreHandle_t xMutexIndicatorLight;

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
  bool value = false;
  while (true)
  {

    LightTurnL || LightTurnR || HazardLight
        ? vTaskDelay(400) // 灯光闪烁
        : vTaskDelay(20); // 快速轮询是否开启灯光

    LightTurnL || HazardLight // 左转向灯
        ? digitalWrite(PIN_L_LIGHT, value)
        : digitalWrite(PIN_L_LIGHT, 0);

    LightTurnR || HazardLight // 右转向灯
        ? digitalWrite(PIN_R_LIGHT, value)
        : digitalWrite(PIN_R_LIGHT, 0);

    value = !value; // 反转灯光
  }
}

void TaskIndicatorLightControl(void *pt)
{
  bool both;
  bool changed = false;
  while (true)
  {
    both = xboxController.data.btnLB && xboxController.data.btnRB;

    if (both)
    {
      LightTurnR = false;
      LightTurnL = false;
      HazardLight = changed ? HazardLight : !HazardLight;
      changed = true;
    }
    else if (xboxController.data.btnLB && !HazardLight)
    {
      if (xSemaphoreTake(xMutexIndicatorLight, 1000))
      {
        LightTurnL = changed ? LightTurnL : !LightTurnL;
        LightTurnR = false;
        HazardLight = false;
        changed = true;
        xSemaphoreGive(xMutexIndicatorLight);
      }
    }
    else if (xboxController.data.btnRB && !HazardLight)
    {
      if (xSemaphoreTake(xMutexIndicatorLight, 1000))
      {

        LightTurnR = changed ? LightTurnR : !LightTurnR;
        LightTurnL = false;
        HazardLight = false;
        changed = true;
        xSemaphoreGive(xMutexIndicatorLight);
      }
    }
    else
    {
      changed = false;
    }
    // Serial.print("R :");
    // Serial.print(LightTurnR);
    // Serial.print(" L :");
    // Serial.print(LightTurnL);
    // Serial.print(" H :");
    // Serial.print(HazardLight);
    // Serial.print(" B :");
    // Serial.print(both);
    // Serial.print(" ch :");
    // Serial.println(changed);
    vTaskDelay(5);
  }
}

void lightFast()
{
  analogWrite(PIN_LIGHT, 255);
  vTaskDelay(100);
  analogWrite(PIN_LIGHT, 0);
}

void lightSlow()
{
  for (int i = 0; i <= 255; i = i + 5)
  {
    analogWrite(PIN_LIGHT, i);
    vTaskDelay(3);
  }
  vTaskDelay(100);

  for (int i = 255; i >= 0; i = i - 5)
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

  xMutexIndicatorLight = xSemaphoreCreateMutex();

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

  // /*  LB  */
  // ButtonListenData LB;
  // LB.value = (bool *)&LightTurnL;
  // LB.button = (bool *)&xboxController.data.btnLB;
  // LB.mutex = (SemaphoreHandle_t *)&xMutexIndicatorLight;
  // xTaskCreatePinnedToCore(TaskButtonListen, "Listen LB", 1024, (void *)&LB, 3, NULL, 1);

  // /*  RB  */
  // ButtonListenData RB;
  // RB.value = (bool *)&LightTurnR;
  // RB.button = (bool *)&xboxController.data.btnRB;
  // RB.mutex = (SemaphoreHandle_t *)&xMutexIndicatorLight;
  // xTaskCreatePinnedToCore(TaskButtonListen, "Listen RB", 1024, (void *)&RB, 3, NULL, 1);
  xTaskCreate(TaskIndicatorLight, "Indicator Light", 1024, NULL, 1, NULL);
  xTaskCreate(TaskIndicatorLightControl, "IndicatorLightControl", 1024, NULL, 1, NULL);

  xTaskCreate(TaskStatusLight, "status light", 1024, NULL, 1, NULL);
}

void loop()
{
  VehicleControl();
}