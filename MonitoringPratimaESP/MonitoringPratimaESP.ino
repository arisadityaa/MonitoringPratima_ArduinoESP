#include <Arduino.h>
#include <ArduinoJson.h>
#include "esp_camera.h"
#include <base64.h>
#include "time.h"
#include <WiFi.h>
#include "FS.h"                   // SD Card ESP32
#include "SD_MMC.h"               // SD Card ESP32
#include <Firebase_ESP_Client.h>

// Provide the token generation process info.
#include <addons/TokenHelper.h>
// Provide the RTDB payload printing info and other helper functions.
#include <addons/RTDBHelper.h>

// header file
#include "esp_pin.h"
#include "camera_config.h"
#include "secret.h"

//time NTP
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600;
const long my_gmt = 7;
const int   daylightOffset_sec = 3600;
String Time, Day, dateTime;

#define FLASH_LED_PIN 4

/* 2. Define the API Key Firebase*/
#define API_KEY API_key
/* 3. Define the RTDB URL */
#define DATABASE_URL database_URL  //<databaseName>.firebaseio.com or <databaseName>.<region>.firebasedatabase.app
/* 4. Define the user Email and password that alreadey registerd or added in your project */
#define USER_EMAIL user_Email
#define USER_PASSWORD user_password
// Define Firebase Data object
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

String DEVICE_REGISTRATION_ID_TOKEN = "";
#define FIREBASE_FCM_SERVER_KEY FCM_Token

// millis
unsigned long serialData = millis();
unsigned long sendNotif = millis();
unsigned long readDataFirebase = millis();
unsigned long millisCapture = millis();
unsigned long scheduleGambar;


// wifi

const char *ssid = your_SSID;
const char *password = your_Password;

// deklarasi Variabel
String ImageBase64;
int motion = 0, flame = 0, distance = 0;
int scheduleSet = 0;
float kelembaban = 0, suhu = 0;
String dataSerial;
String dataConvert;
String dataIndex[10];
int dataCount;
int SD_Memory = 0, SD_Free = 0, SD_used = 0;

// deklarasi variabel deteksi
int detectFlame, detectMotion;
int isSchedule;
int setDistance;
int resetbtn, settingbtn;
bool isFlame = false, isMotion = false, isDistance = false, isTemp = false;
bool isCard = true;
int flameflag = 0, distanceflag = 0, tempflag = 0, motionflag = 0;


void printLocalTime() {
  char str_time[10];
  char str_day[30];
  char str_dateTime[20];
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }

  strftime(str_day, sizeof str_day, "%A, %d %B %Y", &timeinfo);
  strftime(str_time, sizeof str_time, "%H:%M:%S", &timeinfo);
  strftime(str_dateTime, sizeof str_dateTime, "%Y-%m-%d-%H-%M-%S", &timeinfo);
  Time = String(str_time);
  Day = String(str_day);
  dateTime = String(str_dateTime);
}

void configWifi() {
  WiFi.mode(WIFI_STA);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  Serial.print("ESP32-CAM IP Address: ");
  Serial.println(WiFi.localIP());
}

// Initialize the micro SD card
void initMicroSDCard() {
  // Start Micro SD card
  Serial.println("Starting SD Card");
  if (!SD_MMC.begin()) {
    Serial.println("SD Card Mount Failed");
    isCard = false;
    return;
  }
  uint8_t cardType = SD_MMC.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("No SD Card attached");
    isCard = false;
    return;
  }
  Serial.println("SD Card Ok");
}

void checkStorage() {
  if (isCard == true) {
    uint64_t totalSpace = SD_MMC.totalBytes() / (1024 * 1024);
    uint64_t usedSpace = SD_MMC.usedBytes() / (1024 * 1024);
    uint64_t freeSpace = totalSpace - usedSpace;

    SD_Memory = totalSpace;
    SD_used = usedSpace;
    SD_Free = freeSpace;
  }
}

