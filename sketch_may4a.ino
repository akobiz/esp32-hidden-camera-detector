#include <Arduino.h>
#include <AccelStepper.h>
#include <esp_camera.h>
#include <WiFi.h>
#include <WebServer.h>
#include <html.h>
#include <SPIFFS.h>
#include <JPEGDecoder.h>
#include <JPEGENC.h>
#include <DNSServer.h>

const byte DNS_PORT = 53;
DNSServer dnsServer;

// Піни
#define LED_FLASH 4
#define LED_BUILDIN 33
const int stepper_h_enabled_pin = 15;
const int stepper_d_enabled_pin = 14;

// WiFi дані для доступу
#define WiFi_SSID "HiddenCameraDetector_Access" // SSID for the access point
#define WiFi_PASSWORD "camera123" // Password for the access point

#define IMAGE_WIDTH 320
#define IMAGE_HEIGHT 240
#define BRIGHTNESS_THRESHOLD 220      // Поріг загальної яскравості для відблиску
#define COLOR_CHANNEL_THRESHOLD 240   // Мінімальна яскравість кожного каналу
#define MAX_GLARE_AREA 20000          // Максимальна площа для відблиску
#define MIN_GLARE_AREA 250             // Мінімальна площа для відблиску
#define MAX_ASPECT_RATIO 1.8          // Максимальне співвідношення сторін для відблиску

// Ініціалізуємо змінну веб-серверу
WebServer webserver(80);

// Змінна для контролю зупинки при виявленні відблиску
bool isFlashDetected = false;

// Wifi конфігурація
IPAddress local_IP(192, 168, 4, 1); // статична айпі адреса
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);

// Створення двигунів
AccelStepper stepper_h(AccelStepper::DRIVER, 13, 12);
AccelStepper stepper_d(AccelStepper::DRIVER, 0, 2);

// Обмеження руху
const int H_MIN = 0;
const int H_MAX = 100;       // ~180°
const int H_CENTER = 50;     // ~90°
const int H_STEP = 25;       // ~45°

const int D_FRONT_MIN = -8;  // ~-15°
const int D_FRONT_MAX = 25;  // ~45°
const int D_REAR_MIN = 75;   // ~135°
const int D_REAR_MAX = 108;  // ~195°
const int D_STEP = 8;        // ~15°

bool initialized = false;
bool scanning_down = true;
bool rear_scanning = false;
uint scanning_progress = 0;

int current_h = H_MAX;
int current_d = D_FRONT_MAX;

bool scan_in_progress = false;

camera_config_t config;

// Ввімкнення/вимкнення драйверів
void enable_stepper_h() 
{ 
  digitalWrite(stepper_h_enabled_pin, LOW); 
}
void disable_stepper_h() 
{
 digitalWrite(stepper_h_enabled_pin, HIGH); 
}
void enable_stepper_d() 
{ 
  digitalWrite(stepper_d_enabled_pin, LOW); 
}
void disable_stepper_d() 
{ 
  digitalWrite(stepper_d_enabled_pin, HIGH); 
}


// Спалах
void turnOnFlashing() 
{
  digitalWrite(LED_FLASH, HIGH);
}

void turnOffFlashing()
{
  digitalWrite(LED_FLASH, LOW);
}

// Рух двигуна з автоматичним увімкненням
void moveStepper(AccelStepper &stepper, int position, int enable_pin) 
{
  digitalWrite(enable_pin, LOW);
  stepper.moveTo(position);
}

void initCamera() 
{
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = 5;
  config.pin_d1 = 18;
  config.pin_d2 = 19;
  config.pin_d3 = 21;
  config.pin_d4 = 36;
  config.pin_d5 = 39;
  config.pin_d6 = 34;
  config.pin_d7 = 35;
  config.pin_xclk = 0;
  config.pin_pclk = 22;
  config.pin_vsync = 25;
  config.pin_href = 23;
  config.pin_sscb_sda = 26;
  config.pin_sscb_scl = 27;
  config.pin_pwdn = 32;
  config.pin_reset = -1;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_RGB565; //PIXFORMAT_RGB565 | PIXFORMAT_JPEG

  // QVGA = 320x240
  config.frame_size = FRAMESIZE_QVGA;
  config.jpeg_quality = 10;
  config.fb_count = 1;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) 
  {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  Serial.println("Camera init success");
}

