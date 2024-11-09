#include <RTClib.h>
#include <Adafruit_NeoPixel.h>
#include <Time.h>

#define PIN 6           // LED 스트립이 연결된 핀 번호
#define NUMPIXELS 36    // LED의 개수

RTC_DS3231 rtc;
Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);


#define MAX_PRESETS 60


int showing_minute = 0;
int showing_hour = 0;
bool hourly_notification = true;  //정시알림, 수정필요
int blink_time = 500;


struct color_preset_struct {
  uint8_t led_select_id_bitmask; // 비트마스크, 색상을 적용할 위치, 오전,오후,자정,정오,시_시각,시_접미사,분_시각,분_접미사
  uint8_t startIndexTime; // 1일을 10분 단위로 쪼갬
  uint8_t endIndexTime;   // 1일을 10분 단위로 쪼갬
  uint8_t dayOfWeek_bitmask;  //비트마스크, 적용할 요일
  uint32_t led_color; //pixels.Color(red, green, blue)
};
int color_preset_size = 0;
color_preset_struct color_preset_arr[MAX_PRESETS];

void reset_color_preset_arr() {
  // 초기화: 모든 요소를 기본값으로 설정
  color_preset_size = 0;

  // "새벽" 프리셋 추가 (예: 01:00 ~ 07:00)
  color_preset_arr[color_preset_size++] = {
    0b11111111, // 오전, 자정, 시_시각, 분_시각 활성화
    6,         // startIndexTime: 04:00 (10분 단위 인덱스)
    42,         // endIndexTime: 07:00 (10분 단위 인덱스)
    0b01111111, // 일요일(0)부터 금요일(5)까지 활성화
    pixels.Color(10, 0, 0) // 빨간색 (새벽의 차가운 색상)
  };

  // "대낮" 프리셋 추가 (예: 10:00 ~ 16:00)
  color_preset_arr[color_preset_size++] = {
    0b00110000, // 시_시각, 시_접미사 활성화
    60,         // startIndexTime: 10:00 (10분 단위 인덱스)
    96,         // endIndexTime: 16:00 (10분 단위 인덱스)
    0b01111111, // 일요일(0)부터 금요일(5)까지 활성화
    pixels.Color(255, 255, 0) // 노란색 (밝은 색상)
  };

  // "저녁" 프리셋 추가 (예: 18:00 ~ 20:00)
  color_preset_arr[color_preset_size++] = {
    0b00001010, // 오후, 정오 활성화
    108,        // startIndexTime: 18:00 (10분 단위 인덱스)
    120,        // endIndexTime: 20:00 (10분 단위 인덱스)
    0b01111111, // 일요일(0)부터 금요일(5)까지 활성화
    pixels.Color(255, 100, 0) // 주황색 (저녁의 따뜻한 색상)
  };

  // "한밤" 프리셋 추가 (예: 21:00 ~ 01:00)
  color_preset_arr[color_preset_size++] = {
    0b11110000, // 시_시각, 시_접미사, 자정, 한밤 중 활성화
    126,        // startIndexTime: 21:00 (10분 단위 인덱스)
    6,         // endIndexTime: 03:00 (다음 날 새벽 3시, 10분 단위 인덱스)
    0b01111111, // 일요일(0)부터 금요일(5)까지 활성화
    pixels.Color(50, 50, 0) // 어두운 보라색 아님 (한밤의 고요한 색상)
  };
}

