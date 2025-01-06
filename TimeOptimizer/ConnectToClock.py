import serial
import serial.tools.list_ports
import time
from dataclasses import dataclass
from datetime import datetime,timedelta
import subprocess
import os
import platform
import shutil
import zipfile
import urllib.request
import re
import json
from json import JSONDecodeError

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


def find_arduino_port():
    """Find the port where Arduino is connected"""
    import serial.tools.list_ports
    ports = serial.tools.list_ports.comports()
    for port in ports:
        if "Arduino" in port.description or "USB Serial" in port.description:
            print(f"Arduino found on port: {port.device}")
            return port.device
    raise RuntimeError("Arduino not found. Please connect the Arduino.")

class ConnectionToClock:
    """
    아두이노와의 연결을 설정하고 명령을 주고받는 클래스
    """

    def __init__(self, port=None, speed=4800):
        self.speed = speed
        self.port = port
        if port is None:
            port = find_arduino_port()  # 아두이노 포트 자동 탐지
        if port is None:
            raise Exception("Arduino port not found. Please connect the Arduino.")
        self.arduino = serial.Serial(port=port, baudrate=speed, timeout=1)
        print(f"Connected to Arduino on port {self.port} with baudrate {self.speed}")

    def waiting_boot(self, boot_str, maxWait=60):
        """Read from the serial port until the target value is found"""
        try:
            start_time = datetime.now()
            print(f"Waiting for {boot_str}...")
            while datetime.now() - start_time < timedelta(seconds=maxWait):
                if self.arduino.in_waiting > 0:
                    data=self.arduino.readline().decode('utf-8').strip()
                    print(data)
                    if boot_str in data:
                        print(f"{boot_str} received.")
                        time.sleep(3)
                        return True


            print(f"Waiting for {boot_str} timed out.")
            return False

        except serial.SerialException as e:
            print(f"Serial communication error: {e}")
        except Exception as e:
            print(f"Error: {e}")


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
        dict_for_json = {"function_str": "adjust_time", "datetime": formatted_dt}
        time.sleep(remaining_microseconds / 1000000)
        return self.send_serial(dict_for_json)

    def send_now_time(self):
        return self.send_time(datetime.now())



class ArduinoCLI:
    def __init__(self, port=None):
        self.cli_path = self._check_and_install_cli()
        self.port = port or find_arduino_port()
        self.fqbn = self.get_fqbn(self.port)


    def _check_and_install_cli(self):
        """Check if Arduino CLI is installed, and install it if not"""
        cli_path = shutil.which("arduino-cli")
        if cli_path:
            print("Arduino CLI is already installed.")
            return os.path.abspath(cli_path)  # 절대 경로 반환

        print("Arduino CLI is not installed. Installing...")

        # Determine OS
        os_name = platform.system().lower()
        if os_name == "windows":
            self._install_cli_windows()
            cli_path = os.path.join(os.getcwd(), "arduino-cli.exe")

        elif os_name in ["linux", "darwin"]:  # Linux or macOS
            install_command = (
                "curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh"
            )
            result = run_command(install_command)
            cli_path = shutil.which("arduino-cli")

        else:
            raise RuntimeError("Unsupported operating system: " + os_name)

        if cli_path and os.path.exists(cli_path):
            print("Arduino CLI installed successfully.")
            return cli_path
        else:
            raise RuntimeError("Failed to install Arduino CLI. Please check your environment.")

    def _install_cli_windows(self):
        """Install Arduino CLI on Windows without PowerShell"""
        try:
            url = "https://downloads.arduino.cc/arduino-cli/arduino-cli_latest_Windows_64bit.zip"
            zip_path = "arduino-cli.zip"
            extract_path = os.getcwd()

            print("Downloading Arduino CLI...")
            urllib.request.urlretrieve(url, zip_path)

            print("Extracting Arduino CLI...")
            with zipfile.ZipFile(zip_path, 'r') as zip_ref:
                zip_ref.extractall(extract_path)

            os.remove(zip_path)
            print("Arduino CLI downloaded and extracted successfully.")
        except Exception as e:
            raise RuntimeError(f"Failed to install Arduino CLI on Windows: {e}")

    def run(self, command):
        """Run an Arduino CLI command"""
        full_command = f"{self.cli_path} {command}"
        print(f"Running command: {full_command}")
        return run_command(full_command)

    def extract_libraries_from_sketch(self, sketch_path):
        """Extract required libraries from the sketch file comments"""
        try:
            with open(sketch_path, 'r', encoding='utf-8') as sketch_file:
                sketch_content = sketch_file.read()
            # Match @libraries comments to extract library names
            libraries = re.findall(r'@library ([A-Za-z0-9_\- ]+)', sketch_content)
            print(f"Extracted libraries from comments: {libraries}")
            return libraries
        except FileNotFoundError:
            raise RuntimeError(f"Sketch file not found: {sketch_path}")


    def get_fqbn(self, port):
        """Automatically get the FQBN for the board connected to the specified port"""
        print("Detecting board FQBN...")
        board_list_output = self.run("board list --format json")
        if not board_list_output:
            raise RuntimeError("Failed to list boards. Make sure the board is connected.")

        try:
            import json
            board_list = json.loads(board_list_output)
            for port_info in board_list.get("detected_ports", []):
                if port_info.get("port", {}).get("address") == port:
                    matching_boards = port_info.get("matching_boards", [])
                    if matching_boards:
                        fqbn = matching_boards[0].get("fqbn")
                        if fqbn:
                            print(f"Detected FQBN: {fqbn}")
                            return fqbn
                        else:
                            print(f"No FQBN found for the board connected to {port}.")
                            raise RuntimeError(f"No FQBN found for port {port}.")
        except json.JSONDecodeError:
            raise RuntimeError("Failed to parse board list output.")

        raise RuntimeError(f"No board detected on port {port}. Please check the connection.")


    def install_libraries_and_upload(self, sketch_path):
        """Install required libraries and upload the sketch to the Arduino board"""
        try:
            libraries = self.extract_libraries_from_sketch(sketch_path)
            for lib in libraries:
                print(f"Installing library: {lib}...")
                result = self.run(f"lib install \"{lib}\"")
                if not result:
                    print(f"Warning: Library {lib} installation failed.")

            print("Compiling sketch...")
            compile_result = self.run(f"compile --fqbn {self.fqbn} {sketch_path}")
            if not compile_result:
                raise RuntimeError("Sketch compilation failed.")

            print("Uploading sketch to the board...")
            upload_result = self.run(f"upload -p {self.port} --fqbn {self.fqbn} {sketch_path}")
            if not upload_result:
                raise RuntimeError("Sketch upload failed.")

            print("Sketch uploaded successfully.")
        except Exception as e:
            print(f"install_libraries_and_upload Error: {e}")


def run_command(command):
    """Run a shell command and return the output"""
    try:
        result = subprocess.run(command, capture_output=True, text=True, shell=True, encoding='utf-8', errors='replace')
        if result.returncode == 0:
            print(f"Command output: {result.stdout}")
            return result.stdout
        else:
            print(f"Error run_command: {result.stderr}")
            return None
    except Exception as e:
        print(f"run_command failed: {e}")
        return None


if __name__ == "__main__":
    ArduinoCLI().install_libraries_and_upload("../Korean_Clock_OOP/Korean_Clock_OOP.ino")

    print(datetime)
    test_connection=ConnectionToClock()
    if not test_connection.waiting_boot("Clock Booted"):
        raise RuntimeError("Clock Boot Failed")
    while True:
        print(test_connection.send_now_time())
        time.sleep(1)