bool capturePhotoSaveSpiffs() 
{
  turnOnFlashing();
  delay(200);
  camera_fb_t * fb = esp_camera_fb_get();
  delay(100);
  turnOffFlashing();
  if (!fb) 
  {
    Serial.println("Camera capture failed");
    return false;
  }

  File file = SPIFFS.open("/last.jpg", FILE_WRITE);
  if (!file) 
  {
    Serial.println("Failed to open file in writing mode");
    esp_camera_fb_return(fb);
    return false;
  }

  file.write(fb->buf, fb->len);
  file.close();
  esp_camera_fb_return(fb);

  Serial.println("Photo saved to /last.jpg");
  return true;
}

// Функція конвертації RGB565 формату в JPEG
bool rgb565_to_jpeg(uint8_t *rgb565_data, size_t rgb565_len, int width, int height, const char* filename) 
{
    uint8_t *jpg_buf = NULL;
    size_t jpg_len = 0;

    // Перевіряємо вхідні параметри
    if (!rgb565_data || rgb565_len == 0 || width == 0 || height == 0 || !filename) 
    {
        Serial.println("Помилка конвертації: некоректні параметри");
        return false;
    }

    // fmt2jpg — функція з esp_camera.h, яка кодує RGB565 у JPEG
    bool ok = fmt2jpg(rgb565_data, rgb565_len, width, height, PIXFORMAT_RGB565, 80, &jpg_buf, &jpg_len);

    if (!ok || jpg_buf == NULL || jpg_len == 0) 
    {
        Serial.println("Помилка конвертації RGB565 в JPEG");
        if (jpg_buf) free(jpg_buf);
        return false;
    }

    // Перевіряємо, чи є файлова система
    if (!SPIFFS.begin(true)) 
    {
        Serial.println("Помилка монтування SPIFFS");
        free(jpg_buf);
        return false;
    }

    File file = SPIFFS.open(filename, FILE_WRITE);
    if (!file) 
    {
        Serial.println("Не вдалося відкрити файл для запису");
        free(jpg_buf);
        return false;
    }

    // Записуємо з перевіркою помилок
    size_t written = file.write(jpg_buf, jpg_len);
    file.close();
    free(jpg_buf);

    if (written != jpg_len) 
    {
        Serial.println("Помилка запису: записано не всі дані");
        return false;
    }

    Serial.print("Зображення успішно збережено як JPEG. Розмір: ");
    Serial.println(jpg_len);
    return true;
}

// Функція для обведення участків з виявленим спалахом
void draw_rectangle_rgb565(uint8_t* buf, int width, int height, int x0, int y0, int x1, int y1) 
{
  // yellow color - maximum brigthness
  uint16_t color = 0xFFE0; // yell (0xFFE0 = red + green)
  
  // Перевірка кордонів зони
  x0 = max(0, min(width-1, x0));
  y0 = max(0, min(height-1, y0));
  x1 = max(0, min(width-1, x1));
  y1 = max(0, min(height-1, y1));
  
  // Малювання горизонтальних ліній
  for (int x = x0; x <= x1; x++) 
  {
    // Верхня горизонталь
    uint16_t* pixel = (uint16_t*)(buf + ((y0 * width + x) * 2));
    *pixel = color;
    
    // Нижня горизонталь
    pixel = (uint16_t*)(buf + ((y1 * width + x) * 2));
    *pixel = color;
  }
  
  // Малювання вертикальних ліній
  for (int y = y0; y <= y1; y++) 
  {
    // Ліва вертикаль
    uint16_t* pixel = (uint16_t*)(buf + ((y * width + x0) * 2));
    *pixel = color;
    
    // Права вертикаль
    pixel = (uint16_t*)(buf + ((y * width + x1) * 2));
    *pixel = color;
  }
}