String* get_led_id_arr(struct color_preset_struct preset) {
  // 최대 8개의 ID를 저장할 수 있도록 배열 생성
  static String led_id_arr[8];
  int index = 0;

  // 각 비트에 해당하는 ID를 확인하고 배열에 추가
  if (preset.led_select_id_bitmask & 0b00000001) led_id_arr[index++] = "오전";
  if (preset.led_select_id_bitmask & 0b00000010) led_id_arr[index++] = "오후";
  if (preset.led_select_id_bitmask & 0b00000100) led_id_arr[index++] = "자정";
  if (preset.led_select_id_bitmask & 0b00001000) led_id_arr[index++] = "정오";
  if (preset.led_select_id_bitmask & 0b00010000) led_id_arr[index++] = "시_시각";
  if (preset.led_select_id_bitmask & 0b00100000) led_id_arr[index++] = "시_접미사";
  if (preset.led_select_id_bitmask & 0b01000000) led_id_arr[index++] = "분_시각";
  if (preset.led_select_id_bitmask & 0b10000000) led_id_arr[index++] = "분_접미사";

  // 배열의 나머지 요소는 빈 문자열로 초기화
  for (int i = index; i < 8; i++) {
    led_id_arr[i] = ""; // 빈 문자열로 초기화
  }

  return led_id_arr;
}
bool is_contains_led_id(struct color_preset_struct preset, String target) {
  String* led_id_arr = get_led_id_arr(preset);

  for (int i = 0; i < 8; i++) {
    if (led_id_arr[i] == target) { // 배열의 요소가 target과 일치하는 경우
      return true; // 배열에 해당 문자열이 있음
    }
  }
  return false; // 배열에 해당 문자열이 없음
}
int* get_dayOfWeek_arr(struct color_preset_struct preset) {
  //일: 0, 월: 1, 화: 2, 수: 3, 목: 4, 금: 5, 토: 6
  // 최대 8개의 요일 ID를 저장할 수 있도록 배열 생성
  static int dayOfWeek_arr[8];
  int index = 0;

  // 각 비트에 해당하는 ID를 확인하고 배열에 추가
  if (preset.dayOfWeek_bitmask & 0b00000001) dayOfWeek_arr[index++] = 0;
  if (preset.dayOfWeek_bitmask & 0b00000010) dayOfWeek_arr[index++] = 1;
  if (preset.dayOfWeek_bitmask & 0b00000100) dayOfWeek_arr[index++] = 2;
  if (preset.dayOfWeek_bitmask & 0b00001000) dayOfWeek_arr[index++] = 3;
  if (preset.dayOfWeek_bitmask & 0b00010000) dayOfWeek_arr[index++] = 4;
  if (preset.dayOfWeek_bitmask & 0b00100000) dayOfWeek_arr[index++] = 5;
  if (preset.dayOfWeek_bitmask & 0b01000000) dayOfWeek_arr[index++] = 6;
  if (preset.dayOfWeek_bitmask & 0b10000000) dayOfWeek_arr[index++] = 7;

  // 배열의 나머지 요소는 -1로 초기화하여 사용되지 않음을 표시
  for (int i = index; i < 8; i++) {
    dayOfWeek_arr[i] = -1; // -1로 초기화하여 사용되지 않는 값 표시
  }
  
  //printIntArray(dayOfWeek_arr,8);  //디버깅용

  return dayOfWeek_arr; // 올바른 배열 반환
}
bool is_contains_dayOfWeek(struct color_preset_struct preset, int target) {
  //일: 0, 월: 1, 화: 2, 수: 3, 목: 4, 금: 5, 토: 6
  int* dayOfWeek_arr = get_dayOfWeek_arr(preset);

  for (int i = 0; i < 8; i++) {
    if (dayOfWeek_arr[i] == -1) {
      break; // -1을 만나면 더 이상 유효한 값이 없으므로 반복문 종료
    }
    if (dayOfWeek_arr[i] == target) { // 배열의 요소가 target과 일치하는 경우
      return true; // 배열에 해당 값이 있음
    }
  }
  return false; // 배열에 해당 값이 없음
}

uint8_t get_IndexTime(DateTime dt) {
  // 총 몇 번째 10분 단위인지 계산
  uint8_t IndexTime = (dt.hour() * 60 + dt.minute()) / 10;
  return IndexTime;
}

// 초기화 함수
void initializeColorPresets() {
  color_preset_size = 0;
}

