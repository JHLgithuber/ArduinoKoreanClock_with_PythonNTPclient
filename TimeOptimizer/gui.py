import tkinter as tk
from tkinter import ttk, messagebox, colorchooser
import serial
import json
from tkcalendar import Calendar
import datetime

class KoreanClockGUI:
    def __init__(self):
        self.root = tk.Tk()
        self.root.title("한글 시계 조정 프로그램")
        self.root.geometry("800x800")
        self.root.resizable(True, True)
        self.root.configure(bg="#D9D9D9")

        self.serial_connection = None  # 시리얼 연결 객체

        self.style = ttk.Style()
        self.style.theme_use("clam")
        self.style.configure("Accent.TButton", foreground="white", background="#0078D7", font=("Arial", 10))
        self.style.configure("Critical.TButton", foreground="white", font=("Arial", 10))
        self.style.map("Critical.TButton", background=[("active", "red"), ("!disabled", "maroon")])
        self.style.configure("TCheckbutton", foreground="white", background="#0078D7", font=("Arial", 10))

        self._create_widgets()
        self.root.mainloop()

    def _create_widgets(self):
        """GUI 요소를 생성합니다."""
        # 그리드 레이아웃 설정
        self.root.columnconfigure(0, weight=1)
        self.root.columnconfigure(1, weight=1)
        self.root.rowconfigure(0, weight=1)
        self.root.rowconfigure(1, weight=1)

        # 좌상단: 현재 시간
        self.current_time_frame = ttk.LabelFrame(self.root, text="현재 시간", padding=(10, 10))
        self.current_time_frame.grid(row=0, column=0, sticky="nsew", padx=10, pady=10)

        self.current_time_label = ttk.Label(self.current_time_frame, text="2025-01-13 15:30:00", font=("Arial", 12))
        self.current_time_label.pack(padx=5, pady=5)

        # 우상단: 시리얼 포트 설정
        self.port_frame = ttk.LabelFrame(self.root, text="한글시계 연결 설정", padding=(10, 10))
        self.port_frame.grid(row=0, column=1, sticky="nsew", padx=10, pady=10)

        self.port_combobox = ttk.Combobox(
            self.port_frame, values=["COM1", "COM2", "COM3"], state="readonly", font=("Arial", 10)
        )
        self.port_combobox.set("시리얼 포트를 선택하세요")
        self.port_combobox.pack(fill="x", padx=5, pady=5)

        self.connect_button = ttk.Button(self.port_frame, text="연결", command=self._connect_port)
        self.connect_button.pack(pady=5, ipadx=10, ipady=5)

        self.change_speed_frame = ttk.LabelFrame(self.port_frame, text="통신속도 변경", padding=(10, 10))
        self.change_speed_frame.pack(fill="x", padx=5, pady=5)

        self.baudrate_combobox = ttk.Combobox(
            self.change_speed_frame, values=["4800", "9600", "19200", "115200"], state="readonly", font=("Arial", 10)
        )
        self.baudrate_combobox.set("4800")
        self.baudrate_combobox.pack(fill="x", padx=5, pady=5)

        self.change_speed_button = ttk.Button(self.change_speed_frame, text="통신속도 변경", command=self._change_speed)
        self.change_speed_button.pack(pady=5, ipadx=10, ipady=5)

        # 좌하단: 시간 설정
        self.time_frame = ttk.LabelFrame(self.root, text="시간 설정", padding=(10, 10))
        self.time_frame.grid(row=1, column=0, sticky="nsew", padx=10, pady=10)

        self.ntp_sync_button = ttk.Button(self.time_frame, text="자동 시간 동기화", command=self._sync_ntp)
        self.ntp_sync_button.pack(pady=10, ipadx=10, ipady=5)

        self.ntp_sync_button = ttk.Button(self.time_frame, text="NTP 선택 동기화", command=self._sync_ntp)
        self.ntp_sync_button.pack(pady=10, ipadx=10, ipady=5)

        self.manual_time_button = ttk.Button(self.time_frame, text="수동 시간 설정", command=self._open_manual_time_window)
        self.manual_time_button.pack(pady=10, ipadx=10, ipady=5)

        # 우하단: 색상 설정
        self.color_frame = ttk.LabelFrame(self.root, text="색상 설정", padding=(10, 10))
        self.color_frame.grid(row=1, column=1, sticky="nsew", padx=10, pady=10)

        self.default_color_frame = ttk.Frame(self.color_frame)
        self.default_color_frame.pack()

        self.color_preview_canvas = tk.Canvas(self.default_color_frame, width=30, height=30, bg="white")
        self.color_preview_canvas.grid(row=0, column=0, padx=10, pady=10)

        self.choose_color_button = ttk.Button(self.default_color_frame, text="기본 색상 선택", command=self._choose_color)
        self.choose_color_button.grid(row=0, column=1, pady=10, ipadx=10, ipady=5)



        self.time_preset_frame = ttk.LabelFrame(self.color_frame, text="시간별 색상 프리셋 설정", padding=(10, 10), )
        self.time_preset_frame.pack(fill="x", padx=5, pady=5, expand=True)

        self.inner_time_preset_frame = ttk.Frame(self.time_preset_frame)
        self.inner_time_preset_frame.pack()

        self.add_time_preset_button = ttk.Button(self.inner_time_preset_frame, text="프리셋 추가", command=self._open_add_preset_window)
        self.add_time_preset_button.grid(row=0, column = 0, columnspan = 2, sticky="EW", padx=3, pady=5, ipadx=5, ipady=5)

        self.edit_time_preset_button = ttk.Button(self.inner_time_preset_frame, text="프리셋 편집", command=self._add_preset)
        self.edit_time_preset_button.grid(row=1, column=0, sticky="EW", padx=3, pady=5, ipadx=5, ipady=5)

        self.reset_time_preset_button = ttk.Button(self.inner_time_preset_frame, text="프리셋 리셋", command=self._reset_preset, style="Critical.TButton")
        self.reset_time_preset_button.grid(row=1, column = 1, sticky="EW", padx=3, pady=5, ipadx=5, ipady=5)



        self.cust_preset_frame = ttk.LabelFrame(self.color_frame, text="사용자 설정 프리셋 설정", padding=(10, 10))
        self.cust_preset_frame.pack(fill="x", padx=5, pady=5, expand=True)

        self.inner_cust_preset_frame = ttk.Frame(self.cust_preset_frame)
        self.inner_cust_preset_frame.pack()

        self.add_cust_preset_button = ttk.Button(self.inner_cust_preset_frame, text="프리셋 추가", command=self._open_add_preset_window)
        self.add_cust_preset_button.grid(row=0, column = 0, columnspan = 2, sticky="EW", padx=3, pady=5, ipadx=5, ipady=5)

        self.edit_cust_preset_button = ttk.Button(self.inner_cust_preset_frame, text="프리셋 편집", command=self._add_preset)
        self.edit_cust_preset_button.grid(row=1, column=0, sticky="EW", padx=3, pady=5, ipadx=5, ipady=5)

        self.reset_cust_preset_button = ttk.Button(self.inner_cust_preset_frame, text="프리셋 리셋", command=self._reset_preset, style="Critical.TButton")
        self.reset_cust_preset_button.grid(row=1, column = 1, sticky="EW", padx=3, pady=5, ipadx=5, ipady=5)

        # 상태 출력
        self.status_label = tk.Label(
            self.root, text="상태: 대기 중", font=("Arial", 10), fg="#555"
        )
        self.status_label.grid(row=2, column=0, columnspan=2, pady=10)

    def _open_add_preset_window(self):
        """색상 프리셋 추가 창을 엽니다."""
        self.add_preset_window= tk.Toplevel(self.root)
        self.add_preset_window.title("프리셋 추가")
        self.add_preset_window.geometry("600x350")
        self.add_preset_window.resizable(False, False)
        self.add_preset_window.configure(bg="#D9D9D9")

        self.add_preset_ledID_frame = ttk.LabelFrame(self.add_preset_window, text="적용 위치")
        self.add_preset_ledID_frame.pack(fill="x", padx=5, pady=5, expand=True)
        self.AM_checkbox=ttk.Checkbutton(self.add_preset_ledID_frame, text="오전", style="TCheckbutton")
        self.AM_checkbox.pack(side="left")
        self.PM_checkbox = ttk.Checkbutton(self.add_preset_ledID_frame, text="오후", style="TCheckbutton")
        self.PM_checkbox.pack(side="left")
        self.MID_checkbox = ttk.Checkbutton(self.add_preset_ledID_frame, text="자정", style="TCheckbutton")
        self.MID_checkbox.pack(side="left")
        self.NOON_checkbox = ttk.Checkbutton(self.add_preset_ledID_frame, text="정오", style="TCheckbutton")
        self.NOON_checkbox.pack(side="left")
        self.H_checkbox = ttk.Checkbutton(self.add_preset_ledID_frame, text="시", style="TCheckbutton")
        self.H_checkbox.pack(side="left")
        self.Hsuf_checkbox = ttk.Checkbutton(self.add_preset_ledID_frame, text="시_접미사", style="TCheckbutton")
        self.Hsuf_checkbox.pack(side="left")
        self.M_checkbox = ttk.Checkbutton(self.add_preset_ledID_frame, text="분", style="TCheckbutton")
        self.M_checkbox.pack(side="left")
        self.Msuf_checkbox = ttk.Checkbutton(self.add_preset_ledID_frame, text="분_접미사", style="TCheckbutton")
        self.Msuf_checkbox.pack(side="left")

        self.add_preset_time_frame = ttk.Frame(self.add_preset_window)
        self.add_preset_time_frame.pack()






    def _open_manual_time_window(self):
        """수동 시간 설정 창을 엽니다."""
        self.manual_window = tk.Toplevel(self.root)
        self.manual_window.title("수동 시간 설정")
        self.manual_window.geometry("250x350")
        self.manual_window.resizable(False, False)
        self.manual_window.configure(bg="#D9D9D9")

        # 캘린더 위젯
        self.calendar = Calendar(self.manual_window, selectmode='day', date_pattern='yyyy-mm-dd')
        self.calendar.grid(row=0, column=0, columnspan=3, pady=10, padx=10)

        # 시간 입력 위젯
        self.hour_label = ttk.Label(self.manual_window, text="시:")
        self.hour_label.grid(row=1, column=0, padx=5, pady=5)
        self.hour_entry = ttk.Entry(self.manual_window, width=5, font=("Arial", 10))
        self.hour_entry.insert(0, "12")
        self.hour_entry.grid(row=1, column=1, padx=5, pady=5)

        self.minute_label = ttk.Label(self.manual_window, text="분:")
        self.minute_label.grid(row=2, column=0, padx=5, pady=5)
        self.minute_entry = ttk.Entry(self.manual_window, width=5, font=("Arial", 10))
        self.minute_entry.insert(0, "00")
        self.minute_entry.grid(row=2, column=1, padx=5, pady=5)

        self.second_label = ttk.Label(self.manual_window, text="초:")
        self. second_label.grid(row=3, column=0, padx=5, pady=5)
        self.second_entry = ttk.Entry(self.manual_window, width=5, font=("Arial", 10))
        self.second_entry.insert(0, "00")
        self.second_entry.grid(row=3, column=1, padx=5, pady=5)

        def set_manual_time():
            self.selected_date = calendar.get_date()
            self.hour = self.hour_entry.get()
            self.minute = self.minute_entry.get()
            self.second = self.second_entry.get()
            self.time_value = f"{self.selected_date} {self.hour}:{self.minute}:{self.second}"
            if self.serial_connection:
                command = json.dumps({"function": "adjust_time", "datetime": self.time_value})
                self.serial_connection.write(command.encode())
                messagebox.showinfo("시간 설정", f"시간이 {self.time_value}로 설정되었습니다.")
            else:
                messagebox.showerror("오류", "시리얼 포트가 연결되지 않았습니다.")

        set_time_button = ttk.Button(self.manual_window, text="시간 설정", command=set_manual_time)
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
        self.color_code = colorchooser.askcolor(title="색상 선택")[1]
        if self.color_code:
            self.status_label.config(text=f"색상 선택됨: {self.color_code}")
            if self.serial_connection:
                rgb = tuple(int(self.color_code[i : i + 2], 16) for i in (1, 3, 5))
                command = json.dumps({"function": "preset_edit", "presetType": "custom", "presetCurd": "add", "Preset_RGB": rgb})
                self.serial_connection.write(command.encode())
                messagebox.showinfo("색상 선택", f"선택한 색상: {self.color_code}")
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
        """시리얼 통신속도 변경."""
        if self.serial_connection:
            baudrate = self.baudrate_combobox.get()
            command = json.dumps({"function": "changeSpeedSerial", "speed": int(baudrate)})
            self.serial_connection.write(command.encode())
            self.status_label.config(text=f"통신속도 변경됨: {baudrate}bps")
            messagebox.showinfo("통신속도 변경", f"시리얼 통신속도가 {baudrate}bps로 변경되었습니다.")
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
    app = KoreanClockGUI()


if __name__ == "__main__":
    main()