bool detectFlash(camera_fb_t* fb) 
{
  int width = fb->width;
  int height = fb->height;
  uint16_t* buf = (uint16_t*)fb->buf;
  
  // Максимальна кількість відблисків для виявлення
  const int MAX_FLASHES = 3; // 3 для економії
  
  // Масиви для зберігання координат відблисків
  int min_x[MAX_FLASHES], min_y[MAX_FLASHES], max_x[MAX_FLASHES], max_y[MAX_FLASHES];
  bool flash_active[MAX_FLASHES];
  int flash_count = 0;
  
  // Ініціалізація масивів
  for (int i = 0; i < MAX_FLASHES; i++) 
  {
    min_x[i] = width;
    min_y[i] = height;
    max_x[i] = 0;
    max_y[i] = 0;
    flash_active[i] = false;
  }
  
  // Виділення пам'яті динамічним методом
  uint8_t* processed = (uint8_t*)malloc(width * height);
  if (!processed) 
  {
    Serial.println("Помилка виділення пам'яті для processed");
    return false; // Не вдалося виділити пам'ять
  }
  memset(processed, 0, width * height);
  
  // Пошук яскравих пікселів та поєднання їх за відстаню
  for (int y = 0; y < height; y++) 
  {
    for (int x = 0; x < width; x++) 
    {
      // Піксель перевірений?
      if (processed[y * width + x]) continue;
      
      uint16_t pixel = buf[y * width + x];
      
      // Отримуємо компоненти RGB
      uint8_t r = (pixel >> 11) & 0x1F; // 5 біт для червоного
      uint8_t g = (pixel >> 5) & 0x3F;  // 6 біт для зеленого
      uint8_t b = pixel & 0x1F;         // 5 біт для синього
      
      // Нормалізація компонентів RGB від 0 до 255
      r = (r * 255) / 31;
      g = (g * 255) / 63;
      b = (b * 255) / 31;
      
      // --- СТАРА ПЕРЕВІРКА ЯСКРАВОСТІ ---
      // int brightness = (r * 0.299 + g * 0.587 + b * 0.114);
      // if (brightness > BRIGHTNESS_THRESHOLD && 
      //     r > COLOR_CHANNEL_THRESHOLD && 
      //     g > COLOR_CHANNEL_THRESHOLD && 
      //     b > COLOR_CHANNEL_THRESHOLD) 
      // {
      //   ...
      // }
      // --- КІНЕЦЬ СТАРОЇ ПЕРЕВІРКИ ---

      // --- НОВА ПЕРЕВІРКА ДЛЯ ЧЕРВОНОГО ВІДБЛИСКУ ---
      if (
        r > COLOR_CHANNEL_THRESHOLD && // Червоний канал високий
        g < r * 0.3 &&                 // Зелений значно менший за червоний
        b < r * 0.3                    // Синій значно менший за червоний
      )
      {
        // Мітка на піксель
        processed[y * width + x] = 1;
        
        // Перевірка або додавання пікселя
        bool added_to_existing = false;
        for (int i = 0; i < flash_count; i++) 
        {
          if (!flash_active[i]) continue;
          
          // Якщо піксель близько до іншого такого ж, об'єднати їх
          if (x >= min_x[i] - 10 && x <= max_x[i] + 10 && 
              y >= min_y[i] - 10 && y <= max_y[i] + 10) 
          {
            min_x[i] = min(min_x[i], x);
            min_y[i] = min(min_y[i], y);
            max_x[i] = max(max_x[i], x);
            max_y[i] = max(max_y[i], y);
            added_to_existing = true;
            break;
          }
        }
        
        // Якщо ні - створити нову групу
        if (!added_to_existing && flash_count < MAX_FLASHES) 
        {
          min_x[flash_count] = x;
          min_y[flash_count] = y;
          max_x[flash_count] = x;
          max_y[flash_count] = y;
          flash_active[flash_count] = true;
          flash_count++;
        }
      }
      // --- КІНЕЦЬ НОВОЇ ПЕРЕВІРКИ ---
    }
  }
  
  // Вивільнення пам яті
  free(processed);
  
  // Перевірка всіх відблисків
  bool any_valid_flash = false;
  
  for (int i = 0; i < flash_count; i++) 
  {
    if (flash_active[i]) 
    {
      int area = (max_x[i] - min_x[i] + 1) * (max_y[i] - min_y[i] + 1);
      float aspect_ratio = float(max(max_x[i] - min_x[i] + 1, max_y[i] - min_y[i] + 1)) / 
                          float(min(max_x[i] - min_x[i] + 1, max_y[i] - min_y[i] + 1));
      
      // Перевірка чи існує відблиск 
      if (area > MAX_GLARE_AREA || area < MIN_GLARE_AREA || aspect_ratio > MAX_ASPECT_RATIO) 
      {
        flash_active[i] = false;
        Serial.print("Відблиск #"); Serial.print(i+1);
        Serial.println(" відкинуто через розмір/форму");
        Serial.print("Площа: "); Serial.print(area);
        Serial.print(", Співвідношення сторін: "); Serial.println(aspect_ratio);
      } 
      else 
      {
        any_valid_flash = true;
        Serial.print("Відблиск #"); Serial.print(i+1);
        Serial.println(" підтверджено!");
        Serial.print("Координати: (");
        Serial.print(min_x[i]); Serial.print(","); Serial.print(min_y[i]); Serial.print(") - (");
        Serial.print(max_x[i]); Serial.print(","); Serial.print(max_y[i]); Serial.println(")");
        Serial.print("Площа: "); Serial.print(area);
        Serial.print(", Співвідношення сторін: "); Serial.println(aspect_ratio);
        
        // Виділяємо зону, яку слід обвести
        int draw_min_x = max(0, min_x[i] - 5);
        int draw_min_y = max(0, min_y[i] - 5);
        int draw_max_x = min(width - 1, max_x[i] + 5);
        int draw_max_y = min(height - 1, max_y[i] + 5);
        
        // Обведенням жовтим прямокутником зони з підозрілим відблиском
        draw_rectangle_rgb565(fb->buf, width, height, draw_min_x, draw_min_y, draw_max_x, draw_max_y);
      }
    }
  }
  
  // Зберігаємо фото в JPEG форматі
  bool saved = rgb565_to_jpeg(fb->buf, fb->len, width, height, "/last.jpg");
  if (!saved) 
  {
    Serial.println("Помилка збереження зображення");
  }
  
  if (!any_valid_flash) 
  {
    Serial.println("Відблисків не знайдено або всі потенційні відблиски відкинуто");
    return false;
  }
  
  return true;
}