void kirimNotif(String title, String body) {
  Serial.print("Send " + title);
  FCM_Legacy_HTTP_Message msg;

  msg.targets.to = DEVICE_REGISTRATION_ID_TOKEN;

  msg.options.time_to_live = "1000";
  msg.options.priority = "high";

  msg.payloads.notification.title = title;
  msg.payloads.notification.body = body;
  if (Firebase.FCM.send(&fbdo, &msg)) // send message to recipient
    Serial.printf("ok\n%s\n\n", Firebase.FCM.payload(&fbdo).c_str());
  else
    Serial.println(fbdo.errorReason());
}

void notifyMemory() {
  if (isCard == true) {
    if (SD_Free != 0 && SD_Free < 1000) {
      if (DEVICE_REGISTRATION_ID_TOKEN != "") {
        kirimNotif("Penyimpanan Perangkat Habis", "Penyimpanan pada Perangkat 1 Hampir Habis");
      }
    }
  }
}

String CaptureImage() {
  camera_fb_t * fb = NULL;
  fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    return "";
  }

  if (isCard == true) {
    // Path where new picture will be saved in SD Card
    String path = "/picture_" + String(dateTime) + ".jpg";
    Serial.printf("Picture file name: %s\n", path.c_str());

    // Save picture to microSD card
    fs::FS &fs = SD_MMC;
    File file = fs.open(path.c_str(), FILE_WRITE);
    if (!file) {
      Serial.printf("Failed to open file in writing mode");
    } else {
      file.write(fb->buf, fb->len); // payload (image), payload length
      Serial.printf("Saved: %s\n", path.c_str());
    }
    file.close();
  }

  String Image = base64::encode(fb->buf, fb->len);
  esp_camera_fb_return(fb);

  return Image;
}

