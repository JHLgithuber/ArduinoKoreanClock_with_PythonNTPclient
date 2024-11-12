#include <RTClib.h>
#include <Adafruit_NeoPixel.h>
#include <Time.h>
#include <ArduinoJson.h>

#define PIN 6         // LED 스트립이 연결된 핀 번호
#define NUMPIXELS 36  // LED의 개수

RTC_DS3231 rtc;
Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);


#define MAX_CUSTOM_PRESETS 50
#define MAX_TIME_PRESETS 30


int showingMinute = 0;
int showingHour = 0;
bool hourlyNotificationSet = true;  //정시알림, 수정필요
int blinkTime = 500;

struct RGBstruct {
  uint8_t r, g, b;
};

struct ColorPresetStruct {
  uint8_t priority;           // 우선순위
  uint8_t ledID_bitmask;      // 비트마스크, 색상을 적용할 위치, 오전,오후,자정,정오,시_시각,시_접미사,분_시각,분_접미사
  uint8_t startIndexTime;     // 1일을 10분 단위로 쪼갬
  uint8_t endIndexTime;       // 1일을 10분 단위로 쪼갬
  uint8_t dayOfWeek_bitmask;  // 비트마스크, 적용할 요일
  RGBstruct ledColor;
};

int freeMemory() {
  extern int __heap_start, *__brkval;
  int v;
  return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}

void print_freeMemory() {
  Serial.print("Free memory before adding preset: ");
  Serial.println(freeMemory());
}

uint8_t get_IndexTime(DateTime dt) {
  // 총 몇 번째 10분 단위인지 계산
  uint8_t IndexTime = (dt.hour() * 60 + dt.minute()) / 10;
  return IndexTime;
}




class ColorPreset {
protected:
  void makeColorPresetArr(int size) {
    maxSize = size;
    colorPresetArr = new ColorPresetStruct[maxSize];  // 동적 배열 할당
  }
  void purgeColorPresetArr() {
    delete[] colorPresetArr;  // 메모리 해제
  }

  int colorPresetSize = 0;
  int maxSize = 0;

public:
  ColorPresetStruct* colorPresetArr;  // 동적 배열 포인터

  int get_colorPresetSize() const {
    return colorPresetSize;
  }
  int get_maxSize() const {
    return maxSize;
  }

  void sortPresetArr() {
    for (int i = 0; i < colorPresetSize - 1; i++) {
      for (int j = 0; j < colorPresetSize - i - 1; j++) {
        if (colorPresetArr[j].priority > colorPresetArr[j + 1].priority) {
          // 두 요소를 교환
          ColorPresetStruct temp = colorPresetArr[j];
          colorPresetArr[j] = colorPresetArr[j + 1];
          colorPresetArr[j + 1] = temp;
        }
      }
    }
  }

  String* get_LedID_arr(ColorPresetStruct preset) {
    static String led_id_arr[8];
    int index = 0;

    // 각 비트에 해당하는 ID를 배열에 추가
    if (preset.ledID_bitmask & 0b00000001) led_id_arr[index++] = "오전";
    if (preset.ledID_bitmask & 0b00000010) led_id_arr[index++] = "오후";
    if (preset.ledID_bitmask & 0b00000100) led_id_arr[index++] = "자정";
    if (preset.ledID_bitmask & 0b00001000) led_id_arr[index++] = "정오";
    if (preset.ledID_bitmask & 0b00010000) led_id_arr[index++] = "시_시각";
    if (preset.ledID_bitmask & 0b00100000) led_id_arr[index++] = "시_접미사";
    if (preset.ledID_bitmask & 0b01000000) led_id_arr[index++] = "분_시각";
    if (preset.ledID_bitmask & 0b10000000) led_id_arr[index++] = "분_접미사";

    // 나머지 요소는 빈 문자열로 초기화
    for (int i = index; i < 8; i++) {
      led_id_arr[i] = "";
    }

    return led_id_arr;
  }

  bool isContains_ledID(ColorPresetStruct preset, String target) {
    String* led_id_arr = get_LedID_arr(preset);

    for (int i = 0; i < 8; i++) {
      if (led_id_arr[i] == target) {
        return true;  // 배열에 해당 문자열이 있음
      }
    }
    return false;  // 배열에 해당 문자열이 없음
  }

  bool isContain_dayOfWeek(ColorPresetStruct preset, int target) {
    int* dayOfWeek_arr = get_dayOfWeek_arr(preset);

    for (int i = 0; i < 8; i++) {
      if (dayOfWeek_arr[i] == -1) break;            // 유효한 값이 없으므로 반복문 종료
      if (dayOfWeek_arr[i] == target) return true;  // 배열에 해당 값이 있음
    }
    return false;  // 배열에 해당 값이 없음
  }

