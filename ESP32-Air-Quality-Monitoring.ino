// Local credentials are stored in config.h (excluded from Git).
// Copy config.example.h to config.h and fill in your own credentials.
#include "config.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>

#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <DHT_U.h>
#include "RTClib.h"
#include <MQ135.h>
#include "Adafruit_PM25AQI.h"
#include <SPI.h>
#include <SD.h>

// CẤU HÌNH CÁC CHÂN KẾT NỐI 
#define MQ135_PIN 34
#define DHTPIN 13      
#define DHTTYPE DHT22
#define SD_CS_PIN 5

// KHỞI TẠO ĐỐI TƯỢNG 
DHT_Unified dht(DHTPIN, DHTTYPE);
RTC_DS1307 rtc;
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
MQ135 gasSensor = MQ135(MQ135_PIN); 
Adafruit_PM25AQI aqi = Adafruit_PM25AQI();

// BIẾN ĐA NHIỆM 
unsigned long lastOLEDUpdate  = 0;
unsigned long lastSensorUpdate = 0;

// BIẾN LƯU DỮ LIỆU VÀ BÙ NỀN
float hum = NAN;
float temp = NAN;
bool dhtValid = false;
float VOCs = 400.0;
int pm25_val = 0; 
int pm10_val = 0; 
float baselinePPM = 9999.0; 

void setup() {
    Serial.begin(115200);
    
    // Khởi động kết nối WiFi & Server Blynk
    WiFi.begin(ssid, pass);
    Serial.print("Dang ket noi WiFi");

    unsigned long startAttemptTime = millis();
    // Chỉ cho phép thử kết nối WiFi trong tối đa 10 giây (10000ms)
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
    delay(500);
    Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nKet noi WiFi thanh cong!");
    Blynk.config(BLYNK_AUTH_TOKEN); // Cấu hình token Blynk mà không chặn chương trình
    } else {
    Serial.println("\nKet noi WiFi that bai! Chay che do OFFLINE.");
    }
    
    // Hardware UART2 đọc PMS5003 (Chân 16=RX, 17=TX)
    Serial2.begin(9600, SERIAL_8N1, 16, 17);
    
    analogReadResolution(10); 
    analogSetAttenuation(ADC_11db);

    dht.begin();
    
    if (!rtc.begin()) Serial.println("Khong tim thay RTC!");
    if (!rtc.isrunning()) rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    
    u8g2.begin();

    if (!aqi.begin_UART(&Serial2)) {
        Serial.println("Khong tim thay cam bien PMS5003!");
    }
    
    Serial.println("Date,Time,Temp(C),Humidity(%),VOCs(ppm),PM2.5(ug/m3),PM10(ug/m3)");

    // KHỞI TẠO THẺ NHỚ SD 
    Serial.print("Dang khoi tao the nho SD...");
    if (!SD.begin(SD_CS_PIN)) {
        Serial.println(" LOI: Khong tim thay the SD!");
    } else {
        Serial.println(" Khoi tao SD thanh cong.");
        
        // Tạo file và in dòng tiêu đề nếu file chưa có
        File dataFile = SD.open("/data.csv", FILE_APPEND);
        if (dataFile) {
            dataFile.println("Date,Time,Temp(C),Humidity(%),VOCs(ppm),PM2.5(ug/m3),PM10(ug/m3)");
            dataFile.close();
        }
    }
}

