#include <Arduino.h>
#include <Arduino_FreeRTOS.h>
#include <Wire.h>
#include <semphr.h>
#include <SPI.h>
#include <LM35.h>
#include <LiquidCrystal.h>


int POWER_PIN = 13;//板载黄灯
int heatSwitch = 3;
int heatLED = 8;//红灯
int Sounder = 9;//无源蜂鸣器
int ENDLED = 7;//绿灯
int waterLED = 6;//蓝灯

LiquidCrystal lcd(12, 11, 5, 4, 10, 2);
LM35 lm(A0);

SemaphoreHandle_t heatInterrupt; //控制按钮加热的二值信号量
SemaphoreHandle_t heatLEDOn;     //加热灯亮
SemaphoreHandle_t autoPouring;//自动加水二值信号量
SemaphoreHandle_t heatEnd; //加热结束任务二值信号量
SemaphoreHandle_t heatProcess;//加热过程状态显示二值信号量
//任务句柄
TaskHandle_t readSerialHandler;
TaskHandle_t startTaskHandler;
TaskHandle_t heatLEDOnTaskHandler;
TaskHandle_t heatTaskHandler;
TaskHandle_t autoPouringHandler;
TaskHandle_t heatEndTaskHandler;
TaskHandle_t heatProcessTaskHandler;

void doSwitch(char message); //做判断，判断输入的值做出相应行为
void heatHandler();
void heatLEDOnHandler();
void heatProcessHandler();
void heatEndHandler();

void readSerial(void *pvParameters);//读串口任务
void startTask(void *pvParameters);//开始任务
void heatTask(void *pvParameters); //加热任务
void heatLEDOnTask(void *pvParameters);//加热任务
void autoPouringTask(void *pvParameters);//自动注水任务
//void heatEndTask(void *pvParameters);//加热结束任务
void heatProcessTask(void *pvParameters);//加热过程显示状态任务

int LiquidTemper = 100; //液体沸腾温度，默认烧水
int temperature = 0;//水温
int water = 60;//容量
int cur_water = 0;//当前水位
int auto_water = 0;//自动注水判断
char message = 'L';//传回信息

void setup()
{
  // put your setup code here, to run once:
  pinMode(heatLED, OUTPUT);
  pinMode(ENDLED, OUTPUT);
  pinMode(Sounder, OUTPUT);
  pinMode(heatSwitch, OUTPUT);
  pinMode(POWER_PIN, OUTPUT);

  digitalWrite(heatSwitch, LOW);
  digitalWrite(POWER_PIN, LOW);
  digitalWrite(heatLED, LOW);
  digitalWrite(ENDLED, LOW);
  //初始化液晶屏
  lcd.begin(16, 2);
  lcd.setCursor(4, 0);
  lcd.print("544 TECH");
  //启动串口
  Serial.begin(9600);

  //若温度已达沸点，输出
  if ((lm.cel() >= LiquidTemper))
    Serial.println("Already!");
  //创建开始任务
  xTaskCreate(startTask, "startTask", 64, NULL, 1, &startTaskHandler);
  vTaskStartScheduler(); //开始任务调度
}
void loop()
{
  // put your main code here, to run repeatedly:
}

void startTask(void *pvParameters)
{
  taskENTER_CRITICAL(); //进入临界区

  //读串口
  xTaskCreate(readSerial, "readSerial", 64, NULL, 2, &readSerialHandler);

  heatLEDOn = xSemaphoreCreateBinary(); //加热灯信号量
  heatInterrupt = xSemaphoreCreateBinary();
  autoPouring = xSemaphoreCreateBinary();
  heatEnd = xSemaphoreCreateBinary();
  heatProcess = xSemaphoreCreateBinary();

  xTaskCreate(heatLEDOnTask, "heatLEDOnTask", 64, NULL, 4, &heatLEDOnTaskHandler);
  //启动加热——中断方式,创建加热灯任务
  xTaskCreate(heatTask, "heatTask", 64, NULL, 4, &heatTaskHandler);
  //手动板开关启动加热
  if (heatInterrupt != NULL)
  {
    //如果中断被触发，运行heatTaskHandler
    attachInterrupt(digitalPinToInterrupt(heatSwitch), heatHandler, LOW);
  }
  //创建自动注水任务
  xTaskCreate(autoPouringTask, "autoPouringTask", 64, NULL, 3, &autoPouringHandler);
  //创建加热过程任务
  xTaskCreate(heatProcessTask, "heatProcessTask", 128, NULL, 1, NULL);
  //删除开始任务
  vTaskDelete(startTaskHandler);
  taskEXIT_CRITICAL();
}

/**
 * 读串口
 * **/
void readSerial(void *pvParameters)
{
  (void)pvParameters;
  pinMode(heatLED, OUTPUT);
  int num = 0;
  for (;;)
  {
    num += 1;
    temperature = lm.cel();//更新温度
    //获得用户界面传来的信号，进行判断做出行动
    message = Serial.read();
    doSwitch(message);

    vTaskDelay(2);
    //水温达到沸点且处于加热状态
    if (temperature >= LiquidTemper && digitalRead(heatLED) == 1)
      heatEndHandler();

    vTaskDelay(2);
    //更新加热过程状态
    if (num % 10000 == 0)
      heatProcessTask(NULL);
    vTaskDelay(2);
  }
}

/**
 * 判断函数
 * **/
