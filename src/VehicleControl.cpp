/* 将摇杆输入值转换为角度并应用到舵机 */
#include <XboxController.h>
#include <ESP32Servo.h>

struct VehicleControlData
{
  XboxController *controller;
  int channelTurn;
  int channelMove;
  int prinMoveR;
  Servo *servo;
};

void TaskTurn(void *pt)
{
  const int width = 65535;
  const int deadZone = 4500;
  const int LStart = width / 2 - deadZone / 2;
  const int RStart = width / 2 + deadZone / 2;
  const int JoyLength = 256;

  VehicleControlData data = *(VehicleControlData *)pt;
  XboxController controller = *data.controller;
  Servo servo = *data.servo;
  int joy = 0;

  while (true)
  {
    joy = controller.get().joyLHori;
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
      servo.write(ang);
    }
    else // 在死区内时回正
    {
      servo.write(90);
    }

    vTaskDelay(1);
  }
}

void TaskMove(void *pt)
{

  VehicleControlData data = *(VehicleControlData *)pt;
  XboxController controller = *data.controller;

  while (true)
  {
    int LT = controller.get().trigLT;
    int RT = controller.get().trigRT;

    digitalWrite(data.prinMoveR, (bool)LT);                 // LT按下反转马达
    ledcWrite(data.channelMove, round((LT ? LT : RT) / 4)); // LT值优先
    vTaskDelay(1);
    // if (controller.connected)
    // {
    //   Serial.print("Move : ");
    //   Serial.print(round((LT ? LT : RT) / 4));
    //   Serial.print(", R : ");
    //   Serial.print((bool)LT);
    //   Serial.print(", fq : ");
    //   Serial.println(ledcRead(data.channelMove));
    // }
  }
}

Servo servo;

void VehicleControlSetup(XboxController *controller, int channelTurn, int servoPin, int channelMove, int motorPin, int motorPinR)
{

  ESP32PWM::allocateTimer(3);
  servo.setPeriodHertz(50);
  servo.attach(servoPin, 50, 2500);

  ledcSetup(channelMove, 2000, 8);
  ledcAttachPin(motorPin, channelMove);

  VehicleControlData data;
  data.channelMove = channelMove;
  data.prinMoveR = motorPinR;
  data.channelTurn = channelTurn;
  data.controller = controller;
  data.servo = &servo;

  servo.write(90);

  xTaskCreate(TaskTurn, "Turn", 2048, (void *)&data, 2, NULL);
  xTaskCreate(TaskMove, "Move", 2048, (void *)&data, 2, NULL);
}