  int* get_dayOfWeek_arr(ColorPresetStruct preset) {
    static int dayOfWeek_arr[8];
    int index = 0;

    if (preset.dayOfWeek_bitmask & 0b00000001) dayOfWeek_arr[index++] = 0;  //일요일
    if (preset.dayOfWeek_bitmask & 0b00000010) dayOfWeek_arr[index++] = 1;  //월요일
    if (preset.dayOfWeek_bitmask & 0b00000100) dayOfWeek_arr[index++] = 2;  //화요일
    if (preset.dayOfWeek_bitmask & 0b00001000) dayOfWeek_arr[index++] = 3;  //수요일
    if (preset.dayOfWeek_bitmask & 0b00010000) dayOfWeek_arr[index++] = 4;  //목요일
    if (preset.dayOfWeek_bitmask & 0b00100000) dayOfWeek_arr[index++] = 5;  //금요일
    if (preset.dayOfWeek_bitmask & 0b01000000) dayOfWeek_arr[index++] = 6;  //토요일
    if (preset.dayOfWeek_bitmask & 0b10000000) dayOfWeek_arr[index++] = 7;  //일요일(안씀)

    for (int i = index; i < 8; i++) {
      dayOfWeek_arr[i] = -1;  // 사용되지 않는 값 표시
    }

    return dayOfWeek_arr;
  }


  // 실질적 구조체 배열 추가 함수 - 모든 비트마스크와 우선순위를 사용
  bool addColorPreset(uint8_t priority, uint8_t ledID_bitmask, uint8_t startIndexTime, uint8_t endIndexTime, int dayOfWeek_bitmask, RGBstruct ledColor) {
    if (colorPresetSize >= maxSize) {
      Serial.println("메모리가 가득 찼습니다.");
      return false;  // 배열이 가득 찼습니다.
    }

    // 새로운 항목 추가
    colorPresetArr[colorPresetSize] = { priority, ledID_bitmask, startIndexTime, endIndexTime, dayOfWeek_bitmask, ledColor };
    colorPresetSize++;
    delay(100);
    return true;
  }
  // LED ID 배열을 받아 비트마스크로 변환 후 호출하는 함수
  bool addColorPreset(uint8_t priority, String led_select_id_arr[], uint8_t startIndexTime, uint8_t endIndexTime, uint8_t dayOfWeek_bitmask, RGBstruct ledColor) {
    uint8_t ledID_bitmask = 0;
    for (int i = 0; i < (sizeof(led_select_id_arr) / sizeof(led_select_id_arr[0])); i++) {
      if (led_select_id_arr[i] == "오전") ledID_bitmask |= 0b00000001;
      if (led_select_id_arr[i] == "오후") ledID_bitmask |= 0b00000010;
      if (led_select_id_arr[i] == "자정") ledID_bitmask |= 0b00000100;
      if (led_select_id_arr[i] == "정오") ledID_bitmask |= 0b00001000;
      if (led_select_id_arr[i] == "시_시각") ledID_bitmask |= 0b00010000;
      if (led_select_id_arr[i] == "시_접미사") ledID_bitmask |= 0b00100000;
      if (led_select_id_arr[i] == "분_시각") ledID_bitmask |= 0b01000000;
      if (led_select_id_arr[i] == "분_접미사") ledID_bitmask |= 0b10000000;
    }
    return addColorPreset(priority, ledID_bitmask, startIndexTime, endIndexTime, dayOfWeek_bitmask, ledColor);
  }
  // dayOfWeek 배열을 받아 비트마스크로 변환 후 호출하는 함수
  bool addColorPreset(uint8_t priority, String led_select_id_arr[], uint8_t startIndexTime, uint8_t endIndexTime, int dayOfWeek_arr[], RGBstruct ledColor) {
    uint8_t dayOfWeek_bitmask = 0;
    for (int i = 0; dayOfWeek_arr[i] != -1; i++) {
      if (dayOfWeek_arr[i] >= 0 && dayOfWeek_arr[i] <= 6) {
        dayOfWeek_bitmask |= (1 << dayOfWeek_arr[i]);
      }
    }
    return addColorPreset(priority, led_select_id_arr, startIndexTime, endIndexTime, dayOfWeek_bitmask, ledColor);
  }
  // LED ID 없이 dayOfWeek 배열을 비트마스크로 변환 후 호출하는 함수
  bool addColorPreset(uint8_t priority, uint8_t startIndexTime, uint8_t endIndexTime, int dayOfWeek_arr[], RGBstruct ledColor) {
    String default_led_select_id_arr[] = { "" };
    return addColorPreset(priority, default_led_select_id_arr, startIndexTime, endIndexTime, dayOfWeek_arr, ledColor);
  }
  // LED ID 없이 dayOfWeek 비트마스크만 받아 호출하는 함수
  bool addColorPreset(uint8_t priority, uint8_t startIndexTime, uint8_t endIndexTime, uint8_t dayOfWeek_bitmask, RGBstruct ledColor) {
    return addColorPreset(priority, (uint8_t)0, startIndexTime, endIndexTime, dayOfWeek_bitmask, ledColor);
  }
};