void doSwitch(char message)
{
  switch (message)
  {
  case '0': //关机
    digitalWrite(LED_BUILTIN, LOW);
    digitalWrite(ENDLED, LOW);
    break;
  case '1': //启动
    digitalWrite(LED_BUILTIN, HIGH);
    break;
  case '2': //烧水
    auto_water = 1;
    //digitalWrite(heatSwitch, HIGH);
    xSemaphoreGive(heatLEDOn);
    break;
  case '3': //停止烧水
    auto_water = 0;
    digitalWrite(heatSwitch, HIGH);
    digitalWrite(heatSwitch, LOW);
    break;
  case '4': //控制加水
    digitalWrite(heatSwitch, LOW);
    auto_water = 1;
    xSemaphoreGive(autoPouring);
    break;
  case '5': //停止加水
    auto_water = 0;
    break;
  case '6': //用水
    if (cur_water > 2)
      cur_water -= 3;
    break;
  case '9': //控制输出
    Serial.print("t:[");
    Serial.print(temperature);
    Serial.print("]");
    Serial.print("water:[");
    Serial.print(cur_water);
    Serial.print("]");
    Serial.print("power:[");
    Serial.print(digitalRead(LED_BUILTIN));
    Serial.print("]");
    Serial.print("waterLED:[");
    Serial.print(digitalRead(waterLED));
    Serial.print("]");
    Serial.print("heatLED:[");
    Serial.print(digitalRead(heatLED));
    Serial.print("]");
    Serial.print("endLED:[");
    Serial.print(digitalRead(ENDLED));
    Serial.println("]");
    break;
  default:
    break;
  }
}

/**
 * 
 * 自动注水
 * 
 * **/
void autoPouringTask(void *pvParameters)
{
  (void)pvParameters;
  for (;;)
  {
    if (xSemaphoreTake(autoPouring, portMAX_DELAY) == pdPASS)
    {
      if (auto_water && cur_water < water)
      { //当前水位小于容量
        digitalWrite(waterLED, HIGH);
        //更新液晶屏
        lcd.setCursor(0, 1);
        lcd.print("Pouring");
        //等待注水时间
        for (; cur_water < water && auto_water; cur_water += 3)
        {
          vTaskDelay(20);
        }
        digitalWrite(waterLED, LOW);
        lcd.setCursor(0, 1);
        lcd.print("          ");
      }
    }
  }
}

/**
 *
 * 释放加热信号量
 *
 **/
void heatHandler()
{
  //当开关断开时io3为高电平，释放信号量
  if (digitalRead(heatSwitch) == 1)
    xSemaphoreGiveFromISR(heatInterrupt, NULL);
  //低电平将信号灯电瓶置为低电平
  else if (digitalRead(heatSwitch) == 0)
    digitalWrite(heatLED, LOW);
}
/**
 *
 * 释放加热信号灯亮信号量
 *
 * **/
void heatDELOnHandler()
{
  if (water > 10 && water < 100)
  {
    digitalWrite(waterLED, HIGH);
    delay(water);
    digitalWrite(waterLED, LOW);
  }
  xSemaphoreGive(heatLEDOn);
}



/**
 *
 * 加热信号灯亮
 *
 * **/
void heatLEDOnTask(void *pvParameters)
{
  (void)pvParameters;
  pinMode(heatLED, OUTPUT);
  pinMode(ENDLED, OUTPUT);
  pinMode(waterLED, OUTPUT);
  for (;;)
  {
    if (xSemaphoreTake(heatLEDOn, portMAX_DELAY) == pdPASS)
    {
      if (temperature < LiquidTemper)
      {
        //水温小于沸点
        digitalWrite(ENDLED, LOW);
        digitalWrite(heatLED, HIGH);
      }
    }
  }
}

/**
 *
 * 加热操作
 *
 * */
void heatTask(void *pvParameters)
{
  (void)pvParameters;
  //pinMode(heatSwitch,OUTPUT);
  pinMode(heatLED, OUTPUT);
  pinMode(ENDLED, OUTPUT);
  for (;;)
  {
    if (xSemaphoreTake(heatInterrupt, portMAX_DELAY) == pdPASS)
    {
      if (temperature < LiquidTemper)
      {
        //温度小于沸点
        digitalWrite(ENDLED, LOW);
        digitalWrite(heatLED, HIGH);
      }
    }
  }
}

/**
 * 加热过程显示状态任务
 * **/
void heatProcessTask(void *pvParameters)
{
  pinMode(heatLED, OUTPUT);
  for (;;)
  {
    //调度锁，读传感器
    vTaskSuspendAll();
    if (digitalRead(heatLED) == 1)
    {
      //处于加热状态，读传感器
      if (isnan(lm.cel()))
      {
        Serial.println("Failed to read from the LM35");
      }
      else
      {
        lcd.setCursor(0, 1);
        lcd.print(temperature);
        lcd.print(" *C     ");
      }
    }
    xTaskResumeAll();
    vTaskDelay(50);
  }
}

/**
 * 加热结束函数
 * **/
void heatEndHandler()
{
  pinMode(heatLED, OUTPUT);
  pinMode(ENDLED, OUTPUT);
  pinMode(Sounder, OUTPUT);
  
  digitalWrite(heatLED, LOW);
  digitalWrite(heatLED, LOW);
  digitalWrite(ENDLED, HIGH);
  digitalWrite(heatSwitch, LOW);
  analogWrite(Sounder, 127);
  vTaskDelay(50);
  analogWrite(Sounder, 0);
  
  lcd.setCursor(0, 1);
  lcd.print("Success!    ");
}
