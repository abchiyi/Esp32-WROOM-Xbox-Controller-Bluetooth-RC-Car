#include <Arduino.h>

#include <math.h>
#include <ESP32Servo.h>
#include <NimBLEDevice.h>
#include <XboxControllerNotificationParser.h>

XboxControllerNotificationParser xboxNotif;
Servo servo;

void scanEndedCB(NimBLEScanResults results);

static NimBLEAdvertisedDevice *advDevice;

bool scanning = false;
bool connected = false;
static uint32_t scanTime = 0; /** 0 = scan forever */

static NimBLEAddress targetDeviceAddress("98:7A:14:29:10:41"); // 手柄蓝牙地址

static NimBLEUUID uuidServiceGeneral("1801");
static NimBLEUUID uuidServiceBattery("180f");
static NimBLEUUID uuidServiceHid("1812");
static NimBLEUUID uuidCharaReport("2a4d");
static NimBLEUUID uuidCharaPnp("2a50");
static NimBLEUUID uuidCharaHidInformation("2a4a");
static NimBLEUUID uuidCharaPeripheralAppearance("2a01");
static NimBLEUUID uuidCharaPeripheralControlParameters("2a04");

// 定义控制 Pin
#define PIN_MOVE 12   // 移动控制
#define PIN_MOVE_R 27 // 倒车控制
#define PIN_TURN 26   // 转向控制
#define PIN_LIGHT 25  // 状态灯
int stk_l;
bool stk_l_init;

const int width = 65535;
const int deadZone = 4500;
const int LStart = width / 2 - deadZone / 2;
const int RStart = width / 2 + deadZone / 2;
const int JoyLength = 256;

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
void LightTask(void *pt)
{

  while (true)
  {
    if (connected)
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

  Serial.print("ANG :");
  Serial.println(ang);
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
    analogWrite(PIN_MOVE, round(trig / 4));
    reverse
        ? digitalWrite(PIN_MOVE_R, 1)
        : digitalWrite(PIN_MOVE_R, 0);
  }
  else
  {
    analogWrite(PIN_MOVE, 0);
    digitalWrite(PIN_MOVE_R, 0);
  }
}

/* 车辆控制 */
void VehicleControl(uint8_t *pData, size_t length)
{
  xboxNotif.update(pData, length);
  Serial.print("Width : ");
  Serial.print(width);
  Serial.print(" LStart :");
  Serial.print(LStart);
  Serial.print(" RStart :");
  Serial.print(RStart);
  Serial.print(" deadZone :");
  Serial.println(deadZone);
  Serial.print("JOY :");
  Serial.println(xboxNotif.joyLHori);

  const int LT = xboxNotif.trigLT;
  const int RT = xboxNotif.trigRT;

  LT   ? Move(LT, true, false)  // 倒车
  : RT ? Move(RT, false, false) // 前进
       : Move(0, false, true);  // 停止

  // 执行转向动作
  const int joy = xboxNotif.joyLHori;
  joy <= LStart   ? Turn(joy)     // 从左摇杆起始值计算旋转90°~0°
  : joy >= RStart ? Turn(joy)     // 从右摇杆起始值计算旋转90°~180°
                  : Turn(LStart); // 复位至90°
}

class ClientCallbacks : public NimBLEClientCallbacks
{
  void onConnect(NimBLEClient *pClient)
  {
    Serial.println("Connected");
    connected = true;
    // pClient->updateConnParams(120,120,0,60);
  };

  void onDisconnect(NimBLEClient *pClient)
  {
    Serial.print(pClient->getPeerAddress().toString().c_str());
    Serial.println(" Disconnected");
    connected = false;
  };

  /** Called when the peripheral requests a change to the connection parameters.
   *  Return true to accept and apply them or false to reject and keep
   *  the currently used parameters. Default will return true.
   */
  bool onConnParamsUpdateRequest(NimBLEClient *pClient,
                                 const ble_gap_upd_params *params)
  {
    Serial.print("onConnParamsUpdateRequest");
    if (params->itvl_min < 24)
    { /** 1.25ms units */
      return false;
    }
    else if (params->itvl_max > 40)
    { /** 1.25ms units */
      return false;
    }
    else if (params->latency > 2)
    { /** Number of intervals allowed to skip */
      return false;
    }
    else if (params->supervision_timeout > 100)
    { /** 10ms units */
      return false;
    }

    return true;
  };

