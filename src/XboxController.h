
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <XboxControllerNotificationParser.h>

class XboxController
{
private:
  static NimBLEAddress targetDeviceAddress;

public:
  static XboxControllerNotificationParser data;
  static bool scanning;
  static bool connected;
  void connect(NimBLEAddress);
  XboxControllerNotificationParser get(void);
};