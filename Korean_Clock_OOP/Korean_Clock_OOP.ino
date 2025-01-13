// @library Adafruit NeoPixel
// @library RTClib
// @library ArduinoJson


#include <RTClib.h>
#include <Adafruit_NeoPixel.h>
#include <Time.h>
#include <ArduinoJson.h>
#include <EEPROM.h>


#define PIN 6         // LED 스트립이 연결된 핀 번호
#define NUMPIXELS 36  // LED의 개수

RTC_DS3231 rtc;
Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);


#define MAX_CUSTOM_PRESETS 2
#define MAX_TIME_PRESETS 2


int showingMinute = 0;
int showingHour = 0;
bool hourlyNotificationSet = true;  //정시알림, 수정필요
int blinkTime = 500;

struct RGBstruct {
  uint8_t r, g, b;
};

struct ColorPresetStruct {
  uint8_t priority;           // 우선순위
  uint8_t ledID_bitmask;      // 비트마스크, 색상을 적용할 위치, 오전,오후,자정,정오,시_시각,시_접미사,분_시각,분_접미사 시간프리셋에서는 0
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

void print_freeMemory(const String function) {
  Serial.print(F("Free memory "));
  Serial.print(function);
  Serial.print(F(": "));
  Serial.println(freeMemory());
}
void print_freeMemory() {
  Serial.print(F("Free memory: "));
  Serial.println(freeMemory());
}


uint8_t get_IndexTime(DateTime dt) {
  // 총 몇 번째 10분 단위인지 계산
  uint8_t IndexTime = (dt.hour() * 60 + dt.minute()) / 10;
  return IndexTime;
}

class EEPROM_CURD {
  //빈공간: 0x00
  //헤드 플래그: 0xAA 0xAA
  //예비공간: 0xFF
private:
  int headAddress = 0;
  struct Header_EEPROM {
    int head_ColorCustomPreset;
    int count_ColorCustomPreset;
    int max_ColorCustomPreset;
    int head_ColorTimePreset;
    int count_ColorTimePreset;
    int max_ColorTimePreset;
    int tailAddress;

    RGBstruct defaultColor;
    } header;

  //Header_EEPROM header;
  void setZero(int address) {
    EEPROM.update(address, 0x00);
  }
  void setFlag(int address) {
    EEPROM.update(address, 0xAA);
    EEPROM.update(address + 1, 0xAA);
  }
  void setBlank(int address) {
    EEPROM.update(address, 0xFF);
  }

  void findHead() {
    for (int i = 0; i < EEPROM.length(); i++) {
      if (EEPROM.read(i) == 0xAA && EEPROM.read(i + 1) == 0xAA) {
        headAddress = i + 2;
        return;
      }
    }
    //헤더를 찾지 못했을 때
    headAddress = 0;
    headerInitialization();
    return;
  }
  void zeroFill() {
    for (int i = 0; i < EEPROM.length(); i++) {
      setZero(i);
    }
  }
  void zeroFill(int start, int end) {
    if (start < 2) {
      zeroFill(0, end);
    } else {
      for (int i = start - 2; i < end; i++) {
        setZero(i);
      }
    }
  }
  void headerInitialization() {
    header.head_ColorCustomPreset = 0;
    header.count_ColorCustomPreset = 0;
    header.max_ColorCustomPreset = 0;
    header.head_ColorTimePreset = 0;
    header.count_ColorTimePreset = 0;
    header.max_ColorTimePreset = 0;
    header.tailAddress = 0;
    header.defaultColor={255,255,255};
  }







public:
  EEPROM_CURD() {
    findHead();
    EEPROM.get(headAddress, header);
  }
  ~EEPROM_CURD() {
    updateHeader();
    //TODO: 빈 공간 및 예비공간 채우기
  }
  void newWrite(int max_ColorCustomPreset, int max_ColorTimePreset) {
    zeroFill(headAddress, header.tailAddress);
    headAddress = header.tailAddress;

    headerInitialization();
    header.head_ColorCustomPreset = headAddress + sizeof(header);
    header.max_ColorCustomPreset = max_ColorCustomPreset;
    header.head_ColorTimePreset = header.head_ColorCustomPreset + sizeof(ColorPresetStruct) * max_ColorCustomPreset;
    header.max_ColorTimePreset = max_ColorTimePreset;
    header.tailAddress = header.head_ColorTimePreset + sizeof(ColorPresetStruct) * max_ColorTimePreset;
  }
  Header_EEPROM getHeader() {
    EEPROM.get(headAddress, header);
    return header;
  }
  void updateHeader() {  //헤더 쓰기
    EEPROM.put(headAddress, header);
  }
  bool putStruct(ColorPresetStruct data, int type) {
    int address = 0;
    if (type == 0) {
      address = header.head_ColorCustomPreset + sizeof(ColorPresetStruct) * header.count_ColorCustomPreset;
      EEPROM.put(address, data);
      header.count_ColorCustomPreset += 1;
    } else if (type == 1) {
      address = header.head_ColorTimePreset + sizeof(ColorPresetStruct) * header.count_ColorTimePreset;
      EEPROM.put(address, data);
      header.count_ColorTimePreset += 1;
    } else {
      return false;
    }
    return true;
  }
  ColorPresetStruct getStruct(int index, int type) {
    int address = 0;
    ColorPresetStruct data = ColorPresetStruct{};
    if (type == 0) {
      address = header.head_ColorCustomPreset + sizeof(ColorPresetStruct) * index;
    } else if (type == 1) {
      address = header.head_ColorTimePreset + sizeof(ColorPresetStruct) * index;
    } else {
      return ColorPresetStruct{};
    }

    EEPROM.get(address, data);
    return data;
  }
  void flush() {
    zeroFill();
  }
};
EEPROM_CURD eeprom_curd();

class ColorPreset {
private:
  ColorPresetStruct* colorPresetArr;  // 동적 배열 포인터
protected:
  void makeColorPresetArr(const int size) {
    maxSize = size;
    colorPresetArr = new ColorPresetStruct[maxSize];  // 동적 배열 할당
  }
  void purgeColorPresetArr() {
    delete[] colorPresetArr;  // 메모리 해제
  }