  /********************* Security handled here **********************
  ****** Note: these are the same return values as defaults ********/
  uint32_t onPassKeyRequest()
  {
    Serial.println("Client Passkey Request");
    /** return the passkey to send to the server */
    return 0;
  };

  bool onConfirmPIN(uint32_t pass_key)
  {
    Serial.print("The passkey YES/NO number: ");
    Serial.println(pass_key);
    /** Return false if passkeys don't match. */
    return true;
  };

  /** Pairing process complete, we can check the results in ble_gap_conn_desc */
  void onAuthenticationComplete(ble_gap_conn_desc *desc)
  {
    Serial.println("onAuthenticationComplete");
    if (!desc->sec_state.encrypted)
    {
      Serial.println("Encrypt connection failed - disconnecting");
      /** Find the client with the connection handle provided in desc */
      NimBLEDevice::getClientByID(desc->conn_handle)->disconnect();
      return;
    }
  };
};

/** Define a class to handle the callbacks when advertisments are received */
class AdvertisedDeviceCallbacks : public NimBLEAdvertisedDeviceCallbacks
{
  void onResult(NimBLEAdvertisedDevice *advertisedDevice)
  {
    Serial.print("Advertised Device found: ");
    Serial.println(advertisedDevice->toString().c_str());
    Serial.printf("name:%s, address:%s\n", advertisedDevice->getName().c_str(),
                  advertisedDevice->getAddress().toString().c_str());
    Serial.printf("uuidService:%s\n",
                  advertisedDevice->haveServiceUUID()
                      ? advertisedDevice->getServiceUUID().toString().c_str()
                      : "none");

    if (advertisedDevice->getAddress().equals(targetDeviceAddress))
    // if (advertisedDevice->isAdvertisingService(uuidServiceHid))
    {
      Serial.println("Found Our Service");
      /** stop scan before connecting */
      NimBLEDevice::getScan()->stop();
      /** Save the device reference in a global for the client to use*/
      advDevice = advertisedDevice;
    }
  };
};

unsigned long printInterval = 100UL;

/** Notification / Indication receiving handler callback */
void notifyCB(NimBLERemoteCharacteristic *pRemoteCharacteristic, uint8_t *pData,
              size_t length, bool isNotify)
{
  VehicleControl(pData, length);
}

void scanEndedCB(NimBLEScanResults results)
{
  Serial.println("Scan Ended");
  scanning = false;
}

static ClientCallbacks clientCB;

void charaPrintId(NimBLERemoteCharacteristic *pChara)
{
  Serial.printf("s:%s c:%s h:%d",
                pChara->getRemoteService()->getUUID().toString().c_str(),
                pChara->getUUID().toString().c_str(), pChara->getHandle());
}

void printValue(std::__cxx11::string str)
{
  Serial.printf("str: %s\n", str.c_str());
  Serial.printf("hex:");
  for (auto v : str)
  {
    Serial.printf(" %02x", v);
  }
  Serial.println("");
}

void charaRead(NimBLERemoteCharacteristic *pChara)
{
  if (pChara->canRead())
  {
    charaPrintId(pChara);
    Serial.println(" canRead");
    auto str = pChara->readValue();
    if (str.size() == 0)
    {
      str = pChara->readValue();
    }
    printValue(str);
  }
}

void charaSubscribeNotification(NimBLERemoteCharacteristic *pChara)
{
  if (pChara->canNotify())
  {
    charaPrintId(pChara);
    Serial.println(" canNotify ");
    if (pChara->subscribe(true, notifyCB, true))
    {
      Serial.println("set notifyCb");
      // return true;
    }
    else
    {
      Serial.println("failed to subscribe");
    }
  }
}

