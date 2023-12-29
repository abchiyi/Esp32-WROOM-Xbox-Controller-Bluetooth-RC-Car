
/* 车辆状态灯
   缓慢持续闪烁未连接到手柄
   间隔快速双闪已连接到手柄
 */
void StatusLightSetup(int pin, int pwmChannel);

/* 设置大灯，远光/近光/示宽灯
    灯光均由同一组LED灯通过不同亮度实现
 */
void HeadlightSetup(int pin, int pwmChannel);

/* 设置转向灯任务*/
void IndicatorLightSetup(void *controller, int pin, int pwmChannel);