  int colorPresetSize = 0;
  int maxSize = 0;

public:


  int get_colorPresetSize() const {
    return colorPresetSize;
  }
  int get_maxSize() const {
    return maxSize;
  }

  String* get_LedID_arr(const ColorPresetStruct& preset) {
    static String led_id_arr[8];
    int index = 0;

    // 각 비트에 해당하는 ID를 배열에 추가
    if (preset.ledID_bitmask & 0b00000001) led_id_arr[index++] = F("오전");
    if (preset.ledID_bitmask & 0b00000010) led_id_arr[index++] = F("오후");
    if (preset.ledID_bitmask & 0b00000100) led_id_arr[index++] = F("자정");
    if (preset.ledID_bitmask & 0b00001000) led_id_arr[index++] = F("정오");
    if (preset.ledID_bitmask & 0b00010000) led_id_arr[index++] = F("시_시각");
    if (preset.ledID_bitmask & 0b00100000) led_id_arr[index++] = F("시_접미사");
    if (preset.ledID_bitmask & 0b01000000) led_id_arr[index++] = F("분_시각");
    if (preset.ledID_bitmask & 0b10000000) led_id_arr[index++] = F("분_접미사");

    // 나머지 요소는 빈 문자열로 초기화
    for (int i = index; i < 8; i++) {
      led_id_arr[i] = "";
    }

    return led_id_arr;
  }

  bool isContains_ledID(const ColorPresetStruct& preset, const String target) {
    String* led_id_arr = get_LedID_arr(preset);

    for (int i = 0; i < 8; i++) {
      if (led_id_arr[i] == target) {
        return true;  // 배열에 해당 문자열이 있음
      }
    }
    return false;  // 배열에 해당 문자열이 없음
  }

  bool isContain_dayOfWeek(const ColorPresetStruct& preset, const int target) {
    int* dayOfWeek_arr = get_dayOfWeek_arr(preset);

    for (int i = 0; i < 8; i++) {
      if (dayOfWeek_arr[i] == -1) break;            // 유효한 값이 없으므로 반복문 종료
      if (dayOfWeek_arr[i] == target) return true;  // 배열에 해당 값이 있음
    }
    return false;  // 배열에 해당 값이 없음
  }

