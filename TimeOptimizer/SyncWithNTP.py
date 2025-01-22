import ntplib
import csv
import socket
from datetime import datetime, timezone


def request_ntp_time(server_address):
    try:
        client = ntplib.NTPClient()
        print(f"다음 주소의 NTP 서버 요청 중: {server_address}")
        response = client.request(server_address, version=3)
        ntp_time = datetime.fromtimestamp(response.tx_time, tz=timezone.utc)
        print(f"NTP 서버({server_address})에서 시간 수신 성공: {ntp_time}")
        return ntp_time
    except (ntplib.NTPException,socket.gaierror) as e:
        print(f"NTP 서버({server_address})에 연결 실패: {e}")
        raise ConnectionError(e) from e


def get_local_rtc_time():
    return datetime.now(timezone.utc)


class ConnectToNTP:
    def __init__(self, ntp_servers_file="ntp_servers.csv"):
        """
        ConnectToNTP 클래스 초기화
        :param ntp_servers_file: NTP 서버 리스트 파일 경로
        """
        self.ntp_servers = None
        self.load_ntp_servers(ntp_servers_file)

    def load_ntp_servers(self, file_path):
        """
        파일에서 NTP 서버 리스트-딕셔너리로 읽어옵니다.
        :param file_path: NTP 서버 리스트 파일 경로
        :return: NTP 서버 리스트 (list)
        """
        try:
            result = []
            with open(file_path, mode="r", encoding="utf-8") as file:
                reader = csv.DictReader(file)
                for i, row in enumerate(reader, start=1):  # 1부터 시작하는 인덱스
                    row_data = {"index": i}
                    row_data.update(row)  # 기존 CSV의 데이터를 추가
                    result.append(row_data)
            self.ntp_servers = result
            return result
        except FileNotFoundError:
            print(f"파일을 찾을 수 없습니다: {file_path}")
            return []

    def get_ntp_list(self):
        return self.ntp_servers

    def auto_get_ntp_time(self, server_address=None, server_name=None, server_index=None):
        """
        NTP 서버 리스트를 순회하며 시간을 가져옵니다.
        :return: UTC 시간의 datetime 객체 또는 None
        """
        if server_address is None:
            for server in self.ntp_servers:
                if server["name"] == server_name or server["index"] == server_index or (server_name is None and server_index is None):
                    try:
                        print(f"""{server["name"]} NTP 서버({server["server"]})에 연결 시도 중""")
                        return request_ntp_time(server["server"])
                    except ConnectionError as e:
                        print(f"{e}로 인해 다음 서버에 연결 시도")
            print("모든 NTP 서버에 연결할 수 없습니다. 로컬 시간을 사용합니다.")
            return get_local_rtc_time()
        else:
            return request_ntp_time(server_address)

    def print_all_ntp_time(self):
        """
        NTP 서버 리스트를 순회하며 시간을 가져옵니다.
        :return: UTC 시간의 datetime 객체 또는 None
        """
        client = ntplib.NTPClient()
        ntp_time=None
        for server in self.ntp_servers:
            try:
                print(f"""NTP 서버 요청 중: {server["name"]}""")
                response = client.request(server["server"], version=3)
                ntp_time = datetime.fromtimestamp(response.tx_time, tz=timezone.utc)
                print(f"""{server["name"]} NTP 서버({server["server"]})에서 시간 수신 성공: {ntp_time}""")
                print("NTP 시간 (로컬):", ntp_time.astimezone(),"\n")
            except (ntplib.NTPException, socket.gaierror) as e:
                print(f"""{server["name"]} NTP 서버({server["server"]})에 연결 실패: {e}""")
        return ntp_time


# 테스트 실행
if __name__ == "__main__":
    ntp_client = ConnectToNTP()  # 기본 ntp_servers.csv.txt 사용
    ntp_time = ntp_client.auto_get_ntp_time(server_name="NTP풀")
    #ntp_time = ntp_client.print_all_ntp_time()
    if ntp_time:
        print("NTP 시간 (로컬):", ntp_time.astimezone())  # 로컬 시간으로 변환