void photoHandler()
{
  Serial.println("Початок фотографування...");
  
  // Перевіряємо доступну пам'ять перед обробкою
  Serial.print("Вільна пам'ять перед обробкою: "); 
  Serial.println(ESP.getFreeHeap());
  
  // Перезапускаємо камеру для надійності
  /*esp_camera_deinit();
  delay(100);
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) 
  {
    Serial.printf("Помилка ініціалізації камери: 0x%x\n", err);
    return;
  }*/
  
  turnOnFlashing();
  delay(250);
  camera_fb_t* fb = esp_camera_fb_get();
  delay(150);
  turnOffFlashing();
  
  if (!fb) 
  {
    Serial.println("Помилка захоплення зображення");
    return;
  }
  
  // Перевіряємо розмір отриманого буфера
  if (fb->len == 0 || fb->width == 0 || fb->height == 0) 
  {
    Serial.println("Отримано некоректне зображення");
    esp_camera_fb_return(fb);
    return;
  }
  
  Serial.println("Фото зроблено, аналізую на наявність відблисків...");
  Serial.print("Розмір буфера: "); Serial.println(fb->len);
  Serial.print("Розміри зображення: "); Serial.print(fb->width); Serial.print("x"); Serial.println(fb->height);
  
  // Спроба аналізу зображення на наявність відблиску
  bool flash_found = false;
  
  // Використовуємо try-catch-подібний підхід з обробкою помилок
  // ESP32 не підтримує справжній try-catch, тому робимо це через повернення помилок
  flash_found = detectFlash(fb);
  
  // Виводимо результат
  if (flash_found) 
  {
    Serial.println("УВАГА! Виявлено потенційну приховану камеру!");
    Serial.println("Перевірте фото для детальної інформації");
    digitalWrite(LED_BUILDIN, LOW); // Блимаємо світлодіодом як сигнал тривоги
    
    // Встановлюємо глобальну змінну для зупинки сканування
    isFlashDetected = true;
    Serial.println("Сканування призупинено. Очікуємо підтвердження від користувача.");
    
    // Блимаємо світлодіодом декілька разів (але коротше, щоб не затримувати обробку)
    for (int i = 0; i < 2; i++) 
    {
      digitalWrite(LED_BUILDIN, HIGH);
      delay(50);
      digitalWrite(LED_BUILDIN, LOW);
      delay(50);
    }
  } else {
    Serial.println("Камеру не виявлено");
    digitalWrite(LED_BUILDIN, HIGH); // Вимкнути світлодіод
  }
  
  // Звільняємо буфер фрейму
  esp_camera_fb_return(fb);
  
  // Перевіряємо пам'ять після обробки
  Serial.print("Вільна пам'ять після обробки: ");
  Serial.println(ESP.getFreeHeap());
  
  Serial.println("Аналіз фото завершено");
}