class ColorCustomPreset : public ColorPreset {
public:
  ColorCustomPreset(int maxSize) {
    makeColorPresetArr(maxSize);  // 배열 크기 설정
  }
  ~ColorCustomPreset() {
    purgeColorPresetArr();
  }

  void reset_colorPresetArr() {
    // 초기화: 모든 요소를 기본값으로 설정
    colorPresetSize = 0;

    // "새벽" 프리셋 - LED ID 마스크 적용 (부드러운 주황색)
    addColorPreset(
      1,
      0b10000001,      // 오전, 시_시각 활성화
      6,               // startIndexTime: 01:00
      18,              // endIndexTime: 03:00
      0b1111111,       // 일요일부터 토요일까지
      { 255, 140, 0 }  // 부드러운 주황색
    );

    // "아침" 프리셋 - LED ID 마스크 적용 (밝은 노란색)
    addColorPreset(
      2,
      0b11111100,      // 오전, 시_시각, 시_접미사 활성화
      42,              // startIndexTime: 07:00
      60,              // endIndexTime: 10:00
      0b0011111,       // 월요일부터 금요일까지
      { 255, 223, 0 }  // 밝은 노란색
    );

    // "대낮" 프리셋 - LED ID 마스크 적용 (밝은 흰색, 최대 밝기)
    addColorPreset(
      3,
      0b11111111,        // 모든 위치 활성화
      60,                // startIndexTime: 10:00
      96,                // endIndexTime: 16:00
      0b1111111,         // 일요일부터 토요일까지
      { 255, 255, 255 }  // 최대 밝기의 흰색
    );

    // "저녁" 프리셋 - LED ID 마스크 적용 (따뜻한 핑크색)
    addColorPreset(
      4,
      0b00001010,        // 오후, 정오 활성화
      108,               // startIndexTime: 18:00
      120,               // endIndexTime: 20:00
      0b0111111,         // 월요일부터 금요일까지
      { 255, 105, 180 }  // 따뜻한 핑크색
    );

    // "한밤" 프리셋 - LED ID 마스크 적용 (어두운 빨간색)
    addColorPreset(
      5,
      0b11110000,   // 시_시각, 시_접미사, 자정, 한밤 활성화
      126,          // startIndexTime: 21:00
      6,            // endIndexTime: 03:00 (다음 날 새벽 3시)
      0b0111111,    // 월요일부터 금요일까지
      { 55, 0, 0 }  // 아주 어두운 빨간색
    );

    sortPresetArr();
  }
};

class ColorTimePreset : public ColorPreset {
public:
  ColorTimePreset(int maxSize) {
    makeColorPresetArr(maxSize);  // 배열 크기 설정
  }
  ~ColorTimePreset() {
    purgeColorPresetArr();
  }

  void reset_colorPresetArr() {
    // 초기화: 모든 요소를 기본값으로 설정
    colorPresetSize = 0;

    // 새벽 시간대 기본 색상 (부드러운 황갈색)
    addColorPreset(
      1,
      6,                // startIndexTime: 01:00
      18,               // endIndexTime: 03:00
      0b1111111,        // 일요일부터 토요일까지
      { 210, 105, 30 }  // 부드러운 황갈색
    );

    // 아침 시간대 기본 색상 (밝은 오렌지색)
    addColorPreset(
      2,
      42,              // startIndexTime: 07:00
      60,              // endIndexTime: 10:00
      0b0011111,       // 월요일부터 금요일까지
      { 255, 165, 0 }  // 밝은 오렌지색
    );

    // 대낮 시간대 기본 색상 (밝은 흰색)
    addColorPreset(
      3,
      60,                // startIndexTime: 10:00
      96,                // endIndexTime: 16:00
      0b1111111,         // 일요일부터 토요일까지
      { 255, 255, 255 }  // 밝은 흰색, 최대 밝기
    );

    // 오후 시간대 기본 색상 (짙은 주황색)
    addColorPreset(
      4,
      96,              // startIndexTime: 16:00
      108,             // endIndexTime: 18:00
      0b0011111,       // 월요일부터 금요일까지
      { 255, 140, 0 }  // 짙은 주황색
    );

    // 저녁 시간대 기본 색상 (짙은 핑크색)
    addColorPreset(
      5,
      108,               // startIndexTime: 18:00
      120,               // endIndexTime: 20:00
      0b0111111,         // 월요일부터 금요일까지
      { 219, 112, 147 }  // 짙은 핑크색
    );

    // 한밤중 기본 색상 (아주 어두운 빨간색)
    addColorPreset(
      6,
      126,          // startIndexTime: 21:00
      6,            // endIndexTime: 03:00
      0b1111111,    // 일요일부터 토요일까지
      { 30, 0, 0 }  // 매우 어두운 빨간색
    );

    sortPresetArr();
  }
};


