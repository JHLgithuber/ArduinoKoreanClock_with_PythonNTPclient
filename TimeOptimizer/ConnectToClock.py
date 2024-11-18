import serial
import serial.tools.list_ports
import time
from dataclasses import dataclass
from datetime import datetime,timedelta
import json


@dataclass
class RGBStruct:
    r: int  # 0-255
    g: int  # 0-255
    b: int  # 0-255


@dataclass
class ColorPresetStruct:
    priority: int  # 우선순위
    ledID_bitmask: int  # 비트마스크
    startIndexTime: int  # 시작 시간 인덱스 (10분 단위)
    endIndexTime: int  # 종료 시간 인덱스 (10분 단위)
    dayOfWeek_bitmask: int  # 요일 비트마스크
    ledColor: RGBStruct  # RGBStruct 인스턴스


def find_arduino():
    """
    연결된 포트 중 아두이노가 연결된 포트를 탐지합니다.
    """
    ports = serial.tools.list_ports.comports()
    for port in ports:
        if "Arduino" in port.description or "USB Serial" in port.description:
            print(f"Arduino found on port: {port.device}")
            return port.device
    print("Arduino not found. Please check the connection.")
    return None


class ConnectionToClock:
    """
    아두이노와의 연결을 설정하고 명령을 주고받는 클래스
    """

    def __init__(self, port=None, speed=4800):
        self.speed = None
        self.port = None
        if port is None:
            port = find_arduino()  # 아두이노 포트 자동 탐지
        if port is None:
            raise Exception("Arduino port not found. Please connect the Arduino.")
        self.arduino = serial.Serial(port=port, baudrate=speed, timeout=1)
        print(f"Connected to Arduino on port {port} with baudrate {speed}")

    def send_serial(self, param):
        """아두이노에 명령 전송"""
        if isinstance(param, dict):
            command = json.dumps(param)
        elif isinstance(param, str):
            command = param
        else:
            raise TypeError("입력값은 문자열이나 딕셔너리만 가능합니다.")

        self.arduino.write(command.encode())  # 문자열을 바이트로 인코딩하여 전송
        time.sleep(0.1)  # 약간의 대기 시간
        if self.arduino.in_waiting > 0:  # 아두이노 응답 읽기
            response = self.arduino.readline().decode('utf-8').strip()
            print(f"Arduino response: {response}\n")
            return response
        else:
            print("No response from Arduino.\n")

    def close_serial(self):
        """시리얼 포트 닫기"""
        self.arduino.close()
        print("Connection closed.")

    def change_baudrate(self, new_speed):
        """
        보드레이트를 변경하고 시리얼 연결을 재설정합니다.
        """
        print(f"Changing baudrate from {self.speed} to {new_speed}...")
        self.arduino.close()  # 기존 연결 닫기
        self.speed = new_speed  # 보드레이트 업데이트
        self.arduino = serial.Serial(port=self.port, baudrate=self.speed, timeout=1)  # 새 연결 설정
        print(f"Baudrate changed to {self.speed}")

    def send_time(self, dt):
        remaining_microseconds = 1000000 - dt.microsecond
        delayed_time = dt + timedelta(microseconds=remaining_microseconds)
        formatted_dt = dt.strftime("%Y-%m-%d %H:%M:%S")
        dict_for_json = {"function": "adjust_time", "datetime": formatted_dt}
        time.sleep(remaining_microseconds / 1000000)
        return self.send_serial(dict_for_json)

    def send_now_time(self):
        return self.send_time(datetime.now())


if __name__ == "__main__":
    print(datetime)
    test_connection=ConnectionToClock()
    while True:
        print(test_connection.send_now_time())
        time.sleep(1)