void setup() {
  Serial.begin(115200);

  stepper_h.setMaxSpeed(100.0);
  stepper_h.setAcceleration(50.0);

  stepper_d.setMaxSpeed(50.0);
  stepper_d.setAcceleration(50.0);

  pinMode(LED_FLASH, OUTPUT);
  digitalWrite(LED_FLASH, LOW);
  pinMode(LED_BUILDIN, OUTPUT);
  digitalWrite(LED_BUILDIN, HIGH);

  pinMode(stepper_h_enabled_pin, OUTPUT);
  disable_stepper_h();
  pinMode(stepper_d_enabled_pin, OUTPUT);
  disable_stepper_d();

  Serial.println("Stepper drivers initialized.");
  //Flashing(10);

  // Initialize SPIFFS 
  if (!SPIFFS.begin(true)) 
  {
      Serial.println("An error occurred while mounting SPIFFS");
      return;
    }

  // Initialize camera
  Serial.println("Camera initialization started...");
  initCamera();
  
  // Initialize WiFi access point
  WiFi.softAPConfig(local_IP, gateway, subnet);
  WiFi.softAP(WiFi_SSID, WiFi_PASSWORD);
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

  Serial.println("AP is configured...");

  Serial.print("Waiting for client connection to AP...");

  while (WiFi.softAPgetStationNum() == 0) 
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nClient connected to AP!");

  // Route webserver pages
  webserver.on("/", HTTP_GET, []() 
  {
    webserver.send_P(200, "text/html", index_page);
  });

  webserver.on("/start-scan", HTTP_GET, []() 
  {
    if (!scan_in_progress) {
      scan_in_progress = true;
      initialized = false; // щоб переініціалізувалося
      webserver.send(200, "text/plain", "Scanning started");
    } else {
      webserver.send(200, "text/plain", "Already scanning");
    }
  });

  webserver.on("/photo", HTTP_GET, []()
  {
    File file = SPIFFS.open("/last.jpg", "r");
    if (!file || file.isDirectory()) {
      webserver.send(404, "text/plain", "Photo not found");
      return;
    }

    webserver.streamFile(file, "image/jpeg");
    file.close();
  });

  webserver.on("/status", HTTP_GET, []()
  {
    if (scan_in_progress) {
      webserver.send(200, "text/plain", "true");
    } else {
      webserver.send(200, "text/plain", "false");
    }
  });

  webserver.on("/progress", HTTP_GET, []()
  {
    webserver.send_P(200, "text/plain", String(scanning_progress).c_str());
  });

  webserver.on("/reset-flash-detection", HTTP_GET, []()
  {
    isFlashDetected = false;
    webserver.send(200, "text/plain", "Сканування відновлено");
    Serial.println("Відновлено сканування після виявлення відблиску");
  });
  
  // Маршрут для продовження після підтвердження
  webserver.on("/continue-scan", HTTP_GET, []()
  {
    if (isFlashDetected) 
    {
      isFlashDetected = false;
      webserver.send(200, "text/plain", "Сканування продовжено");
      Serial.println("Продовжуємо сканування після підтвердження виявлення");
    } else {
      webserver.send(200, "text/plain", "Сканування не було зупинено");
    }
  });
  
  // Маршрут для перевірки, чи виявлено відблиск
  webserver.on("/is-flash-detected", HTTP_GET, []()
  {
    webserver.send(200, "text/plain", isFlashDetected ? "true" : "false");
  });

   // Маршрут для зупинки сканування
  webserver.on("/stop-scanning", HTTP_GET, []()
  {
    scan_in_progress = false;
    webserver.send(200, "text/plain", "Сканування зупинено");
    Serial.println("Сканування зупинено через запит користувача.");
  });
  
  // Webserver configuration
  webserver.begin();

  Serial.println("WebServer is ON...");

  // End of the setup
  Serial.println("Setup complete.");
}

