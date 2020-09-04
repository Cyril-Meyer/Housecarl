/* Housecarl
 * Open Source home security IP cam
 * Based on AI Thinker ESP32-CAM board
 */
#define VERSION "1.0"

// Personal configuration
#include "configuration.h"
// ESP32-CAM Pinout
#include "camerapins.h"
// HTML pages
#include "html.h"

// Include for brownout detector deactivation
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
// Read and Write from flash memory
#include <EEPROM.h>
// Camera
#include "esp_camera.h"
// Read and Write from SD Card
#include "FS.h"
#include "SD_MMC.h"
// Connect to WiFi and server
#include <WiFi.h>

// Current image id
unsigned long currentImgId = 0;
// Current log id
unsigned long currentLogId = 0;

// Reserve 2 x 4 bytes (32 bits) value into EEPROM
#define EEPROM_SIZE 8

// Camera config
camera_config_t camera_config;

// Wifi server on port 80
WiFiServer server(80);

void setup()
{
  // Disable brownout detector
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);


  // Serial setup
  Serial.begin(115200);

  // clean serial, println = print + "\r\n"
  Serial.println();

  // Hello
  Serial.print("Housecarl ");
  Serial.print(VERSION);
  Serial.println();

  // EEPROM used to read and save last image number and logs number
  EEPROM.begin(EEPROM_SIZE);
  Serial.print("[INFO]  ");
  Serial.println("EEPROM initialization");

  // GPIO 4 = FLASH
  pinMode(4, INPUT);
  if(digitalRead(4) == HIGH)
  {
    Serial.print("[INFO]  ");
    Serial.println("Reset image and log id ");
    EEPROM_writeLong(0, 0);
    EEPROM_writeLong(4, 0);
  }
  
  currentImgId = EEPROM_readLong(0);
  currentLogId = EEPROM_readLong(4);
  Serial.print("[INFO]  ");
  Serial.print("Current image id       ");
  Serial.println(currentImgId);
  Serial.print("[INFO]  ");
  Serial.print("Current log id         ");
  Serial.println(currentLogId);


  // Camera initialization
  Serial.print("[INFO]  ");
  Serial.println("Camera initialization  ");
  
  // Camera config
  camera_config.ledc_channel = LEDC_CHANNEL_0;
  camera_config.ledc_timer = LEDC_TIMER_0;
  camera_config.pin_d0 = Y2_GPIO_NUM;
  camera_config.pin_d1 = Y3_GPIO_NUM;
  camera_config.pin_d2 = Y4_GPIO_NUM;
  camera_config.pin_d3 = Y5_GPIO_NUM;
  camera_config.pin_d4 = Y6_GPIO_NUM;
  camera_config.pin_d5 = Y7_GPIO_NUM;
  camera_config.pin_d6 = Y8_GPIO_NUM;
  camera_config.pin_d7 = Y9_GPIO_NUM;
  camera_config.pin_xclk = XCLK_GPIO_NUM;
  camera_config.pin_pclk = PCLK_GPIO_NUM;
  camera_config.pin_vsync = VSYNC_GPIO_NUM;
  camera_config.pin_href = HREF_GPIO_NUM;
  camera_config.pin_sscb_sda = SIOD_GPIO_NUM;
  camera_config.pin_sscb_scl = SIOC_GPIO_NUM;
  camera_config.pin_pwdn = PWDN_GPIO_NUM;
  camera_config.pin_reset = RESET_GPIO_NUM;
  camera_config.xclk_freq_hz = 20000000;
  camera_config.pixel_format = PIXFORMAT_JPEG;
  
  /*
   * FRAMESIZE_QVGA (320 x 240)
   * FRAMESIZE_CIF (352 x 288)
   * FRAMESIZE_VGA (640 x 480)
   * FRAMESIZE_SVGA (800 x 600)
   * FRAMESIZE_XGA (1024 x 768)
   * FRAMESIZE_SXGA (1280 x 1024)
   * FRAMESIZE_UXGA (1600 x 1200)
   */
   
  /*
  if(psramFound())
  {
    camera_config.frame_size = FRAMESIZE_UXGA;
    camera_config.jpeg_quality = 10;
    camera_config.fb_count = 2;
  }
  else
  {
    camera_config.frame_size = FRAMESIZE_SVGA;
    camera_config.jpeg_quality = 12;
    camera_config.fb_count = 1;
  }
  */
  camera_config.frame_size = FRAMESIZE_SXGA;
  camera_config.jpeg_quality = 20;
  camera_config.fb_count = 2;

  esp_err_t err = esp_camera_init(&camera_config);
  if (err  != ESP_OK)
  {
    Serial.print("[ERROR] ");
    Serial.print("Camera initialization  ");
    Serial.println(err);
    return;
  }
  

  // SD initialization
  Serial.print("[INFO]  ");
  Serial.println("SD Card initialization ");
  
  // if(!SD_MMC.begin())
  if(!SD_MMC.begin("/sdcard", true))
  {
    Serial.print("[ERROR] ");
    Serial.print("SD Card initialization ");
    Serial.println("SD_MMC.begin()");
    return;
  }
  
  if(SD_MMC.cardType() == CARD_NONE)
  {
    Serial.print("[ERROR] ");
    Serial.print("SD Card initialization ");
    Serial.println("No SD Card detected");
    return;
  }


  // Connect to WiFi
  Serial.print("[INFO]  ");
  Serial.print("Connecting to WiFi     ");
  Serial.println(ssid);
  Serial.print("[INFO]  ");
  Serial.print("Requested IP address   ");
  Serial.println(ip);
  
  WiFi.config(ip, gateway, subnet, primaryDNS, secondaryDNS);
  WiFi.begin(ssid, password);

  // Connected to WiFi
  Serial.print("[INFO]  ");
  Serial.print("WiFi connected         ");
  Serial.println(ssid);
  Serial.print("[INFO]  ");
  Serial.print("IP address             ");
  Serial.println(WiFi.localIP());

  // Start server
  server.begin();
  Serial.print("[INFO]  ");
  Serial.println("Server ready           ");
}