bool addColorPreset(String led_select_id_arr[], uint8_t startIndexTime, uint8_t endIndexTime, int dayOfWeek_arr[], uint32_t led_color) {
  if (color_preset_size >= MAX_PRESETS) {
    // 배열이 가득 찼습니다.
    return false;
  }

  // led_select_id_arr[]를 비트마스크로 변환
  uint8_t led_select_id_bitmask = 0;
  for (int i = 0; led_select_id_arr[i] != ""; i++) {
    if (led_select_id_arr[i] == "오전") led_select_id_bitmask |= 0b00000001;
    if (led_select_id_arr[i] == "오후") led_select_id_bitmask |= 0b00000010;
    if (led_select_id_arr[i] == "자정") led_select_id_bitmask |= 0b00000100;
    if (led_select_id_arr[i] == "정오") led_select_id_bitmask |= 0b00001000;
    if (led_select_id_arr[i] == "시_시각") led_select_id_bitmask |= 0b00010000;
    if (led_select_id_arr[i] == "시_접미사") led_select_id_bitmask |= 0b00100000;
    if (led_select_id_arr[i] == "분_시각") led_select_id_bitmask |= 0b01000000;
    if (led_select_id_arr[i] == "분_접미사") led_select_id_bitmask |= 0b10000000;
  }

  // dayOfWeek_arr[]를 비트마스크로 변환
  uint8_t dayOfWeek_bitmask = 0;
  for (int i = 0; dayOfWeek_arr[i] != -1; i++) {
    if (dayOfWeek_arr[i] >= 0 && dayOfWeek_arr[i] <= 6) {
      dayOfWeek_bitmask |= (1 << dayOfWeek_arr[i]);
    }
  }

  // 새로운 항목 추가
  color_preset_arr[color_preset_size].led_select_id_bitmask = led_select_id_bitmask;
  color_preset_arr[color_preset_size].startIndexTime = startIndexTime;
  color_preset_arr[color_preset_size].endIndexTime = endIndexTime;
  color_preset_arr[color_preset_size].dayOfWeek_bitmask = dayOfWeek_bitmask;
  color_preset_arr[color_preset_size].led_color = led_color;

  color_preset_size++;
  return true;
}

/*// 항목 검색 함수 (구조체 반환)
color_preset_struct getColorPreset(const char* id) {
  for (int i = 0; i < color_preset_size; i++) {
    if (strcmp(color_preset[i].id, id) == 0) {
      return color_preset[i]; // 구조체 반환 (isValid는 이미 true)
    }
  }
  // 찾지 못한 경우
  color_preset_struct emptyPreset;
  emptyPreset.isValid = false; // 유효하지 않은 구조체임을 표시
  return emptyPreset;
}*/

/*// 항목 삭제 함수
bool removeColorPreset(const char* id) {
  int index = -1;
  for (int i = 0; i < color_preset_size; i++) {
    if (strcmp(color_preset[i].id, id) == 0) {
      index = i;
      break;
    }
  }
  if (index == -1) {
    // ID를 찾지 못함
    return false;
  }

  // 항목 삭제: 뒤의 항목들을 앞으로 이동
  for (int i = index; i < color_preset_size - 1; i++) {
    color_preset[i] = color_preset[i + 1];
  }
  color_preset_size--;
  return true;
}*/

/*// 항목 업데이트 함수
bool updateColorPreset(const char* id, uint8_t startTime, uint8_t endTime, uint8_t dayOfWeek, uint32_t led_color) {
  for (int i = 0; i < color_preset_size; i++) {
    if (strcmp(color_preset[i].id, id) == 0) {
      // 기존 항목 업데이트
      color_preset[i].startTime = startTime;
      color_preset[i].endTime = endTime;
      color_preset[i].dayOfWeek = dayOfWeek;
      color_preset[i].led_color = led_color;
      return true;
    }
  }
  // ID를 찾지 못함
  return false;
}*/



// 기본 색상 전역 변수
//Color_Preset["default"] = pixels.Color(10, 0, 0);
//Color_Preset["off"] = pixels.Color(0, 0, 0);
//uint32_t led_default = pixels.Color(255, 255, 255);
uint32_t led_off = pixels.Color(0, 0, 0);