//객체 선언
ColorCustomPreset colorCustomPreset(MAX_CUSTOM_PRESETS);
ColorTimePreset colorTimePreset(MAX_TIME_PRESETS);



uint32_t led_off = pixels.Color(0, 0, 0);

uint32_t getColor() {  //기본 색상값 리턴
  return pixels.Color(255, 255, 255);
}
uint32_t getColor(DateTime now_dt) {  //시간단독대응 색상값 리턴
  uint8_t currentIndexTime = (now_dt.hour() * 60 + now_dt.minute()) / 10;
  int currentDayOfWeek = now_dt.dayOfTheWeek();

  // colorCustomPresetArr 배열을 순회하며 조건에 맞는 항목을 찾음
  for (int i = 0; i < colorTimePreset.get_colorPresetSize(); i++) {
    ColorPresetStruct preset = colorTimePreset.colorPresetArr[i];

    //Serial.print("Checking preset ");
    //Serial.print(i);
    //Serial.print(": ledID_bitmask: ");
    //Serial.println(preset.ledID_bitmask, BIN);

    // 현재 요일이 적용 가능한지 확인
    if (colorTimePreset.isContain_dayOfWeek(preset, currentDayOfWeek)) {
      //Serial.println("Day of week matched.");

      // 시간 범위가 같은 날 내에 있는 경우
      if (preset.startIndexTime <= preset.endIndexTime) {
        if (currentIndexTime >= preset.startIndexTime && currentIndexTime < preset.endIndexTime) {
          //Serial.println("Time matched.");

          //return preset.ledColor; // 조건을 만족하면 즉시 ledColor 반환

          RGBstruct RGBcolor = preset.ledColor;
          return pixels.Color(RGBcolor.r, RGBcolor.g, RGBcolor.b);
        }
      }
      // 시간 범위가 다음날로 넘어가는 경우 (예: 20시 ~ 익일 07시)
      else {
        if (currentIndexTime >= preset.startIndexTime || currentIndexTime < preset.endIndexTime) {
          //Serial.println("Time matched (next day).");

          //return preset.ledColor; // 조건을 만족하면 즉시 ledColor 반환

          RGBstruct RGBcolor = preset.ledColor;
          return pixels.Color(RGBcolor.r, RGBcolor.g, RGBcolor.b);
        }
      }
    }
  }

  // 조건을 만족하는 preset이 없으면 기본 색상 반환
  //Serial.println("No matching preset found. Returning default color.");
  return getColor();  //  기본 색상 반환
  //return led_off;
}
uint32_t getColor(String led_select_id, DateTime now_dt) {
  uint8_t currentIndexTime = (now_dt.hour() * 60 + now_dt.minute()) / 10;
  int currentDayOfWeek = now_dt.dayOfTheWeek();

  // colorCustomPresetArr 배열을 순회하며 조건에 맞는 항목을 찾음
  for (int i = 0; i < colorCustomPreset.get_colorPresetSize(); i++) {
    ColorPresetStruct preset = colorCustomPreset.colorPresetArr[i];

    //Serial.print("Checking preset ");
    //Serial.print(i);
    //Serial.print(": ledID_bitmask: ");
    //Serial.println(preset.ledID_bitmask, BIN);

    // 현재 요일이 적용 가능한지 확인
    if (colorCustomPreset.isContain_dayOfWeek(preset, currentDayOfWeek)) {
      //Serial.println("Day of week matched.");

      // 시간 범위가 같은 날 내에 있는 경우
      if (preset.startIndexTime <= preset.endIndexTime) {
        if (currentIndexTime >= preset.startIndexTime && currentIndexTime < preset.endIndexTime) {
          //Serial.println("Time matched.");

          // led_select_id가 포함되어 있는지 확인
          if (colorCustomPreset.isContains_ledID(preset, led_select_id)) {
            //Serial.println("LED select ID matched. Returning preset color.");
            //return preset.ledColor; // 조건을 만족하면 즉시 ledColor 반환

            RGBstruct RGBcolor = preset.ledColor;
            return pixels.Color(RGBcolor.r, RGBcolor.g, RGBcolor.b);
          }
        }
      }
      // 시간 범위가 다음날로 넘어가는 경우 (예: 20시 ~ 익일 07시)
      else {
        if (currentIndexTime >= preset.startIndexTime || currentIndexTime < preset.endIndexTime) {
          //Serial.println("Time matched (next day).");

          // led_select_id가 포함되어 있는지 확인
          if (colorCustomPreset.isContains_ledID(preset, led_select_id)) {
            //Serial.println("LED select ID matched. Returning preset color.");
            //return preset.ledColor; // 조건을 만족하면 즉시 ledColor 반환

            RGBstruct RGBcolor = preset.ledColor;
            return pixels.Color(RGBcolor.r, RGBcolor.g, RGBcolor.b);
          }
        }
      }
    }
  }

  // 조건을 만족하는 preset이 없으면 시간 단독 대응 색상 반환
  //Serial.println("No matching preset found. Returning default color.");
  return getColor(now_dt);  // 시간 단독 대응 기본 색상 반환
  //return led_off;
}