  int* get_dayOfWeek_arr(const ColorPresetStruct& preset) {
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
  bool addColorPreset(const uint8_t priority, const uint8_t ledID_bitmask, const uint8_t startIndexTime, const uint8_t endIndexTime, const int dayOfWeek_bitmask, const RGBstruct ledColor) {
    if (colorPresetSize >= maxSize) {
      Serial.println(F("addColorPreset_memoryFULL"));
      return false;  // 배열이 가득 찼습니다.
    }

    // 새로운 항목 추가
    colorPresetArr[colorPresetSize] = { priority, ledID_bitmask, startIndexTime, endIndexTime, dayOfWeek_bitmask, ledColor };
    colorPresetSize++;
    delay(100);
    Serial.println(F("addColorPreset DONE"));
    return true;
  }
  /*
  // LED ID 배열을 받아 비트마스크로 변환 후 호출하는 함수
  bool addColorPreset(const uint8_t priority, const String led_select_id_arr[], const uint8_t startIndexTime, const uint8_t endIndexTime, const uint8_t dayOfWeek_bitmask, const RGBstruct ledColor) {
    uint8_t ledID_bitmask = 0;
    for (int i = 0; i < (sizeof(led_select_id_arr) / sizeof(led_select_id_arr[0])); i++) {
      if (led_select_id_arr[i] == F("오전")) ledID_bitmask |= 0b00000001;
      if (led_select_id_arr[i] == F("오후")) ledID_bitmask |= 0b00000010;
      if (led_select_id_arr[i] == F("자정")) ledID_bitmask |= 0b00000100;
      if (led_select_id_arr[i] == F("정오")) ledID_bitmask |= 0b00001000;
      if (led_select_id_arr[i] == F("시_시각")) ledID_bitmask |= 0b00010000;
      if (led_select_id_arr[i] == F("시_접미사")) ledID_bitmask |= 0b00100000;
      if (led_select_id_arr[i] == F("분_시각")) ledID_bitmask |= 0b01000000;
      if (led_select_id_arr[i] == F("분_접미사")) ledID_bitmask |= 0b10000000;
    }
    return addColorPreset(priority, ledID_bitmask, startIndexTime, endIndexTime, dayOfWeek_bitmask, ledColor);
  }
  // dayOfWeek 배열을 받아 비트마스크로 변환 후 호출하는 함수
  bool addColorPreset(const uint8_t priority, const String led_select_id_arr[], const uint8_t startIndexTime, const uint8_t endIndexTime, const int dayOfWeek_arr[], const RGBstruct ledColor) {
    uint8_t dayOfWeek_bitmask = 0;
    for (int i = 0; dayOfWeek_arr[i] != -1; i++) {
      if (dayOfWeek_arr[i] >= 0 && dayOfWeek_arr[i] <= 6) {
        dayOfWeek_bitmask |= (1 << dayOfWeek_arr[i]);
      }
    }
    return addColorPreset(priority, led_select_id_arr, startIndexTime, endIndexTime, dayOfWeek_bitmask, ledColor);
  }
  // LED ID 없이 dayOfWeek 배열을 비트마스크로 변환 후 호출하는 함수
  bool addColorPreset(const uint8_t priority, const uint8_t startIndexTime, const uint8_t endIndexTime, const int dayOfWeek_arr[], const RGBstruct ledColor) {
    return addColorPreset(priority, (uint8_t)0, startIndexTime, endIndexTime, dayOfWeek_arr, ledColor);
  }*/
  // LED ID 없이 dayOfWeek 비트마스크만 받아 호출하는 함수
  bool addColorPreset(const uint8_t priority, const uint8_t startIndexTime, const uint8_t endIndexTime, const uint8_t dayOfWeek_bitmask, const RGBstruct ledColor) {
    return addColorPreset(priority, (uint8_t)0, startIndexTime, endIndexTime, dayOfWeek_bitmask, ledColor);
  }


  //배열을 구함
  // 배열의 포인터를 반환하는 함수
  ColorPresetStruct* getColorPresetArr() {
    return colorPresetArr;  // 배열의 첫 번째 요소에 대한 포인터 반환
  }
  // 특정 인덱스의 요소를 복사해서 리턴하는 함수
  ColorPresetStruct getColorPresetArr(const int index) {
    if (index >= 0 && index < colorPresetSize) {
      return colorPresetArr[index];  // 인덱스의 요소를 복사하여 반환
    } else {
      // 잘못된 인덱스의 경우 빈 객체를 반환하거나 오류를 처리해야 함
      return ColorPresetStruct{};  // 기본 생성자로 초기화된 구조체 반환
    }
  }

  void printColorPresets() const {
    Serial.println(F("----- Color Preset Array -----"));

    for (int i = 0; i < colorPresetSize; i++) {
      const ColorPresetStruct& preset = colorPresetArr[i];  // 현재 구조체

      // 한 줄로 모든 정보를 출력
      Serial.print(F("Index: "));
      Serial.print(i);
      Serial.print(F(", Priority: "));
      Serial.print(preset.priority);
      Serial.print(F(", LED ID: "));
      Serial.print(preset.ledID_bitmask, BIN);  // 이진수 형식
      Serial.print(F(", StartTime: "));
      Serial.print(preset.startIndexTime);
      Serial.print(F(", EndTime: "));
      Serial.print(preset.endIndexTime);
      Serial.print(F(", Days: "));
      Serial.print(preset.dayOfWeek_bitmask, BIN);  // 요일 비트마스크
      Serial.print(F(", Color: ("));
      Serial.print(preset.ledColor.r);
      Serial.print(F(", "));
      Serial.print(preset.ledColor.g);
      Serial.print(F(", "));
      Serial.print(preset.ledColor.b);
      Serial.println(F(")"));
    }

    if (colorPresetSize == 0) {
      Serial.println(F("No presets available."));
    }
  }



  //배열을 비움
  void clearColorPreset(const int newSize) {
    purgeColorPresetArr();
    makeColorPresetArr(newSize);
    Serial.println(F("clearColorPreset DONE"));
  }
};

class ColorCustomPreset : public ColorPreset {
public:
  ColorCustomPreset(const int maxSize) {
    makeColorPresetArr(maxSize);  // 배열 크기 설정
  }
  ~ColorCustomPreset() {
    purgeColorPresetArr();
  }