uint32_t get_color(){ //기본 색상값 리턴
  return pixels.Color(255, 255, 255);
}
uint32_t get_color(DateTime now_dt){ //시간단독대응 색상값 리턴
  return pixels.Color(0, 25, 0);
}
uint32_t get_color(String led_select_id, DateTime now_dt) {
  uint8_t currentIndexTime = (now_dt.hour() * 60 + now_dt.minute()) / 10;
  int currentDayOfWeek = now_dt.dayOfTheWeek();

  // color_preset_arr 배열을 순회하며 조건에 맞는 항목을 찾음
  for (int i = 0; i < color_preset_size; i++) {
    color_preset_struct preset = color_preset_arr[i];

    //Serial.print("Checking preset ");
    //Serial.print(i);
    //Serial.print(": led_select_id_bitmask: ");
    //Serial.println(preset.led_select_id_bitmask, BIN);
    
    // 현재 요일이 적용 가능한지 확인
    if (is_contains_dayOfWeek(preset, currentDayOfWeek)) {
      //Serial.println("Day of week matched.");

      // 시간 범위가 같은 날 내에 있는 경우
      if (preset.startIndexTime <= preset.endIndexTime) {
        if (currentIndexTime >= preset.startIndexTime && currentIndexTime < preset.endIndexTime) {
          //Serial.println("Time matched.");

          // led_select_id가 포함되어 있는지 확인
          if (is_contains_led_id(preset, led_select_id)) {
            //Serial.println("LED select ID matched. Returning preset color.");
            return preset.led_color; // 조건을 만족하면 즉시 led_color 반환
          }
        }
      }
      // 시간 범위가 다음날로 넘어가는 경우 (예: 20시 ~ 익일 07시)
      else {
        if (currentIndexTime >= preset.startIndexTime || currentIndexTime < preset.endIndexTime) {
          //Serial.println("Time matched (next day).");

          // led_select_id가 포함되어 있는지 확인
          if (is_contains_led_id(preset, led_select_id)) {
            //Serial.println("LED select ID matched. Returning preset color.");
            return preset.led_color; // 조건을 만족하면 즉시 led_color 반환
          }
        }
      }
    }
  }

  // 조건을 만족하는 preset이 없으면 시간 단독 대응 색상 반환
  //Serial.println("No matching preset found. Returning default color.");
  return get_color(now_dt); // 시간 단독 대응 기본 색상 반환
  //return led_off;
}


void flow_watchFace(DateTime now_dt){
  for (int i = 0; i < NUMPIXELS; i++) {
    pixels.setPixelColor(i, get_color(now_dt)); // 모든 LED를 시간대응으로 설정
    pixels.show();
    delay(80);
  }
  for (int i = 0; i < NUMPIXELS; i++) {
    pixels.setPixelColor(i, led_off); // 모든 LED를 꺼짐으로 설정
    pixels.show();
    delay(80);
  }
  delay(150);
}
void flow_watchFace(){
  for (int i = 0; i < NUMPIXELS; i++) {
    pixels.setPixelColor(i, get_color()); // 모든 LED를 흰색으로 설정
    pixels.show();
    delay(80);
  }
  for (int i = 0; i < NUMPIXELS; i++) {
    pixels.setPixelColor(i, led_off); // 모든 LED를 꺼짐으로 설정
    pixels.show();
    delay(80);
  }
  delay(150);
}

void blink__watchFace(DateTime now_dt){
  pixels.fill(get_color(now_dt), 0, NUMPIXELS);
  pixels.show();
  delay(blink_time);
  pixels.fill(led_off, 0, NUMPIXELS);
  pixels.show();

  showing_minute = 0;
  showing_hour = 0;
}
void blink__watchFace(){
  pixels.fill(get_color(), 0, NUMPIXELS);
  pixels.show();
  delay(blink_time);
  pixels.fill(led_off, 0, NUMPIXELS);
  pixels.show();

  showing_minute = 0;
  showing_hour = 0;
}




void printDateTime(DateTime dt) {
  // DateTime 객체를 시리얼로 출력하는 함수
  Serial.print(dt.year());
  Serial.print('/');
  Serial.print(dt.month());
  Serial.print('/');
  Serial.print(dt.day());
  Serial.print(' ');
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
      Serial.print(", "); // 각 요소 사이에 쉼표 추가
    }
  }
  Serial.println(); // 배열 출력 후 줄 바꿈
}
void printIntArray(int arr[], int size) {
  for (int i = 0; i < size; i++) {
    Serial.print(arr[i]);
    if (i < size - 1) {
      Serial.print(", "); // 각 요소 사이에 쉼표 추가
    }
  }
  Serial.println(); // 배열 출력 후 줄 바꿈
}