void flowWatchFace(DateTime now_dt) {
  for (int i = 0; i < NUMPIXELS; i++) {
    pixels.setPixelColor(i, getColor(now_dt));  // 모든 LED를 시간대응으로 설정
    pixels.show();
    delay(80);
  }
  for (int i = 0; i < NUMPIXELS; i++) {
    pixels.setPixelColor(i, led_off);  // 모든 LED를 꺼짐으로 설정
    pixels.show();
    delay(80);
  }
  delay(150);
}
void flowWatchFace() {
  for (int i = 0; i < NUMPIXELS; i++) {
    pixels.setPixelColor(i, getColor());  // 모든 LED를 흰색으로 설정
    pixels.show();
    delay(80);
  }
  for (int i = 0; i < NUMPIXELS; i++) {
    pixels.setPixelColor(i, led_off);  // 모든 LED를 꺼짐으로 설정
    pixels.show();
    delay(80);
  }
  delay(150);
}

void blinkWatchFace(DateTime now_dt) {
  pixels.fill(getColor(now_dt), 0, NUMPIXELS);
  pixels.show();
  delay(blinkTime);
  pixels.fill(led_off, 0, NUMPIXELS);
  pixels.show();

  showingMinute = 0;
  showingHour = 0;
}
void blinkWatchFace() {
  pixels.fill(getColor(), 0, NUMPIXELS);
  pixels.show();
  delay(blinkTime);
  pixels.fill(led_off, 0, NUMPIXELS);
  pixels.show();

  showingMinute = 0;
  showingHour = 0;
}



void jsonSerialProcesser(String data) {
  //**JSON**
  /*
  {
    "function": "preset_edit",  // "preset_edit"
    "presetType": "custom",  // "custom" 또는 "time" 중 하나의 값 사용
    "presetCurd": "create"   // "create", "update", "read", "delete"
    "presetData": [
      { //비트마스크는 JSON에서 사용하지 않음
        "priority": 1,  // 우선순위
        "ledID_bitmask": 129,  // 비트마스크 (0b10000001의 10진수 표현)
        "startIndexTime": 6,  // 1일을 10분 단위로 쪼갬 (0부터 143까지 가능)
        "endIndexTime": 18,  // 1일을 10분 단위로 쪼갬 (0부터 143까지 가능)
        "dayOfWeek_bitmask": 63,  // 비트마스크 (0b0111111의 10진수 표현), 월요일부터 토요일
        "ledColor": {
          "r": 255,
          "g": 140,
          "b": 100
        }
      },
      {
        "priority": 2,  // 우선순위
        "ledID_arr": ["오전", "오후", "자정", "정오", "시_시각", "시_접미사", "분_시각", "분_접미사"] // ""으로 끝남 및 남는공간 채움, 9개 요소
        "startIndexTime": 6,  // 1일을 10분 단위로 쪼갬
        "endIndexTime": 18,  // 1일을 10분 단위로 쪼갬
        "dayOfWeek_arr": [0,1,3,5,6,-1,-1,-1]  //-1로 끝남 및 남는공간 채움, 8개의 요소, 0부터 6까지=일요일부터 토요일까지
        "ledColor": {
          "r": 255,
          "g": 140,
          "b": 100
        }
      }
    ]
  }
  */
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, data);

  // Test if parsing succeeds.
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return;
  }

  JsonObject obj = doc.as<JsonObject>();

  if (obj["function"] == "preset_edit") {
    jsonPresetEdit(obj);
  }
}

