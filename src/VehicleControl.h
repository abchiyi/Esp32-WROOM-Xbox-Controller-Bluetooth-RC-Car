/*
 @param controller 控制器对象
 @param channelTurn 转向舵机 pwm 通道
 @param servoPin 舵机驱动引脚
 @param channelMove 驱动马达 pwm 通道
 @param motorPin 驱动电机控制引脚
 @param motorPinR 驱动电机反转控制引脚
 */
void VehicleControlSetup(void *controller, int channelTurn, int servoPin, int channelMove, int motorPin, int motorPinR);