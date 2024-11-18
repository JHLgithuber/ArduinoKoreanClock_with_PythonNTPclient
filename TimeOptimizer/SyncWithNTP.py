import ntplib
from datetime import datetime, timezone

class ConnectToNTP:
    def __init__(self, ntp_servers_file="ntp_servers"):
        """
        ConnectToNTP 클래스 초기화
        :param ntp_servers_file: NTP 서버 리스트 파일 경로
        """
        self.ntp_servers = self.load_ntp_servers(ntp_servers_file)

    def load_ntp_servers(self, file_path):
        """
        파일에서 NTP 서버 리스트를 읽어옵니다.
        :param file_path: NTP 서버 리스트 파일 경로
        :return: NTP 서버 리스트 (list)
        """
        try:
            with open(file_path, "r") as file:
                servers = [line.strip() for line in file if line.strip()]  # 빈 줄 제거
            print(f"NTP 서버 리스트: {servers}")
            return servers
        except FileNotFoundError:
            print(f"파일을 찾을 수 없습니다: {file_path}")
            return []


    def __request_ntp_time(self,server):
        try:
            client = ntplib.NTPClient()
            print(f"NTP 서버 요청 중: {server}")
            response = client.request(server, version=3)
            ntp_time = datetime.fromtimestamp(response.tx_time, tz=timezone.utc)
            print(f"NTP 서버({server})에서 시간 수신 성공: {ntp_time}")
            return ntp_time
        except ntplib.NTPException as e:
            print(f"NTP 서버({server})에 연결 실패: {e}")



    def get_ntp_time(self, server_address=None):
        """
        NTP 서버 리스트를 순회하며 시간을 가져옵니다.
        :return: UTC 시간의 datetime 객체 또는 None
        """
        if server_address is None:
            for server in self.ntp_servers:
                return self.__request_ntp_time(server)
            print("모든 NTP 서버에 연결할 수 없습니다.")
            return None
        else:
            return self.__request_ntp_time(server_address)



    def print_all_ntp_time(self):
        """
        NTP 서버 리스트를 순회하며 시간을 가져옵니다.
        :return: UTC 시간의 datetime 객체 또는 None
        """
        client = ntplib.NTPClient()
        ntp_time=None
        for server in self.ntp_servers:
            try:
                print(f"NTP 서버 요청 중: {server}")
                response = client.request(server, version=3)
                ntp_time = datetime.fromtimestamp(response.tx_time, tz=timezone.utc)
                print(f"NTP 서버({server})에서 시간 수신 성공: {ntp_time}")
                print("NTP 시간 (로컬):", ntp_time.astimezone(),"\n")
            except ntplib.NTPException as e:
                print(f"NTP 서버({server})에 연결 실패: {e}")
        return ntp_time


# 테스트 실행
if __name__ == "__main__":
    ntp_client = ConnectToNTP()  # 기본 ntp_servers.txt 사용
    ntp_time = ntp_client.get_ntp_time()
    #ntp_time = ntp_client.print_all_ntp_time()
    if ntp_time:
        print("NTP 시간 (로컬):", ntp_time.astimezone())  # 로컬 시간으로 변환