void jsonPresetEdit(JsonObject obj) {
  JsonArray arr = obj["presetData"].as<JsonArray>();
  if (obj["presetType"] == "custom") {
    if (obj["presetCurd"] == "create") {
      jsonObjParser_add_colorCustomPreset(arr);
    }
  } else if (obj["presetType"] == "time") {
    if (obj["presetCurd"] == "create") {
      jsonObjParser_add_colorTimePreset(arr);
    }
  }
}

void jsonObjParser_add_colorCustomPreset(JsonArray presetData) {
  for (JsonObject item : presetData) {
    // JSON 배열 크기를 동적으로 확인하여 배열 생성
    int ledArraySize = item["ledID_arr"].size();
    int dayArraySize = item["dayOfWeek_arr"].size();

    // 동적으로 배열 생성
    String* led_select_id_arr = new String[ledArraySize];
    String* dayOfWeek_arr = new String[dayArraySize];

    // JSON 배열을 동적 String 배열로 복사
    copyArray(item["ledID_arr"].as<JsonArray>(), led_select_id_arr, ledArraySize);
    copyArray(item["dayOfWeek_arr"].as<JsonArray>(), dayOfWeek_arr, dayArraySize);

    colorCustomPreset.addColorPreset(
      item["priority"].as<uint8_t>(),           // 형 변환 추가
      led_select_id_arr,      // 형 변환 추가
      item["startIndexTime"].as<uint8_t>(),     // 형 변환 추가
      item["endIndexTime"].as<uint8_t>(),       // 형 변환 추가
      dayOfWeek_arr,  // 형 변환 추가
      (RGBstruct){
        item["ledColor"]["r"].as<uint8_t>(),  // 형 변환 추가
        item["ledColor"]["g"].as<uint8_t>(),  // 형 변환 추가
        item["ledColor"]["b"].as<uint8_t>()   // 형 변환 추가
      });
    colorCustomPreset.sortPresetArr();
  }
}

void jsonObjParser_add_colorTimePreset(JsonArray presetData) {
  for (JsonObject item : presetData) {
    // JSON 배열 크기를 동적으로 확인하여 배열 생성
    int dayArraySize = item["dayOfWeek_arr"].size();

    // 동적으로 배열 생성
    String* dayOfWeek_arr = new String[dayArraySize];

    // JSON 배열을 동적 String 배열로 복사
    copyArray(item["dayOfWeek_arr"].as<JsonArray>(), dayOfWeek_arr, dayArraySize);

    colorTimePreset.addColorPreset(             // colorCustomPreset 대신 colorTimePreset
      item["priority"].as<uint8_t>(),           // 형 변환 추가
      item["startIndexTime"].as<uint8_t>(),     // 형 변환 추가
      item["endIndexTime"].as<uint8_t>(),       // 형 변환 추가
      dayOfWeek_arr,  // 형 변환 추가
      (RGBstruct){
        item["ledColor"]["r"].as<uint8_t>(),  // 형 변환 추가
        item["ledColor"]["g"].as<uint8_t>(),  // 형 변환 추가
        item["ledColor"]["b"].as<uint8_t>()   // 형 변환 추가
      });
    colorTimePreset.sortPresetArr();
  }
}



void printDateTime(DateTime dt) {
  // 요일 문자열 배열
  const char* daysOfWeek[] = { "일요일", "월요일", "화요일", "수요일", "목요일", "금요일", "토요일" };
  int dayOfWeek = dt.dayOfTheWeek();  // 0 = 일요일, 6 = 토요일

  // DateTime 객체를 시리얼로 출력하는 함수
  Serial.print(dt.year());
  Serial.print('/');
  Serial.print(dt.month());
  Serial.print('/');
  Serial.print(dt.day());
  Serial.print("(");
  Serial.print(daysOfWeek[dayOfWeek]);
  Serial.print(") ");
  Serial.print(dt.hour());
  Serial.print(':');
  Serial.print(dt.minute());
  Serial.print(':');
  Serial.println(dt.second());
}

void printStringArray(String arr[], int size) {
  for (int i = 0; i < size; i++) {
    Serial.print(arr[i]);
    if (i < size - 1) {
      Serial.print(", ");  // 각 요소 사이에 쉼표 추가
    }
  }
  Serial.println();  // 배열 출력 후 줄 바꿈
}
void printIntArray(int arr[], int size) {
  for (int i = 0; i < size; i++) {
    Serial.print(arr[i]);
    if (i < size - 1) {
      Serial.print(", ");  // 각 요소 사이에 쉼표 추가
    }
  }
  Serial.println();  // 배열 출력 후 줄 바꿈
}