void adjust_by_UTCK(String rowStrData,int atIndex){
  Serial.println("First character is '@', proceeding with UTCK.");
      if (rowStrData.length() >= 8) {
        int newHour = rowStrData.substring(atIndex+1, atIndex+3).toInt(); // 시간
        int newMinute = rowStrData.substring(atIndex+3, atIndex+5).toInt(); // 분
        int newSecond = rowStrData.substring(atIndex+5, atIndex+7).toInt(); // 초

        // 현재 날짜를 유지하고 시간만 변경
        DateTime now = rtc.now();
        DateTime adjustedTime(now.year(), now.month(), now.day(), newHour, newMinute, newSecond);
        rtc.adjust(adjustedTime);

        Serial.print("Adjusted by UTCK3: ");
        printDateTime(adjustedTime); // 수정된 시간 출력
      } else {
        Serial.println("Error: Input data is too short.");
      }
}


void Proc_Serial() {
  if (Serial.available() > 0) {
    String rowStrData = Serial.readStringUntil('\n');
    Serial.println("Received Serial Data: " + rowStrData);

    // UTCK3 연동, 첫 문자가 '@'인지 확인
    int atIndex = rowStrData.indexOf('@');
    if (atIndex != -1) {
      adjust_by_UTCK(rowStrData,atIndex);
      //blink__watchFace(rtc.now());
      
    } else {
      Serial.println("Error: First character is not '@'.");
    }
  }
}