  void reset_colorPresetArr() {
    clearColorPreset(MAX_CUSTOM_PRESETS);

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
  }
};

class ColorTimePreset : public ColorPreset {
public:
  ColorTimePreset(const int maxSize) {
    makeColorPresetArr(maxSize);  // 배열 크기 설정
  }
  ~ColorTimePreset() {
    purgeColorPresetArr();
  }

  void reset_colorPresetArr() {
    clearColorPreset(MAX_TIME_PRESETS);

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
  uint8_t priority = 0xFF;  //최댓값
  RGBstruct RGBcolor = { 255, 255, 255 };

  // colorCustomPresetArr 배열을 순회하며 조건에 맞는 항목을 찾음
  for (int i = 0; i < colorTimePreset.get_colorPresetSize(); i++) {
    ColorPresetStruct preset = colorTimePreset.getColorPresetArr(i);

    // 현재 요일이 적용 가능한지 확인
    if (colorTimePreset.isContain_dayOfWeek(preset, currentDayOfWeek)) {
      //Serial.println("Day of week matched.");

      // 시간 범위가 같은 날 내에 있는 경우
      if (preset.startIndexTime <= preset.endIndexTime) {
        if (currentIndexTime >= preset.startIndexTime && currentIndexTime < preset.endIndexTime) {
          //Serial.println("Time matched.");

          //return preset.ledColor; // 조건을 만족하면 즉시 ledColor 반환
          if (priority >= preset.priority) {
            priority = preset.priority;
            RGBcolor = preset.ledColor;
          }
        }
      }
      // 시간 범위가 다음날로 넘어가는 경우 (예: 20시 ~ 익일 07시)
      else {
        if (currentIndexTime >= preset.startIndexTime || currentIndexTime < preset.endIndexTime) {
          //Serial.println("Time matched (next day).");

          //return preset.ledColor; // 조건을 만족하면 즉시 ledColor 반환
          if (priority >= preset.priority) {
            priority = preset.priority;
            RGBcolor = preset.ledColor;
          }
        }
      }
    }
  }
  if (priority < 0xFF) {
    return pixels.Color(RGBcolor.r, RGBcolor.g, RGBcolor.b);
  } else {
    return getColor();
  }
}
uint32_t getColor(const String led_select_id, const DateTime now_dt) {
  uint8_t currentIndexTime = (now_dt.hour() * 60 + now_dt.minute()) / 10;
  int currentDayOfWeek = now_dt.dayOfTheWeek();
  uint8_t priority = 0xFF;                 //최댓값
  RGBstruct RGBcolor = { 255, 255, 255 };  // 시간 단독 대응 기본 색상 반환


  // colorCustomPresetArr 배열을 순회하며 조건에 맞는 항목을 찾음
  for (int i = 0; i < colorCustomPreset.get_colorPresetSize(); i++) {
    ColorPresetStruct preset = colorCustomPreset.getColorPresetArr(i);

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

            if (priority >= preset.priority) {
              priority = preset.priority;
              RGBcolor = preset.ledColor;
            }
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

            if (priority >= preset.priority) {
              priority = preset.priority;
              RGBcolor = preset.ledColor;
            }
          }
        }
      }
    }
  }
  if (priority < 0xFF) {
    return pixels.Color(RGBcolor.r, RGBcolor.g, RGBcolor.b);
  } else {
    return getColor(now_dt);
  }
}