void adjust_by_UTCK(String rowStrData, int atIndex) {
  Serial.println("First character is '@', proceeding with UTCK.");
  if (rowStrData.length() >= 8) {
    int newHour = rowStrData.substring(atIndex + 1, atIndex + 3).toInt();    // 시간
    int newMinute = rowStrData.substring(atIndex + 3, atIndex + 5).toInt();  // 분
    int newSecond = rowStrData.substring(atIndex + 5, atIndex + 7).toInt();  // 초

    // 현재 날짜를 유지하고 시간만 변경
    DateTime now = rtc.now();
    DateTime adjustedTime(now.year(), now.month(), now.day(), newHour, newMinute, newSecond);
    rtc.adjust(adjustedTime);

    Serial.print("Adjusted by UTCK3: ");
    printDateTime(adjustedTime);  // 수정된 시간 출력
  } else {
    Serial.println("Error: Input data is too short.");
  }
}


void ProcessingSerial() {
  if (Serial.available() > 0) {
    String rowStrData = Serial.readStringUntil('\n');
    Serial.println("Received Serial Data: " + rowStrData);

    // UTCK3 연동, 첫 문자가 '@'인지 확인
    int atIndex = rowStrData.indexOf('@');
    if (atIndex != -1) {
      adjust_by_UTCK(rowStrData, atIndex);
      //blinkWatchFace(rtc.now());

    } else {
      Serial.println("Error: First character is not '@'.");
    }
  }
}