void loop() 
{
  dnsServer.processNextRequest();
  webserver.handleClient();

  if (!scan_in_progress) 
  {
    Serial.println("Очікуємо на користувачу для старту...");
    delay(500);
    return;
  }
  
  // Перевіряємо, чи було виявлено відблиск
  if (isFlashDetected) 
  {    
    static unsigned long lastBlink = 0;
    static unsigned long lastBeep = 0;
    unsigned long currentMillis = millis();
    
    if (currentMillis - lastBlink > 300) { // Швидше блимаємо для привертання уваги
      lastBlink = currentMillis;
      digitalWrite(LED_BUILDIN, !digitalRead(LED_BUILDIN));
    }
    
    delay(10); // Короткий delay для кращої відповіді веб-сервера
    return;
  }

  if (!initialized) 
  {
    Serial.println("Ініціалізація: рухаюсь до початкових позицій...");
    moveStepper(stepper_h, H_MAX, stepper_h_enabled_pin);
    moveStepper(stepper_d, D_FRONT_MAX, stepper_d_enabled_pin);

    while (stepper_h.distanceToGo() != 0 || stepper_d.distanceToGo() != 0) 
    {
      stepper_h.run();
      stepper_d.run();
    }

    disable_stepper_h();
    disable_stepper_d();

    current_h = H_MAX;
    current_d = D_FRONT_MAX;
    initialized = true;
    Serial.println("Ініціалізацію завершено.");
    
    // Додаємо очищення пам'яті і перезавантаження системи на початку сканування
    Serial.print("Очищення пам'яті. Доступно: ");
    Serial.println(ESP.getFreeHeap());
    return;
  }

  stepper_h.run();
  stepper_d.run();

  if (stepper_h.distanceToGo() == 0 && stepper_d.distanceToGo() == 0) 
  {
    disable_stepper_h();
    disable_stepper_d();

    Serial.print("Позиція досягла: H=");
    Serial.print(current_h);
    Serial.print(" D=");
    Serial.println(current_d);

    Serial.println("Фотографування фото...");
    photoHandler();
    scanning_progress += 1;
    delay(400); // Зменшуємо затримку для пришвидшення сканування
    
    // Додаємо очищення пам'яті після кожної фотографії
    if (scanning_progress % 5 == 0) 
    { // Кожні 5 фото
      Serial.print("Проміжне очищення пам'яті. Доступно: ");
      Serial.println(ESP.getFreeHeap());
    }

    if (scanning_down) 
    {
      current_d -= D_STEP;
      if ((rear_scanning && current_d < D_REAR_MIN) || (!rear_scanning && current_d < D_FRONT_MIN)) 
      {
        // Коли досягли нижньої межі, повертаємося на початкову позицію
        current_d = rear_scanning ? D_REAR_MAX : D_FRONT_MAX;
        moveStepper(stepper_d, current_d, stepper_d_enabled_pin);
        
        // Змінюємо горизонтальну позицію
        current_h -= H_STEP;
        if (current_h < H_MIN) 
        {
          if (!rear_scanning) 
          {
            Serial.println("Перемикання на заднє сканування...");
            rear_scanning = true;
            current_h = H_MAX;
            current_d = D_REAR_MAX;
          } 
          else 
          {
            Serial.println("СКАНУВАННЯ ЗАВЕРШЕНО. Повертання на стартову позицію...");
            moveStepper(stepper_h, H_CENTER, stepper_h_enabled_pin);
            moveStepper(stepper_d, D_FRONT_MAX, stepper_d_enabled_pin);
            while (stepper_h.isRunning() || stepper_d.isRunning()) 
            {
              stepper_h.run();
              stepper_d.run();
            }
            disable_stepper_h();
            disable_stepper_d();

            // Скидаємо все
            scan_in_progress = false;
            rear_scanning = false;
            initialized = false;
            scanning_progress = 0;
            Serial.println("ЗАВЕРШЕНО! Очікування на дії користувача...");
            return;
          }
        }
        moveStepper(stepper_h, current_h, stepper_h_enabled_pin);
      }
    }

    moveStepper(stepper_d, current_d, stepper_d_enabled_pin);
  }
}