void sendStorage() {
  checkStorage();
  FirebaseJson SDCardJson;
  SDCardJson.add("StorageMemory", SD_Memory);
  SDCardJson.add("MemoryUsed", SD_used);
  SDCardJson.add("FreeMemory", SD_Free);
  Serial.printf("Update node... %s\n", Firebase.RTDB.updateNode(&fbdo, "1/StatusDeteksi", &SDCardJson) ? "ok" : fbdo.errorReason().c_str());
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);

  // Connect to Wi-Fi
  configWifi();

  // Config and init the camera
  configInitCamera();

  // Set LED Flash as output
  pinMode(FLASH_LED_PIN, OUTPUT);
  digitalWrite(FLASH_LED_PIN, LOW);

  //konfig firebase
  Serial.printf("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);
  /* Assign the api key (required) */
  config.api_key = API_KEY;
  /* Assign the user sign in credentials */
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  /* Assign the RTDB URL (required) */
  config.database_url = DATABASE_URL;
  /* Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback; // see addons/TokenHelper.h
  // Limit the size of response payload to be collected in FirebaseData
  //  fbdo.setResponseSize(2048);
  Firebase.begin(&config, &auth);
  // Comment or pass false value when WiFi reconnection will control by your code or third party library
  Firebase.reconnectWiFi(true);
  Firebase.FCM.setServerKey(FIREBASE_FCM_SERVER_KEY);
  Firebase.setDoubleDigits(5);
  config.timeout.serverResponse = 10 * 1000;
  //end config firebase

  //init and get the time
  configTime(gmtOffset_sec * my_gmt, daylightOffset_sec, ntpServer);
  printLocalTime();
  if (dateTime == "") {
    ESP.restart();
  }
  initMicroSDCard();
  readFirebase();
  if (Firebase.ready()) {
    sendStorage();
  }
}

void loop() {
  // put your main code here, to run repeatedly:

  //read data serial
  if ( millis() > serialData + 500) {
    dataSerial = "";
    while (Serial.available() > 0) {
      dataSerial += char(Serial.read());
    }
    dataSerial.trim();

    const char serialEnd = ';';
    bool stopSerial = false;
    String dataCek = "";
    const char delimiter = '#';
    dataCount = 0;
    for (int cek = 0; cek <= dataSerial.length(); cek++) {
      if (dataSerial[cek] != serialEnd) {
        if (stopSerial == false) {
          if (dataSerial[cek] == delimiter) {
            dataCount++;
          }
          dataCek += dataSerial[cek];
        }
      } else {
        stopSerial = true;
      }
    }

    if (dataCount == 4) {
      dataConvert = String(dataCek);
      int index = 0;
      for (int i = 0; i <= dataConvert.length(); i++) {
        if (dataConvert[i] != delimiter) {
          dataIndex[index] += char(dataConvert[i]);
        } else {
          index++;
        }
      }
      if (dataIndex[0] != "") {
        distance = dataIndex[0].toInt();
      }
      if (dataIndex[1] != "") {
        suhu = dataIndex[1].toFloat();
      }
      if (dataIndex[2] != "") {
        kelembaban = dataIndex[2].toFloat();
      }
      if (dataIndex[3] != "") {
        flame = dataIndex[3].toInt();
      }
      if (dataIndex[4] != "") {
        motion = dataIndex[4].toInt();
      }
      for (int pos = 0; pos <= 10; pos++) {
        dataIndex[pos] = "";
      }
      Serial.println("Jarak= " + String(distance) + ", Suhu= " + String(suhu) + ", Kelembaban= " + String(kelembaban) + ", Api= " + String(flame) + ", Gerakan= " + String(motion));

      if (Firebase.ready()) {
        FirebaseJson realtimeJson;
        realtimeJson.set("JarakPratima", String(distance));
        realtimeJson.set("Suhu", String(suhu));
        realtimeJson.set("Kelembaban", String(kelembaban));
        realtimeJson.set("Api", String(flame));
        realtimeJson.set("Pergerakan", String(motion));
        Serial.printf("Set json... %s\n", Firebase.RTDB.set(&fbdo, F("1/Realtime"), &realtimeJson) ? "ok" : fbdo.errorReason().c_str());
      }
    }

    serialData = millis();
  }
  //end read data serial

  if (millis() > readDataFirebase + 1000) {
    readFirebase();
    readDataFirebase = millis();
  }

  if (isSchedule == 1 && millis() > millisCapture + scheduleGambar) {
    printLocalTime();
    String ImageData = CaptureImage();
    if (Firebase.ready()) {
      FirebaseJson json;
      json.add("Image", ImageData);
      json.add("Tanggal", Day);
      json.add("Waktu", Time);
      Serial.printf("Update node... %s\n", Firebase.RTDB.updateNode(&fbdo, "1/ScheduleGambar/" + String(dateTime), &json) ? "ok" : fbdo.errorReason().c_str());
      sendStorage();
    }
    notifyMemory();
    millisCapture = millis();
  }

  //send notif
  if (millis() > sendNotif + 500) {
    if (DEVICE_REGISTRATION_ID_TOKEN != "") {

      //deteksi kebakaran
      if (detectFlame == 1) {
        if (flame < 300) {
          flameflag++;
          if (isFlame == false || flameflag % 10 == 0) {
            kirimNotif("Terdeteksi Kebakaran", "Terdeteksi Api Disekitar Penyimpanan Pratima");
            isFlame = true;
          }
        } else {
          isFlame = false;
          flameflag = 0;
        }

        if (suhu > 40) {
           tempflag++;
          if (isTemp == false || tempflag % 10 == 0) {
            kirimNotif("Terdeteksi Kebakaran", "Terdeteksi Suhu Tinggi Disekitar Penyimpanan Pratima");
            isTemp = true;
          }
        } else {
          isTemp = false;
          tempflag = 0;
        }
      }
      //end deteksi kebakaran

      //deteksi pergerakan
      if (detectMotion == 1) {
        if (motion == 1) {
          motionflag++;
          if (isMotion == false || motionflag % 10 == 0) {
            kirimNotif("Terdeteksi Pergerakan", "Terdeteksi Pergerakan Disekitar Penyimpanan Pratima");
            isMotion = true;
          }
        } else {
          isMotion = false;
          motionflag = 0;
        }
      }
      //end deteksi pergerakan

      //deteksi jarak
      if (setDistance > 0) {
        if (distance > setDistance + 1 || distance < setDistance - 1) {
          distanceflag++;
          if (isDistance == false || distanceflag%10==0) {
            kirimNotif("Pratima Tidak Ditempatnya", "Pratima Tidak pada tempatnya atau sudah dipindahkan");
            isDistance = true;
          }
        } else {
          isDistance = false;
          distanceflag = 0;
        }
      }
      //end deteksi jarak
    }
  }
  sendNotif = millis();
  //end send notif
}

void readFirebase() {
  if (Firebase.ready()) {
    //deteksi kebakaran
    if (Firebase.RTDB.getInt(&fbdo, "1/StatusDeteksi/DeteksiKebakaran")) {
      detectFlame = fbdo.to<int>();
    }
    //end deteksi kebakaran

    //deteksi pergerakan
    if (Firebase.RTDB.getInt(&fbdo, "1/StatusDeteksi/DeteksiPergerakan")) {
      detectMotion = fbdo.to<int>();
    }
    //end deteksi pergerakan

    if (Firebase.RTDB.getInt(&fbdo, "1/StatusDeteksi/JarakSetting")) {
      setDistance = fbdo.to<int>();
    }

    //btn reset(deteksi jarak)
    if (Firebase.RTDB.getInt(&fbdo, "1/StatusDeteksi/BtnReset")) {
      resetbtn = fbdo.to<int>();
      if (resetbtn == 1) {
        setDistance = 0;
        resetbtn = 0;
        Serial.print(Firebase.RTDB.setInt(&fbdo, F("1/StatusDeteksi/JarakSetting"), int(setDistance)) ? "Data Jarak Direset" : fbdo.errorReason().c_str());
        Serial.println(Firebase.RTDB.setInt(&fbdo, F("1/StatusDeteksi/BtnReset"), int(resetbtn)) ? " btn reset : ok" : fbdo.errorReason().c_str());
      }
    }
    //end btn reset(deteksi jarak)

    //btn set(deteksi jarak)
    if (Firebase.RTDB.getInt(&fbdo, "1/StatusDeteksi/BtnSetting")) {
      settingbtn = fbdo.to<int>();
      if (settingbtn == 1) {
        setDistance = distance;
        settingbtn = 0;
        Serial.println(Firebase.RTDB.setInt(&fbdo, F("1/StatusDeteksi/JarakSetting"), int(setDistance)) ? "Data Jarak diset" : fbdo.errorReason().c_str());
        Serial.println(Firebase.RTDB.setInt(&fbdo, F("1/StatusDeteksi/BtnSetting"), int(settingbtn)) ? " btn set : ok" : fbdo.errorReason().c_str());
      }
    }
    //end btn set(deteksi jarak)

    //schedule capture gambar
    if (Firebase.RTDB.getInt(&fbdo, "1/StatusDeteksi/ScheduleCapture")) {
      scheduleSet = fbdo.to<int>();
      if (scheduleSet == 0) {
        isSchedule = 0;
        scheduleGambar = 0;
        Serial.println("Schedule Capture Gambar Mati");
      } else {
        isSchedule = 1;
        Serial.println("Schedule Gambar" + String(scheduleSet) + "Menit");
        scheduleGambar = scheduleSet * 60 * 1000;
      }
    }
    //end schedule capture gambar

    //device token
    if (Firebase.RTDB.getInt(&fbdo, "1/StatusDeteksi/Token")) {
      DEVICE_REGISTRATION_ID_TOKEN = fbdo.to<String>();
    } else {
      Serial.println("Error Device Token");
    }
    //end device token
  }
  Serial.println("Deteksi Kebakaran= " + String(detectFlame) + ", Deteksi Pergerakan= " + String(detectMotion) + ", Setting Jarak= " + String(setDistance));
}