void loop()
{
  // Use time0 and time1 to know how much time we lost in processing
  static unsigned int timeSinceLastCapture = 0;
  unsigned long time0 = millis();

  WiFiClient client = server.available();   // Client incoming connection

  // True if there is a waiting client
  if(client)
  {
    // New client available
    Serial.print("[INFO]  ");
    Serial.print("New client available   ");
    Serial.println(client.remoteIP());
    // Client incoming data
    String clientLog = "";
    String clientCurrentLine = "";
    
    // Flags for response
    /* 0 - 403
     * 1 - ls
     * 2 - CAPTURE_FOLDER
     * 3 - LOGS_FOLDER
     */
    unsigned int request = 0;
    // Optional, used when request is 2 or 3
    String fileRequest = "";

    // read data from client
    // client is considered connected if the connection has been closed but there is still unread data.
    while(client.connected())
    {
      if (client.available())
      {
        // read next byte
        char c = client.read();
        clientLog += c;
        
        // If newline character
        if (c == '\n')
        {
          // If clientCurrentLine is blank, that mean that we receive two newline characters in a row.
          // This mean this is the end of the client HTTP request.
          if(clientCurrentLine.length() == 0)
          {
            // HTTP Response
            if(request == 1)
            {
              // HTTP Header
              client.println("HTTP/1.1 200 OK");
              client.println("Content-type:text/html");
              // HTTP content
              String html_ls_ = String(html_ls);
              html_ls_.replace("%%CIID%%", String(currentImgId));
              html_ls_.replace("%%CLID%%", String(currentLogId));
              client.print(html_ls_);
            }
            else if(request == 2)
            {
              String path = CAPTURE_FOLDER + String(fileRequest) +".jpg";

              // change SD card mode
              SD_MMC.end();
              SD_MMC.begin();
              
              // does image exist 
              fs::FS &fs = SD_MMC; 
      
              File file = fs.open(path.c_str(), FILE_READ);
              
              if(file)
              {
                // HTTP Header
                client.println("HTTP/1.1 200 OK");
                client.println("Content-type:image/jpeg");
                client.println();
                
                byte clientBuf[4096];
                int clientCount = 0;
                  
                while(file.available())
                {
                  clientBuf[clientCount] =  file.read();
                  clientCount++;
                  if(clientCount > 4095)
                  {
                    client.write(clientBuf, 4096);
                    clientCount = 0;
                  }
                }
                if(clientCount > 0) client.write(clientBuf,clientCount);
              }
              
              file.close();
              
              SD_MMC.end();
              SD_MMC.begin("/sdcard", true);
            }
            else if(request == 3)
            {
              String path = LOGS_FOLDER + String(fileRequest) +".txt";

              // does image exist 
              fs::FS &fs = SD_MMC; 
      
              File file = fs.open(path.c_str(), FILE_READ);
              
              if(file)
              {
                // HTTP Header
                client.println("HTTP/1.1 200 OK");
                client.println("Content-type:text/html");
                client.println();
                
                byte clientBuf[4096];
                int clientCount = 0;
                  
                while(file.available())
                {
                  clientBuf[clientCount] =  file.read();
                  clientCount++;
                  if(clientCount > 4095)
                  {
                    client.write(clientBuf, 4096);
                    clientCount = 0;
                  }
                }
                if(clientCount > 0) client.write(clientBuf,clientCount);
              }
              
              file.close();
            }
            else
            {
              // HTTP Header
              client.println("HTTP/1.1 403 Forbidden");
              client.println("Content-type:text/html");
              // default : 403 Forbidden
              client.print(html_403_forbidden);
            }

            // HTTP end
            client.println();

            // END client connection
            client.stop();
          }
          else
          {
            if(clientCurrentLine.startsWith("GET") && clientCurrentLine.endsWith("HTTP/1.1") )
            {
              clientCurrentLine.replace("GET", "");
              clientCurrentLine.replace("HTTP/1.1", "");
              clientCurrentLine.replace(" ", "");

              if(clientCurrentLine == "/ls")
              {
                request = 1;
                
                Serial.print("[INFO]  ");
                Serial.println("/ls requested");
              }
              else if(clientCurrentLine.startsWith(CAPTURE_FOLDER))
              {
                clientCurrentLine.replace(CAPTURE_FOLDER, "");
                fileRequest = clientCurrentLine;
                fileRequest.replace(".jpg", "");
                request = 2;
                
                Serial.print("[INFO]  ");
                Serial.print(CAPTURE_FOLDER);
                Serial.print(" file requested ");
                Serial.println(fileRequest);
              }
              else if(clientCurrentLine.startsWith(LOGS_FOLDER))
              {
                clientCurrentLine.replace(LOGS_FOLDER, "");
                fileRequest = clientCurrentLine;
                fileRequest.replace(".txt", "");
                request = 3;
                
                Serial.print("[INFO]  ");
                Serial.print(LOGS_FOLDER);
                Serial.print(" file requested ");
                Serial.println(fileRequest);
              }
            }
            clientCurrentLine = "";
          }
        }
        else if (c != '\r')
        {
          clientCurrentLine += c;
        }
      }
    }
    
    // Update current log id
    currentLogId++;
    EEPROM_writeLong(4, currentLogId);
    
    Serial.print("[INFO]  ");
    Serial.println("Connection closed      ");

    // Save Log
    String filename = "";      
    int zeros;
      
    for(zeros=0 ; zeros<10-String(currentLogId).length() ; ++zeros)
      filename.concat('0');
    filename.concat(String(currentLogId));

    String path = LOGS_FOLDER + String(filename) +".txt";
      
    Serial.print("[INFO]  ");
    Serial.print("Saving current log     ");
    Serial.println(path);
    
    fs::FS &fs = SD_MMC;
    File file = fs.open(path.c_str(), FILE_WRITE);
      
    if(!file)
    {
      Serial.print("[WARN]  ");
      Serial.println("Canno't open file in writing mode");
    } 
    else
    {
      Serial.print("[INFO]  write                  ");
      Serial.println(file.println(clientLog));
    }
    file.close();
  }

  unsigned long time1 = millis();
  timeSinceLastCapture += (time1 - time0);

  if(timeSinceLastCapture >= CAPTURE_DELAY)
  {
    unsigned long time0 = millis();
    
    // Update current image id
    currentImgId++;
    if(currentImgId % 32 == 0)
      EEPROM_writeLong(0, currentImgId);
      
    timeSinceLastCapture = 0;
    
    // Capture
    camera_fb_t * fb = NULL;
    fb = esp_camera_fb_get();  
    if(!fb)
    {
      Serial.print("[WARN]  ");
      Serial.println("Camera capture failed  ");
    }
    else
    {
      // Save Capture
      String filename = "";      
      int zeros;
      
      for(zeros=0 ; zeros<10-String(currentImgId).length() ; ++zeros)
        filename.concat('0');
      filename.concat(String(currentImgId));

      String path = CAPTURE_FOLDER + String(filename) +".jpg";
      
      Serial.print("[INFO]  ");
      Serial.print("Saving current image   ");
      Serial.println(path);

      fs::FS &fs = SD_MMC; 
      
      File file = fs.open(path.c_str(), FILE_WRITE);
      
      if(!file)
      {
        Serial.print("[WARN]  ");
        Serial.println("Canno't open file in writing mode");
      } 
      else
      {
        Serial.print("[INFO]  write                  ");
        Serial.println(file.write(fb->buf, fb->len));
      }
      
      file.close();
    }
    // free
    esp_camera_fb_return(fb);

    unsigned long time1 = millis();
    timeSinceLastCapture += (time1 - time0);
  }
  else
  {
    delay(10);
    timeSinceLastCapture += 10;
  }
}