void refresh_watchFace(DateTime dt){
  pixels.clear();
  int h=dt.hour();
  int m=dt.minute();
  if (h > 0 && h < 12) {  // 오전
    pixels.setPixelColor(0, get_color("오전",dt));
    pixels.setPixelColor(1, get_color("오전",dt));
    pixels.setPixelColor(35, get_color("분_접미사",dt));
  } else if (h > 12) {  // 오후
    pixels.setPixelColor(0, get_color("오후",dt));
    pixels.setPixelColor(6, get_color("오후",dt));
    pixels.setPixelColor(35, get_color("분_접미사",dt));
    h = h - 12;  // 시간: 오전, 오후 12시간제로 변환
  } else if (h == 0) { // 자정
    pixels.setPixelColor(18, get_color("자정",dt));
    pixels.setPixelColor(24, get_color("자정",dt));
  } else if (h == 12) { // 정오
    pixels.setPixelColor(24, get_color("정오",dt));
    pixels.setPixelColor(25, get_color("정오",dt));
  } else {
    pixels.clear();
  }

  switch (h) {  // 시간: 1부터 12까지
    case 1:
      pixels.setPixelColor(3, get_color("시_시각",dt));
      pixels.setPixelColor(17, get_color("시_접미사",dt)); //~시
      break;
    case 2:
      pixels.setPixelColor(4, get_color("시_시각",dt));
      pixels.setPixelColor(17, get_color("시_접미사",dt)); //~시
      break;
    case 3:
      pixels.setPixelColor(5, get_color("시_시각",dt));
      pixels.setPixelColor(17, get_color("시_접미사",dt)); //~시
      break;
    case 4:
      pixels.setPixelColor(11, get_color("시_시각",dt));
      pixels.setPixelColor(17, get_color("시_접미사",dt)); //~시
      break;
    case 5:
      pixels.setPixelColor(12, get_color("시_시각",dt));
      pixels.setPixelColor(13, get_color("시_시각",dt));
      pixels.setPixelColor(17, get_color("시_접미사",dt)); //~시
      break;
    case 6:
      pixels.setPixelColor(7, get_color("시_시각",dt));
      pixels.setPixelColor(13, get_color("시_시각",dt));
      pixels.setPixelColor(17, get_color("시_접미사",dt)); //~시
      break;
    case 7:
      pixels.setPixelColor(8, get_color("시_시각",dt));
      pixels.setPixelColor(14, get_color("시_시각",dt));
      pixels.setPixelColor(17, get_color("시_접미사",dt)); //~시
      break;
    case 8:
      pixels.setPixelColor(9, get_color("시_시각",dt));
      pixels.setPixelColor(10, get_color("시_시각",dt));
      pixels.setPixelColor(17, get_color("시_접미사",dt)); //~시
      break;
    case 9:
      pixels.setPixelColor(15, get_color("시_시각",dt));
      pixels.setPixelColor(16, get_color("시_시각",dt));
      pixels.setPixelColor(17, get_color("시_접미사",dt)); //~시
      break;
    case 10:
      pixels.setPixelColor(2, get_color("시_시각",dt));
      pixels.setPixelColor(17, get_color("시_접미사",dt)); //~시
      break;
    case 11:
      pixels.setPixelColor(2, get_color("시_시각",dt));
      pixels.setPixelColor(3, get_color("시_시각",dt));
      pixels.setPixelColor(17, get_color("시_접미사",dt)); //~시
      break; 
    case 12:
      pixels.setPixelColor(2, get_color("시_시각",dt));
      pixels.setPixelColor(4, get_color("시_시각",dt));
      pixels.setPixelColor(17, get_color("시_접미사",dt)); //~시
      break;
    default:
      break;
      
  }
  

  int m_tens = m / 10;  // 분: 십의 자리 1부터 5까지
  switch (m_tens) {
    case 1:
      pixels.setPixelColor(23, get_color("분_시각",dt));
      break;
    case 2:
      pixels.setPixelColor(19, get_color("분_시각",dt));
      pixels.setPixelColor(23, get_color("분_시각",dt));
      break;
    case 3:
      pixels.setPixelColor(20, get_color("분_시각",dt));
      pixels.setPixelColor(23, get_color("분_시각",dt));
      break;
    case 4:
      pixels.setPixelColor(21, get_color("분_시각",dt));
      pixels.setPixelColor(23, get_color("분_시각",dt));
      break;
    case 5:
      pixels.setPixelColor(22, get_color("분_시각",dt));
      pixels.setPixelColor(23, get_color("분_시각",dt));
      break;
    default:
      pixels.setPixelColor(23, led_off); //~십 아닐때
      break;
  }
  
  int m_ones = m % 10;  // 분: 일의 자리 1부터 9까지
  switch (m_ones) {
    case 1:
      pixels.setPixelColor(26, get_color("분_시각",dt));
      pixels.setPixelColor(35, get_color("분_접미사",dt));
      break;
    case 2:
      pixels.setPixelColor(27, get_color("분_시각",dt));
      pixels.setPixelColor(35, get_color("분_접미사",dt));
      break;
    case 3:
      pixels.setPixelColor(28, get_color("분_시각",dt));
      pixels.setPixelColor(35, get_color("분_접미사",dt));
      break;
    case 4:
      pixels.setPixelColor(29, get_color("분_시각",dt));
      pixels.setPixelColor(35, get_color("분_접미사",dt));
      break;
    case 5:
      pixels.setPixelColor(30, get_color("분_시각",dt));
      pixels.setPixelColor(35, get_color("분_접미사",dt));
      break;
    case 6:
      pixels.setPixelColor(31, get_color("분_시각",dt));
      pixels.setPixelColor(35, get_color("분_접미사",dt));
      break;
    case 7:
      pixels.setPixelColor(32, get_color("분_시각",dt));
      pixels.setPixelColor(35, get_color("분_접미사",dt));
      break;
    case 8:
      pixels.setPixelColor(33, get_color("분_시각",dt));
      pixels.setPixelColor(35, get_color("분_접미사",dt));
      break;
    case 9:
      pixels.setPixelColor(34, get_color("분_시각",dt));
      pixels.setPixelColor(35, get_color("분_접미사",dt));
      break;
    default:
      pixels.setPixelColor(35, get_color("분_접미사",dt)); //~분
      break;
  }
  
  if (m_tens == 0 && m_ones == 0) { // 정시
    pixels.setPixelColor(35, led_off);
  }
  
  pixels.show();
}


unsigned long processing_offset=0;
void setup() {
  rtc.begin();
  Serial.begin(4800);
  pixels.begin();
  pixels.show(); // 모든 LED를 꺼서 초기화

  reset_color_preset_arr(); //프리셋 리셋, 삭제필요

  flow_watchFace();
}
void loop() {
  delay(5);
  if (Serial.available() > 0) {
    Proc_Serial();
  }
  
  DateTime now = rtc.now();
  

    
  if(showing_minute != now.minute() || showing_hour != now.hour()){
    unsigned long start_processing_offset=millis();
    
    if(showing_hour != now.hour() && hourly_notification==true){
      blink__watchFace(now);
    }
    
    showing_hour=now.hour();
    showing_minute=now.minute();
    refresh_watchFace(now);
    
    processing_offset = millis() - start_processing_offset;
    Serial.print("processing_offset: ");
    Serial.println(processing_offset);
  }
}