void flowWatchFace(const DateTime now_dt) {
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

void blinkWatchFace(const DateTime now_dt) {
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

void changeSpeedSerial(long speed) {
  // 새로운 속도 설정
  Serial.print(F("newSpeed: "));
  Serial.println(speed);
  Serial.end();
  delay(100);
  Serial.begin(speed);
  delay(100);
}

void lowSpeedSerial() {
  StaticJsonDocument<64> doc;
  doc["speed"] = 4800;  // 4800 설정
  changeSpeedSerial(doc.as<JsonObject>());
}


StaticJsonDocument<1000> doc;
const char function_str[] PROGMEM = "function";
void jsonSerialProcesser(const String data) {
  //**JSON**
  /*
  {
    "function": "preset_edit",  // "preset_edit"...
    "presetType": "custom",  // "custom" 또는 "time" 중 하나의 값 사용
    "presetCurd": "create"   // "add", "reset", "read", "delete"
    "DATA": [priority,ledID_bitmask,startIndexTime,endIndexTime,dayOfWeek_bitmask]
    "RGB": [255,255,255]
  }
  */
  //data="{\"function\":\"preset_edit\",\"presetType\":\"custom\",\"presetCurd\":\"create\",\"presetData\":[{\"priority\":1,\"ledID_bitmask\":129,\"startIndexTime\":6,\"endIndexTime\":18,\"dayOfWeek_bitmask\":63,\"ledColor\":{\"r\":255,\"g\":140,\"b\":100}},{\"priority\":2,\"ledID_arr\":[\"오전\",\"오후\",\"자정\",\"정오\",\"시_시각\",\"시_접미사\",\"분_시각\",\"분_접미사\"],\"startIndexTime\":6,\"endIndexTime\":18,\"dayOfWeek_arr\":[0,1,3,5,6,-1,-1,-1],\"ledColor\":{\"r\":255,\"g\":140,\"b\":100}}]}";

  print_freeMemory(F("before json"));
  Serial.println("data: " + data);

  // JSON 데이터 파싱
  DeserializationError error = deserializeJson(doc, data);
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return;
  }

  // JsonObject 생성 및 유효성 검사
  JsonObject obj = doc.as<JsonObject>();
  if (!obj.containsKey(F("function"))) {
    Serial.println(F("Error: 'function' key is missing!"));
    return;
  }

//메모리 절약 필요
  if (obj[F("function")] == F("adjust_time")) {  //시간 조정
    adjustByJson(obj);
  } else if (obj[F("function")] == F("changeSpeedSerial")) {  //시리얼 속도변경
    changeSpeedSerial(obj["speed"].as<long>());
  } else if (obj[F("function")] == F("preset_edit")) {  //프리셋 조정
    jsonPresetEdit(obj);
  } else {
    Serial.println(F("Error: Unknown function in JSON!"));
  }
  print_freeMemory(F("after json"));
}


// PROGMEM에 문자열 저장
const char presetCurd_add[] PROGMEM = "add";
const char presetCurd_reset[] PROGMEM = "reset";
const char presetCurd_read[] PROGMEM = "read";
const char presetCurd_clear[] PROGMEM = "clear";

const char presetType_custom[] PROGMEM = "custom";
const char presetType_time[] PROGMEM = "time";

// 문자열 비교 함수 (RAM 절약)
bool compare_P(const char* jsonValue, const char* progmemValue) {
  return strcmp_P(jsonValue, progmemValue) == 0;
}

// JSON 처리 함수
void jsonPresetEdit(const JsonObject& obj) {
  uint8_t DATA[5];
  uint8_t RGB[3];

  JsonArray dataArray = obj["Preset_DATA"].as<JsonArray>();
  for (size_t i = 0; i < 5; i++) {
    DATA[i] = dataArray[i];
  }


  JsonArray rgbArray = obj["Preset_RGB"].as<JsonArray>();
  for (size_t i = 0; i < 3; i++) {
    if (i < rgbArray.size()) {
      RGB[i] = rgbArray[i];
    } else {
      RGB[i] = 255;  // 기본값 (null 대신 0으로 초기화)
    }
  }

  const char* presetType = obj["presetType"];
  const char* presetCurd = obj["presetCurd"];
  // 프리셋 타입 비교
  if (compare_P(presetType, presetType_custom)) {
    // 프리셋 CURD 작업 비교
    if (compare_P(presetCurd, presetCurd_add)) {  // 프리셋 추가
      colorCustomPreset.addColorPreset(DATA[0], DATA[1], DATA[2], DATA[3], DATA[4], { RGB[0], RGB[1], RGB[2] });
    } else if (compare_P(presetCurd, presetCurd_reset)) {  // 모든 프리셋 삭제 후 등록
      colorCustomPreset.reset_colorPresetArr();
    } else if (compare_P(presetCurd, presetCurd_read)) {  // 프리셋 읽기
      colorCustomPreset.printColorPresets();
    } else if (compare_P(presetCurd, presetCurd_clear)) {  // 모든 프리셋 삭제
      colorCustomPreset.clearColorPreset(MAX_CUSTOM_PRESETS);
    } else {
      Serial.println(F("Unknown presetCurd command"));
    }
  } else if (compare_P(presetType, presetType_time)) {
    if (compare_P(presetCurd, presetCurd_add)) {                                                       // 시간 프리셋 추가
      colorTimePreset.addColorPreset(DATA[0], DATA[2], DATA[3], DATA[4], { RGB[0], RGB[1], RGB[2] });  //LED_ID 불필요에 따른 DATA[1] 제거
    } else if (compare_P(presetCurd, presetCurd_reset)) {                                              // 모든 시간 프리셋 삭제 후 등록
      colorTimePreset.reset_colorPresetArr();
    } else if (compare_P(presetCurd, presetCurd_read)) {  // 시간 프리셋 읽기
      colorTimePreset.printColorPresets();
    } else if (compare_P(presetCurd, presetCurd_clear)) {  // 모든 시간 프리셋 삭제
      colorTimePreset.clearColorPreset(MAX_TIME_PRESETS);
    } else {
      Serial.println(F("Unknown presetCurd command"));
    }
  } else {
    Serial.println(F("Unknown presetType"));
  }
}

void printDateTime(const DateTime dt) {
  // 요일 문자열 배열
  const char day0[] PROGMEM = "일요일";
  const char day1[] PROGMEM = "월요일";
  const char day2[] PROGMEM = "화요일";
  const char day3[] PROGMEM = "수요일";
  const char day4[] PROGMEM = "목요일";
  const char day5[] PROGMEM = "금요일";
  const char day6[] PROGMEM = "토요일";
  const char* const daysOfWeek[] PROGMEM = { day0, day1, day2, day3, day4, day5, day6 };
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

void printStringArray(const String arr[], const int size) {
  for (int i = 0; i < size; i++) {
    Serial.print(arr[i]);
    if (i < size - 1) {
      Serial.print(", ");  // 각 요소 사이에 쉼표 추가
    }
  }
  Serial.println();  // 배열 출력 후 줄 바꿈
}
void printIntArray(const int arr[], const int size) {
  for (int i = 0; i < size; i++) {
    Serial.print(arr[i]);
    if (i < size - 1) {
      Serial.print(", ");  // 각 요소 사이에 쉼표 추가
    }
  }
  Serial.println();  // 배열 출력 후 줄 바꿈
}



void adjustByJson(const JsonObject obj) {
  Serial.println(F("proceeding with Client."));
  /*
  Serial.println(F("Debugging JSON Object..."));
  serializeJson(obj, Serial);  // JsonObject 내용을 직렬화해서 출력
  Serial.println();
*/

  // "datetime" 값 추출
  const String& str = obj["datetime"].as<String>();
  /*if (str.length() == 0) {
    Serial.println(F("Error: 'datetime' value is empty!"));
    return;
  }*/

  Serial.print(F("Extracted datetime: "));
  Serial.println(str);

  print_freeMemory(F("before parsing json"));

  int newYear, newMonth, newDay, newHour, newMinute, newSecond;

  // "datetime" 값 파싱
  if (sscanf(str.c_str(), "%d-%d-%d %d:%d:%d", &newYear, &newMonth, &newDay, &newHour, &newMinute, &newSecond) == 6) {
    DateTime adjustedTime(newYear, newMonth, newDay, newHour, newMinute, newSecond);
    rtc.adjust(adjustedTime);

    Serial.print(F("Adjusted by Client: "));
    printDateTime(adjustedTime);  // 수정된 시간 출력
  } else {
    Serial.println(F("Data parsing failed!"));
  }
}

void adjust_by_UTCK(const String& rowStrData, const int& atIndex) {
  Serial.println(F("First character is '@', proceeding with UTCK."));
  if (rowStrData.length() >= 8) {
    const int newHour = rowStrData.substring(atIndex + 1, atIndex + 3).toInt();    // 시간
    const int newMinute = rowStrData.substring(atIndex + 3, atIndex + 5).toInt();  // 분
    const int newSecond = rowStrData.substring(atIndex + 5, atIndex + 7).toInt();  // 초

    // 현재 날짜를 유지하고 시간만 변경
    DateTime now = rtc.now();
    DateTime adjustedTime(now.year(), now.month(), now.day(), newHour, newMinute, newSecond);
    rtc.adjust(adjustedTime);

    Serial.print(F("Adjusted by UTCK3: "));
    printDateTime(adjustedTime);  // 수정된 시간 출력
  } else {
    Serial.println(F("Error: Input data is too short."));
  }
}

void processingSerial() {
  if (Serial.available() > 0) {
    const String rowStrData = Serial.readStringUntil('\n');
    //Serial.println(F("Received Serial Data: ") + rowStrData);

    // UTCK3 연동, 첫 문자가 '@'인지 확인
    int atIndexOf_UTCK3 = rowStrData.indexOf('@');
    int atIndexOf_JSON = rowStrData.indexOf('{');
    if (atIndexOf_UTCK3 != -1) {
      adjust_by_UTCK(rowStrData, atIndexOf_UTCK3);
      //blinkWatchFace(rtc.now());

    } else if (atIndexOf_JSON != -1) {
      jsonSerialProcesser(rowStrData);


    } else {
      //Serial.println(F("Error: First character is unknown."));
      lowSpeedSerial();
    }
  }
}

const char amStr[] PROGMEM = "오전";                 // AM
const char pmStr[] PROGMEM = "오후";                 // PM
const char midnightStr[] PROGMEM = "자정";           // Midnight
const char noonStr[] PROGMEM = "정오";               // Noon
const char hourTimeStr[] PROGMEM = "시_시각";        // Hour Time
const char minuteTimeStr[] PROGMEM = "분_시각";      // Minute Time
const char hourSuffixStr[] PROGMEM = "시_접미사";    // Hour Suffix
const char minuteSuffixStr[] PROGMEM = "분_접미사";  // Minute Suffix


void refreshWatchFace(const DateTime dt) {
  pixels.clear();
  int h = dt.hour();
  int m = dt.minute();

  if (h > 0 && h < 12) {  // 오전
    pixels.setPixelColor(0, getColor(amStr, dt));
    pixels.setPixelColor(1, getColor(amStr, dt));
    pixels.setPixelColor(35, getColor(minuteSuffixStr, dt));
  } else if (h > 12) {  // 오후
    pixels.setPixelColor(0, getColor(pmStr, dt));
    pixels.setPixelColor(6, getColor(pmStr, dt));
    pixels.setPixelColor(35, getColor(minuteSuffixStr, dt));
    h = h - 12;         // 시간: 오전, 오후 12시간제로 변환
  } else if (h == 0) {  // 자정
    pixels.setPixelColor(18, getColor(midnightStr, dt));
    pixels.setPixelColor(24, getColor(midnightStr, dt));
  } else if (h == 12) {  // 정오
    pixels.setPixelColor(24, getColor(noonStr, dt));
    pixels.setPixelColor(25, getColor(noonStr, dt));
  } else {
    pixels.clear();
  }

  switch (h) {  // 시간: 1부터 12까지
    case 1:
      pixels.setPixelColor(3, getColor(hourTimeStr, dt));
      pixels.setPixelColor(17, getColor(hourSuffixStr, dt));  //~시
      break;
    case 2:
      pixels.setPixelColor(4, getColor(hourTimeStr, dt));
      pixels.setPixelColor(17, getColor(hourSuffixStr, dt));  //~시
      break;
    case 3:
      pixels.setPixelColor(5, getColor(hourTimeStr, dt));
      pixels.setPixelColor(17, getColor(hourSuffixStr, dt));  //~시
      break;
    case 4:
      pixels.setPixelColor(11, getColor(hourTimeStr, dt));
      pixels.setPixelColor(17, getColor(hourSuffixStr, dt));  //~시
      break;
    case 5:
      pixels.setPixelColor(12, getColor(hourTimeStr, dt));
      pixels.setPixelColor(13, getColor(hourTimeStr, dt));
      pixels.setPixelColor(17, getColor(hourSuffixStr, dt));  //~시
      break;
    case 6:
      pixels.setPixelColor(7, getColor(hourTimeStr, dt));
      pixels.setPixelColor(13, getColor(hourTimeStr, dt));
      pixels.setPixelColor(17, getColor(hourSuffixStr, dt));  //~시
      break;
    case 7:
      pixels.setPixelColor(8, getColor(hourTimeStr, dt));
      pixels.setPixelColor(14, getColor(hourTimeStr, dt));
      pixels.setPixelColor(17, getColor(hourSuffixStr, dt));  //~시
      break;
    case 8:
      pixels.setPixelColor(9, getColor(hourTimeStr, dt));
      pixels.setPixelColor(10, getColor(hourTimeStr, dt));
      pixels.setPixelColor(17, getColor(hourSuffixStr, dt));  //~시
      break;
    case 9:
      pixels.setPixelColor(15, getColor(hourTimeStr, dt));
      pixels.setPixelColor(16, getColor(hourTimeStr, dt));
      pixels.setPixelColor(17, getColor(hourSuffixStr, dt));  //~시
      break;
    case 10:
      pixels.setPixelColor(2, getColor(hourTimeStr, dt));
      pixels.setPixelColor(17, getColor(hourSuffixStr, dt));  //~시
      break;
    case 11:
      pixels.setPixelColor(2, getColor(hourTimeStr, dt));
      pixels.setPixelColor(3, getColor(hourTimeStr, dt));
      pixels.setPixelColor(17, getColor(hourSuffixStr, dt));  //~시
      break;
    case 12:
      pixels.setPixelColor(2, getColor(hourTimeStr, dt));
      pixels.setPixelColor(4, getColor(hourTimeStr, dt));
      pixels.setPixelColor(17, getColor(hourSuffixStr, dt));  //~시
      break;
    default:
      break;
  }


  int m_tens = m / 10;  // 분: 십의 자리 1부터 5까지
  switch (m_tens) {
    case 1:
      pixels.setPixelColor(23, getColor(minuteTimeStr, dt));
      break;
    case 2:
      pixels.setPixelColor(19, getColor(minuteTimeStr, dt));
      pixels.setPixelColor(23, getColor(minuteTimeStr, dt));
      break;
    case 3:
      pixels.setPixelColor(20, getColor(minuteTimeStr, dt));
      pixels.setPixelColor(23, getColor(minuteTimeStr, dt));
      break;
    case 4:
      pixels.setPixelColor(21, getColor(minuteTimeStr, dt));
      pixels.setPixelColor(23, getColor(minuteTimeStr, dt));
      break;
    case 5:
      pixels.setPixelColor(22, getColor(minuteTimeStr, dt));
      pixels.setPixelColor(23, getColor(minuteTimeStr, dt));
      break;
    default:
      pixels.setPixelColor(23, led_off);  //~십 아닐때
      break;
  }

  int m_ones = m % 10;  // 분: 일의 자리 1부터 9까지
  switch (m_ones) {
    case 1:
      pixels.setPixelColor(26, getColor(minuteTimeStr, dt));
      pixels.setPixelColor(35, getColor(minuteSuffixStr, dt));
      break;
    case 2:
      pixels.setPixelColor(27, getColor(minuteTimeStr, dt));
      pixels.setPixelColor(35, getColor(minuteSuffixStr, dt));
      break;
    case 3:
      pixels.setPixelColor(28, getColor(minuteTimeStr, dt));
      pixels.setPixelColor(35, getColor(minuteSuffixStr, dt));
      break;
    case 4:
      pixels.setPixelColor(29, getColor(minuteTimeStr, dt));
      pixels.setPixelColor(35, getColor(minuteSuffixStr, dt));
      break;
    case 5:
      pixels.setPixelColor(30, getColor(minuteTimeStr, dt));
      pixels.setPixelColor(35, getColor(minuteSuffixStr, dt));
      break;
    case 6:
      pixels.setPixelColor(31, getColor(minuteTimeStr, dt));
      pixels.setPixelColor(35, getColor(minuteSuffixStr, dt));
      break;
    case 7:
      pixels.setPixelColor(32, getColor(minuteTimeStr, dt));
      pixels.setPixelColor(35, getColor(minuteSuffixStr, dt));
      break;
    case 8:
      pixels.setPixelColor(33, getColor(minuteTimeStr, dt));
      pixels.setPixelColor(35, getColor(minuteSuffixStr, dt));
      break;
    case 9:
      pixels.setPixelColor(34, getColor(minuteTimeStr, dt));
      pixels.setPixelColor(35, getColor(minuteSuffixStr, dt));
      break;
    default:
      pixels.setPixelColor(35, getColor(minuteSuffixStr, dt));  //~분
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
  Serial.println(F("\n\n"));
  Serial.println(F("Clock Startup..."));
  rtc.begin();
  pixels.begin();
  pixels.show();  // 모든 LED를 꺼서 초기화


  colorCustomPreset.reset_colorPresetArr();  //프리셋 리셋, 삭제필요
  colorTimePreset.reset_colorPresetArr();    //프리셋 리셋, 삭제필요

  flowWatchFace();

  print_freeMemory();
  Serial.println(F("Clock Booted!!!"));
}
void loop() {
  delay(1);
  if (Serial.available() > 0) {
    processingSerial();
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

    Serial.println();
    printDateTime(now);
    print_freeMemory();
    Serial.print(F("processing_offset: "));
    Serial.println(processing_offset);
  }
}