void refreshWatchFace(DateTime dt) {
  pixels.clear();
  int h = dt.hour();
  int m = dt.minute();
  if (h > 0 && h < 12) {  // 오전
    pixels.setPixelColor(0, getColor("오전", dt));
    pixels.setPixelColor(1, getColor("오전", dt));
    pixels.setPixelColor(35, getColor("분_접미사", dt));
  } else if (h > 12) {  // 오후
    pixels.setPixelColor(0, getColor("오후", dt));
    pixels.setPixelColor(6, getColor("오후", dt));
    pixels.setPixelColor(35, getColor("분_접미사", dt));
    h = h - 12;         // 시간: 오전, 오후 12시간제로 변환
  } else if (h == 0) {  // 자정
    pixels.setPixelColor(18, getColor("자정", dt));
    pixels.setPixelColor(24, getColor("자정", dt));
  } else if (h == 12) {  // 정오
    pixels.setPixelColor(24, getColor("정오", dt));
    pixels.setPixelColor(25, getColor("정오", dt));
  } else {
    pixels.clear();
  }

  switch (h) {  // 시간: 1부터 12까지
    case 1:
      pixels.setPixelColor(3, getColor("시_시각", dt));
      pixels.setPixelColor(17, getColor("시_접미사", dt));  //~시
      break;
    case 2:
      pixels.setPixelColor(4, getColor("시_시각", dt));
      pixels.setPixelColor(17, getColor("시_접미사", dt));  //~시
      break;
    case 3:
      pixels.setPixelColor(5, getColor("시_시각", dt));
      pixels.setPixelColor(17, getColor("시_접미사", dt));  //~시
      break;
    case 4:
      pixels.setPixelColor(11, getColor("시_시각", dt));
      pixels.setPixelColor(17, getColor("시_접미사", dt));  //~시
      break;
    case 5:
      pixels.setPixelColor(12, getColor("시_시각", dt));
      pixels.setPixelColor(13, getColor("시_시각", dt));
      pixels.setPixelColor(17, getColor("시_접미사", dt));  //~시
      break;
    case 6:
      pixels.setPixelColor(7, getColor("시_시각", dt));
      pixels.setPixelColor(13, getColor("시_시각", dt));
      pixels.setPixelColor(17, getColor("시_접미사", dt));  //~시
      break;
    case 7:
      pixels.setPixelColor(8, getColor("시_시각", dt));
      pixels.setPixelColor(14, getColor("시_시각", dt));
      pixels.setPixelColor(17, getColor("시_접미사", dt));  //~시
      break;
    case 8:
      pixels.setPixelColor(9, getColor("시_시각", dt));
      pixels.setPixelColor(10, getColor("시_시각", dt));
      pixels.setPixelColor(17, getColor("시_접미사", dt));  //~시
      break;
    case 9:
      pixels.setPixelColor(15, getColor("시_시각", dt));
      pixels.setPixelColor(16, getColor("시_시각", dt));
      pixels.setPixelColor(17, getColor("시_접미사", dt));  //~시
      break;
    case 10:
      pixels.setPixelColor(2, getColor("시_시각", dt));
      pixels.setPixelColor(17, getColor("시_접미사", dt));  //~시
      break;
    case 11:
      pixels.setPixelColor(2, getColor("시_시각", dt));
      pixels.setPixelColor(3, getColor("시_시각", dt));
      pixels.setPixelColor(17, getColor("시_접미사", dt));  //~시
      break;
    case 12:
      pixels.setPixelColor(2, getColor("시_시각", dt));
      pixels.setPixelColor(4, getColor("시_시각", dt));
      pixels.setPixelColor(17, getColor("시_접미사", dt));  //~시
      break;
    default:
      break;
  }


  int m_tens = m / 10;  // 분: 십의 자리 1부터 5까지
  switch (m_tens) {
    case 1:
      pixels.setPixelColor(23, getColor("분_시각", dt));
      break;
    case 2:
      pixels.setPixelColor(19, getColor("분_시각", dt));
      pixels.setPixelColor(23, getColor("분_시각", dt));
      break;
    case 3:
      pixels.setPixelColor(20, getColor("분_시각", dt));
      pixels.setPixelColor(23, getColor("분_시각", dt));
      break;
    case 4:
      pixels.setPixelColor(21, getColor("분_시각", dt));
      pixels.setPixelColor(23, getColor("분_시각", dt));
      break;
    case 5:
      pixels.setPixelColor(22, getColor("분_시각", dt));
      pixels.setPixelColor(23, getColor("분_시각", dt));
      break;
    default:
      pixels.setPixelColor(23, led_off);  //~십 아닐때
      break;
  }

  int m_ones = m % 10;  // 분: 일의 자리 1부터 9까지
  switch (m_ones) {
    case 1:
      pixels.setPixelColor(26, getColor("분_시각", dt));
      pixels.setPixelColor(35, getColor("분_접미사", dt));
      break;
    case 2:
      pixels.setPixelColor(27, getColor("분_시각", dt));
      pixels.setPixelColor(35, getColor("분_접미사", dt));
      break;
    case 3:
      pixels.setPixelColor(28, getColor("분_시각", dt));
      pixels.setPixelColor(35, getColor("분_접미사", dt));
      break;
    case 4:
      pixels.setPixelColor(29, getColor("분_시각", dt));
      pixels.setPixelColor(35, getColor("분_접미사", dt));
      break;
    case 5:
      pixels.setPixelColor(30, getColor("분_시각", dt));
      pixels.setPixelColor(35, getColor("분_접미사", dt));
      break;
    case 6:
      pixels.setPixelColor(31, getColor("분_시각", dt));
      pixels.setPixelColor(35, getColor("분_접미사", dt));
      break;
    case 7:
      pixels.setPixelColor(32, getColor("분_시각", dt));
      pixels.setPixelColor(35, getColor("분_접미사", dt));
      break;
    case 8:
      pixels.setPixelColor(33, getColor("분_시각", dt));
      pixels.setPixelColor(35, getColor("분_접미사", dt));
      break;
    case 9:
      pixels.setPixelColor(34, getColor("분_시각", dt));
      pixels.setPixelColor(35, getColor("분_접미사", dt));
      break;
    default:
      pixels.setPixelColor(35, getColor("분_접미사", dt));  //~분
      break;
  }

  if (m_tens == 0 && m_ones == 0) {  // 정시
    pixels.setPixelColor(35, led_off);
  }

  pixels.show();
}


unsigned long processing_offset = 0;
void setup() {
  Serial.begin(4800);
  Serial.println("Clock Startup...");
  rtc.begin();
  pixels.begin();
  pixels.show();  // 모든 LED를 꺼서 초기화


  colorCustomPreset.reset_colorPresetArr();  //프리셋 리셋, 삭제필요
  colorTimePreset.reset_colorPresetArr();    //프리셋 리셋, 삭제필요

  flowWatchFace();

  print_freeMemory();
  Serial.println("Clock Booted!!!");
}
void loop() {
  delay(1);
  if (Serial.available() > 0) {
    ProcessingSerial();
  }

  DateTime now = rtc.now();



  if (showingMinute != now.minute() || showingHour != now.hour()) {
    unsigned long start_processing_offset = millis();

    if (showingHour != now.hour() && hourlyNotificationSet == true) {
      blinkWatchFace(now);
    }

    showingHour = now.hour();
    showingMinute = now.minute();
    refreshWatchFace(now);
    processing_offset = millis() - start_processing_offset;

    Serial.println("");
    printDateTime(now);
    print_freeMemory();
    Serial.print("processing_offset: ");
    Serial.println(processing_offset);
  }
}