bool afterConnect(NimBLEClient *pClient)
{
  for (auto pService : *pClient->getServices(true))
  {
    auto sUuid = pService->getUUID();
    if (!sUuid.equals(uuidServiceHid))
    {
      continue; // skip
    }
    Serial.println(pService->toString().c_str());
    for (auto pChara : *pService->getCharacteristics(true))
    {
      charaRead(pChara);
      charaSubscribeNotification(pChara);
    }
  }

  return true;
}

/** Handles the provisioning of clients and connects / interfaces with the
 * server */
bool connectToServer(NimBLEAdvertisedDevice *advDevice)
{
  NimBLEClient *pClient = nullptr;

  /** Check if we have a client we should reuse first **/
  if (NimBLEDevice::getClientListSize())
  {
    pClient = NimBLEDevice::getClientByPeerAddress(advDevice->getAddress());
    if (pClient)
    {
      pClient->connect();
    }
  }

  /** No client to reuse? Create a new one. */
  if (!pClient)
  {
    if (NimBLEDevice::getClientListSize() >= NIMBLE_MAX_CONNECTIONS)
    {
      Serial.println("Max clients reached - no more connections available");
      return false;
    }

    pClient = NimBLEDevice::createClient();

    Serial.println("New client created");

    pClient->setClientCallbacks(&clientCB, false);
    // pClient->setConnectionParams(12, 12, 0, 51);
    pClient->setConnectTimeout(5);
    pClient->connect(advDevice, false);
  }

  int retryCount = 5;
  while (!pClient->isConnected())
  {
    if (retryCount <= 0)
    {
      return false;
    }
    else
    {
      Serial.println("try connection again " + String(millis()));
      delay(1000);
    }

    // NimBLEDevice::getScan()->stop();
    // pClient->disconnect();
    delay(500);
    // Serial.println(pClient->toString().c_str());
    pClient->connect(true);
    --retryCount;
  }

  Serial.print("Connected to: ");
  Serial.println(pClient->getPeerAddress().toString().c_str());
  Serial.print("RSSI: ");
  Serial.println(pClient->getRssi());

  // pClient->discoverAttributes();

  bool result = afterConnect(pClient);
  if (!result)
  {
    return result;
  }

  Serial.println("Done with this device!");
  return true;
}

void startScan()
{
  scanning = true;
  auto pScan = NimBLEDevice::getScan();
  pScan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks());
  pScan->setInterval(45);
  pScan->setWindow(15);
  Serial.println("Start scan");
  pScan->start(scanTime, scanEndedCB);
}

void setup()
{
  Serial.begin(115200);
  Serial.println("Starting NimBLE Client");
  /** Initialize NimBLE, no device name spcified as we are not advertising */
  NimBLEDevice::init("");
  NimBLEDevice::setOwnAddrType(BLE_OWN_ADDR_PUBLIC);
  NimBLEDevice::setSecurityAuth(true, true, true);
  NimBLEDevice::setPower(ESP_PWR_LVL_P9); /** +9db */

  // 初始化针脚
  pinMode(PIN_LIGHT, OUTPUT);
  pinMode(PIN_MOVE, OUTPUT);
  pinMode(PIN_MOVE_R, OUTPUT);
  pinMode(PIN_TURN, OUTPUT);
  stk_l_init = false;
  xTaskCreate(LightTask, "status light", 1024, NULL, 1, NULL);

  // 转向设置
  servo.setPeriodHertz(50);
  servo.attach(PIN_TURN, 50, 2500);
}

void loop()
{
  if (!connected)
  {
    if (advDevice != nullptr)
    {
      if (connectToServer(advDevice))
      {
        Serial.println("Success! we should now be getting notifications");
      }
      else
      {
        Serial.println("Failed to connect");
      }
      advDevice = nullptr;
    }
    else if (!scanning)
    {
      startScan();
    }
  }

  // Serial.println("scanning:" + String(scanning) + " connected:" + String(connected) + " advDevice is nullptr:" + String(advDevice == nullptr));
  delay(2000);
}