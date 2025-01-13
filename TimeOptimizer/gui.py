import tkinter as tk
from tkinter import ttk, messagebox, colorchooser
import serial
import json
from tkcalendar import Calendar
import datetime

class KoreanClockGUI:
    def __init__(self, root):
        self.root = root
        self.root.title("한글 시계 조정 프로그램")
        self.root.geometry("800x500")
        self.root.resizable(False, False)

        self.serial_connection = None  # 시리얼 연결 객체

        self._create_widgets()

    def _create_widgets(self):
        """GUI 요소를 생성합니다."""
        # 그리드 레이아웃 설정
        self.root.columnconfigure(0, weight=1)
        self.root.columnconfigure(1, weight=1)
        self.root.rowconfigure(0, weight=1)
        self.root.rowconfigure(1, weight=1)

        # 좌상단: 현재 시간
        current_time_frame = ttk.LabelFrame(self.root, text="현재 시간", padding=(10, 10))
        current_time_frame.grid(row=0, column=0, sticky="nsew", padx=10, pady=10)

        self.current_time_label = ttk.Label(current_time_frame, text="2025-01-13 15:30:00", font=("Arial", 12))
        self.current_time_label.pack(padx=5, pady=5)

        # 우상단: 시리얼 포트 설정
        port_frame = ttk.LabelFrame(self.root, text="시리얼 포트 설정", padding=(10, 10))
        port_frame.grid(row=0, column=1, sticky="nsew", padx=10, pady=10)

        self.port_combobox = ttk.Combobox(
            port_frame, values=["COM1", "COM2", "COM3"], state="readonly", font=("Arial", 10)
        )
        self.port_combobox.set("포트를 선택하세요")
        self.port_combobox.pack(fill="x", padx=5, pady=5)

        connect_button = ttk.Button(port_frame, text="연결", command=self._connect_port)
        connect_button.pack(pady=5, ipadx=10, ipady=5)

        change_speed_frame = ttk.LabelFrame(port_frame, text="속도 변경", padding=(10, 10))
        change_speed_frame.pack(fill="x", padx=5, pady=5)

        self.baudrate_combobox = ttk.Combobox(
            change_speed_frame, values=["4800", "9600", "19200", "115200"], state="readonly", font=("Arial", 10)
        )
        self.baudrate_combobox.set("4800")
        self.baudrate_combobox.pack(fill="x", padx=5, pady=5)

        change_speed_button = ttk.Button(change_speed_frame, text="속도 변경", command=self._change_speed)
        change_speed_button.pack(pady=5, ipadx=10, ipady=5)

        # 좌하단: 시간 설정
        time_frame = ttk.LabelFrame(self.root, text="시간 설정", padding=(10, 10))
        time_frame.grid(row=1, column=0, sticky="nsew", padx=10, pady=10)

        manual_time_button = ttk.Button(time_frame, text="수동 시간 설정", command=self._open_manual_time_window)
        manual_time_button.pack(pady=10, ipadx=10, ipady=5)

        ntp_sync_button = ttk.Button(time_frame, text="NTP 동기화", command=self._sync_ntp)
        ntp_sync_button.pack(pady=10, ipadx=10, ipady=5)

        # 우하단: 색상 설정
        color_frame = ttk.LabelFrame(self.root, text="색상 설정", padding=(10, 10))
        color_frame.grid(row=1, column=1, sticky="nsew", padx=10, pady=10)

        choose_color_button = ttk.Button(color_frame, text="색상 선택", command=self._choose_color)
        choose_color_button.pack(pady=10, ipadx=10, ipady=5)

        preset_frame = ttk.LabelFrame(color_frame, text="프리셋 설정", padding=(10, 10))
        preset_frame.pack(fill="x", padx=5, pady=5)

        add_preset_button = ttk.Button(preset_frame, text="프리셋 추가", command=self._add_preset)
        add_preset_button.pack(pady=5, ipadx=10, ipady=5)

        reset_preset_button = ttk.Button(preset_frame, text="프리셋 리셋", command=self._reset_preset)
        reset_preset_button.pack(pady=5, ipadx=10, ipady=5)

        # 상태 출력
        self.status_label = tk.Label(
            self.root, text="상태: 대기 중", font=("Arial", 10), fg="#555"
        )
        self.status_label.grid(row=2, column=0, columnspan=2, pady=10)

    def _open_manual_time_window(self):
        """수동 시간 설정 창을 엽니다."""
        manual_window = tk.Toplevel(self.root)
        manual_window.title("수동 시간 설정")
        manual_window.geometry("400x350")

        # 캘린더 위젯
        calendar = Calendar(manual_window, selectmode='day', date_pattern='yyyy-mm-dd')
        calendar.grid(row=0, column=0, columnspan=3, pady=10, padx=10)

        # 시간 입력 위젯
        hour_label = ttk.Label(manual_window, text="시:")
        hour_label.grid(row=1, column=0, padx=5, pady=5)
        hour_entry = ttk.Entry(manual_window, width=5, font=("Arial", 10))
        hour_entry.insert(0, "12")
        hour_entry.grid(row=1, column=1, padx=5, pady=5)

        minute_label = ttk.Label(manual_window, text="분:")
        minute_label.grid(row=2, column=0, padx=5, pady=5)
        minute_entry = ttk.Entry(manual_window, width=5, font=("Arial", 10))
        minute_entry.insert(0, "00")
        minute_entry.grid(row=2, column=1, padx=5, pady=5)

        second_label = ttk.Label(manual_window, text="초:")
        second_label.grid(row=3, column=0, padx=5, pady=5)
        second_entry = ttk.Entry(manual_window, width=5, font=("Arial", 10))
        second_entry.insert(0, "00")
        second_entry.grid(row=3, column=1, padx=5, pady=5)

        def set_manual_time():
            selected_date = calendar.get_date()
            hour = hour_entry.get()
            minute = minute_entry.get()
            second = second_entry.get()
            time_value = f"{selected_date} {hour}:{minute}:{second}"
            if self.serial_connection:
                command = json.dumps({"function": "adjust_time", "datetime": time_value})
                self.serial_connection.write(command.encode())
                messagebox.showinfo("시간 설정", f"시간이 {time_value}로 설정되었습니다.")
            else:
                messagebox.showerror("오류", "시리얼 포트가 연결되지 않았습니다.")

        set_time_button = ttk.Button(manual_window, text="시간 설정", command=set_manual_time)
        set_time_button.grid(row=4, column=0, columnspan=3, pady=10, ipadx=10, ipady=5)

    def _sync_ntp(self):
        """NTP 동기화 이벤트 핸들러."""
        if self.serial_connection:
            command = json.dumps({"function": "sync_ntp"})
            self.serial_connection.write(command.encode())
            messagebox.showinfo("NTP 동기화", "NTP 서버와 동기화 요청을 보냈습니다.")
        else:
            messagebox.showerror("오류", "시리얼 포트가 연결되지 않았습니다.")

    def _choose_color(self):
        """색상 선택 다이얼로그."""
        color_code = colorchooser.askcolor(title="색상 선택")[1]
        if color_code:
            self.status_label.config(text=f"색상 선택됨: {color_code}")
            if self.serial_connection:
                rgb = tuple(int(color_code[i : i + 2], 16) for i in (1, 3, 5))
                command = json.dumps({"function": "preset_edit", "presetType": "custom", "presetCurd": "add", "Preset_RGB": rgb})
                self.serial_connection.write(command.encode())
                messagebox.showinfo("색상 선택", f"선택한 색상: {color_code}")
            else:
                messagebox.showerror("오류", "시리얼 포트가 연결되지 않았습니다.")

    def _connect_port(self):
        """시리얼 포트 연결 이벤트 핸들러."""
        port = self.port_combobox.get()
        if port == "포트를 선택하세요":
            messagebox.showerror("오류", "포트를 선택하세요.")
        else:
            try:
                self.serial_connection = serial.Serial(port, 4800, timeout=1)
                self.status_label.config(text=f"{port} (4800bps) 연결됨")
                messagebox.showinfo("연결", f"{port}에 연결되었습니다.")
            except serial.SerialException as e:
                messagebox.showerror("오류", f"포트 연결 실패: {e}")

    def _change_speed(self):
        """시리얼 속도 변경."""
        if self.serial_connection:
            baudrate = self.baudrate_combobox.get()
            command = json.dumps({"function": "changeSpeedSerial", "speed": int(baudrate)})
            self.serial_connection.write(command.encode())
            self.status_label.config(text=f"속도 변경됨: {baudrate}bps")
            messagebox.showinfo("속도 변경", f"시리얼 속도가 {baudrate}bps로 변경되었습니다.")
        else:
            messagebox.showerror("오류", "시리얼 포트가 연결되지 않았습니다.")

    def _add_preset(self):
        """프리셋 추가 이벤트."""
        if self.serial_connection:
            command = json.dumps({"function": "preset_edit", "presetType": "custom", "presetCurd": "add"})
            self.serial_connection.write(command.encode())
            messagebox.showinfo("프리셋 추가", "프리셋이 추가되었습니다.")
        else:
            messagebox.showerror("오류", "시리얼 포트가 연결되지 않았습니다.")

    def _reset_preset(self):
        """프리셋 리셋 이벤트."""
        if self.serial_connection:
            command = json.dumps({"function": "preset_edit", "presetType": "custom", "presetCurd": "reset"})
            self.serial_connection.write(command.encode())
            messagebox.showinfo("프리셋 리셋", "프리셋이 초기화되었습니다.")
        else:
            messagebox.showerror("오류", "시리얼 포트가 연결되지 않았습니다.")

# 단독 실행 시 GUI 테스트
def main():
    root = tk.Tk()
    style = ttk.Style()
    style.configure("Accent.TButton", foreground="white", background="#0078D7", font=("Arial", 10))
    app = KoreanClockGUI(root)
    root.mainloop()

if __name__ == "__main__":
    main()