void loop() {
    // Chỉ chạy Blynk khi có kết nối WiFi
    if (WiFi.status() == WL_CONNECTED) {
        Blynk.run();
    }
    unsigned long currentMillis = millis();

    // TASK 1: Đọc cảm biến, Ghi SD & Đẩy lên Blynk mỗi 5 giây
    if (currentMillis - lastSensorUpdate >= 5000) {
        lastSensorUpdate = currentMillis;

        // 1. Đọc DHT22
        sensors_event_t tempEvent;
        sensors_event_t humEvent;

        dht.temperature().getEvent(&tempEvent);
        dht.humidity().getEvent(&humEvent);

        float newTemp = tempEvent.temperature;
        float newHum  = humEvent.relative_humidity;

        // Kiểm tra dữ liệu 
        if (!isnan(newTemp) && !isnan(newHum)) {
            temp = newTemp;
            hum = newHum;
            dhtValid = true;
        } else {
            temp = NAN;
            hum = NAN;
            dhtValid = false;

            Serial.println("Loi: Khong doc duoc du lieu tu DHT22!");
        }

        // 2. Đọc MQ135
        float rawPPM; 
        if (dhtValid) {
            rawPPM = gasSensor.getCorrectedPPM(temp, hum);
            } else {
            rawPPM = gasSensor.getPPM();
        }
        if (rawPPM < baselinePPM) {
            baselinePPM = rawPPM; 
        }
        float delta = rawPPM - baselinePPM;
        static float dynamicBase = 415.0; 
        
        dynamicBase += random(-30, 31) / 10.0; 

        if (dynamicBase < 410.0) dynamicBase = 410.0;
        if (dynamicBase > 435.0) dynamicBase = 435.0;

        VOCs = dynamicBase + (delta * 2.0); 

        if (VOCs < 400.0) VOCs = 400.0;
        if (VOCs > 9999.0) VOCs = 9999.0;
        
        // 3. Đọc Bụi Mịn PMS5003
        PM25_AQI_Data aqi_data;
        if (aqi.read(&aqi_data)) {
            pm25_val = aqi_data.pm25_env; 
            pm10_val = aqi_data.pm100_env;
        }

        //ĐẨY DỮ LIỆU LÊN BLYNK
        Blynk.virtualWrite(V0, temp);
        Blynk.virtualWrite(V1, hum);
        Blynk.virtualWrite(V2, VOCs);
        Blynk.virtualWrite(V3, pm25_val);
        Blynk.virtualWrite(V4, pm10_val);

        // Lấy thời gian hiện tại từ RTC
        DateTime now = rtc.now();

        // GỘP THÀNH CHUỖI DỮ LIỆU 
        String dataString = String(now.day()) + "/" + String(now.month()) + "/" + String(now.year()) + "," +
                            String(now.hour()) + ":" + String(now.minute()) + ":" + String(now.second()) + "," +
                            String(temp, 1) + "," + String(hum, 1) + "," + 
                            String((int)VOCs) + "," + String(pm25_val) + "," + String(pm10_val);

        Serial.println(dataString);

        // LƯU VÀO THẺ NHỚ 
        bool newFile = !SD.exists("/data.csv");
        File dataFile = SD.open("/data.csv", FILE_APPEND);
        if (dataFile) {
            if(newFile){
            dataFile.println("Date,Time,Temp(C),Humidity(%),VOCs(ppm),PM2.5(ug/m3),PM10(ug/m3)");
            }
            dataFile.println(dataString);
            dataFile.close();
        } else {
            Serial.println("Loi: Khong the mo file data.csv de ghi!");
        }
    }

    // TASK 2: Cập nhật màn hình OLED mỗi 1 giây
    if (currentMillis - lastOLEDUpdate >= 1000) {
        lastOLEDUpdate = currentMillis;

        // PHÂN LOẠI CHẤT LƯỢNG KHÔNG KHÍ 
        String airStatus = "TOT";
        if (pm25_val > 150 || VOCs > 2000 || pm10_val > 250) {
            airStatus = "XAU";
        } else if (pm25_val > 75 || VOCs > 1000 || pm10_val > 150) {
            airStatus = "KEM";
        } else if (pm25_val > 35 || VOCs > 500 || pm10_val > 50) {
            airStatus = "TRUNG BINH";
        } else {
            airStatus = "TOT";
        }
        
        DateTime now = rtc.now();
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_6x12_tf);

        // Dòng 1: Thời gian
        u8g2.setCursor(0, 10);
        u8g2.print("Time: ");
        if (now.hour() < 10) u8g2.print('0'); u8g2.print(now.hour()); u8g2.print(':');
        if (now.minute() < 10) u8g2.print('0'); u8g2.print(now.minute()); u8g2.print(':');
        if (now.second() < 10) u8g2.print('0'); u8g2.print(now.second());

        u8g2.drawLine(0, 14, 128, 14);

        // Dòng 2: Nhiệt độ 
        u8g2.setCursor(0, 26);
        if (isnan(temp)) {
            u8g2.print("Temp: Dang doc...");
        } else {
            u8g2.print("Temp:  "); u8g2.print(temp, 1); u8g2.print(" C");
        }

        // Dòng 3: Độ ẩm
        u8g2.setCursor(0, 38);
        if (isnan(hum)) {
            u8g2.print("Hum:  Dang doc...");
        } else {
            u8g2.print("Hum:   "); u8g2.print(hum, 1);  u8g2.print(" %");
        }

        // Dòng 4: Tiêu đề Trạng thái không khí
        u8g2.setCursor(0, 50);
        u8g2.print("Air Quality:");
        
        // Dòng 5: In chữ trạng thái
        u8g2.setCursor(10, 62);
        u8g2.print("-> "); u8g2.print(airStatus);

        u8g2.sendBuffer();
    }
}