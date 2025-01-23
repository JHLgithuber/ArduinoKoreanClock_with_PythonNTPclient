import tkinter as tk
from ttkthemes import ThemedTk
from tkinter import ttk, messagebox, colorchooser
import serial
import json
from tkcalendar import Calendar
import datetime

default_background= "#D9D9D9"

class KoreanClockGUI:
    def __init__(self):
        self.root = tk.Tk()
        self.root.title("한글 시계 조정 프로그램")
        self.root.geometry("800x800")
        self.root.resizable(True, True)
        self.root.configure(bg=default_background)

        self.serial_connection = None  # 시리얼 연결 객체

        self.style = ttk.Style()
        self.style.theme_use("alt")
        self.style.configure("Accent.TButton", foreground="white", background="lightgray", font=("Arial", 10))
        self.style.configure("Critical.TButton", foreground="white", font=("Arial", 10))
        self.style.map("Critical.TButton", background=[("active", "red"), ("!disabled", "maroon")])
        self.style.configure("TCheckbutton", foreground="black", background="lightgray", font=("Arial", 10))

        self._create_widgets()
        self.root.mainloop()

    def _create_widgets(self):
        """GUI 요소를 생성합니다."""
        # 메뉴바 추가
        self.menubar = tk.Menu(self.root, background=default_background)
        self.root.config(menu=self.menubar)

        file_menu = tk.Menu(self.menubar, tearoff=0)
        file_menu.add_command(label="새로 만들기", command=lambda: print("새로 만들기 선택"))
        file_menu.add_command(label="열기", command=lambda: print("열기 선택"))
        file_menu.add_separator()
        file_menu.add_command(label="종료", command=self.root.quit)

        edit_menu = tk.Menu(self.menubar, tearoff=0)
        edit_menu.add_command(label="복사", command=lambda: print("복사 선택"))
        edit_menu.add_command(label="붙여넣기", command=lambda: print("붙여넣기 선택"))

        self.menubar.add_cascade(label="파일", menu=file_menu)
        self.menubar.add_cascade(label="편집", menu=edit_menu)

        # 그리드 레이아웃 설정
        self.root.columnconfigure(0, weight=1)
        self.root.columnconfigure(1, weight=1)
        self.root.rowconfigure(0, weight=1)
        self.root.rowconfigure(1, weight=1)

        # 좌상단: 현재 시간
        self.current_time_frame = ttk.LabelFrame(self.root, text="현재 시간", padding=(10, 10))
        self.current_time_frame.grid(row=0, column=0, sticky="nsew", padx=10, pady=10)

        self.current_time_label = ttk.Label(self.current_time_frame, text="2025-01-13 15:30:00", font=("Arial", 15))
        self.current_time_label.pack(side="top", pady=5)

        self.current_text_text = tk.Text(self.current_time_frame, font=("Arial", 15), bg=default_background, height=5, width=10, wrap=tk.WORD, bd=0, fg="white", highlightthickness=0)
        self.current_text_text.tag_configure("center", justify="center")
        self.current_text_text.pack(side="top", expand=True, pady=5)

        self.sync_time_display()

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
        self.ntp_sync_button.pack(pady=10, ipadx=10, ipady=5, fill="x")

        self.ntp_sync_button = ttk.Button(self.time_frame, text="NTP 동기화", command=self._open_list_ntp)
        self.ntp_sync_button.pack(pady=10, ipadx=10, ipady=5, fill="x")

        self.ntp_sync_button = ttk.Button(self.time_frame, text="PC 동기화", command=self._sync_ntp)
        self.ntp_sync_button.pack(pady=10, ipadx=10, ipady=5, fill="x")

        self.manual_time_button = ttk.Button(self.time_frame, text="수동 시간 설정", command=self._open_manual_time_window)
        self.manual_time_button.pack(pady=10, ipadx=10, ipady=5, fill="x")

        autosync_checkbox_val=tk.BooleanVar()
        self.autosync_checkbox = ttk.Checkbutton(
            self.time_frame,
            text="주기적 자동 동기화",
            style="TCheckbutton",
            variable=autosync_checkbox_val,
            command=lambda: self._run_autosync_thread(autosync_checkbox_val)
        )
        self.autosync_checkbox.pack(side="bottom", ipadx=10, ipady=3, padx=20, pady=7, fill="x")

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

        self.add_time_preset_button = ttk.Button(self.inner_time_preset_frame, text="프리셋 추가", command=lambda: self._open_add_preset_window(apply_position=False))
        self.add_time_preset_button.grid(row=0, column = 0, rowspan = 2, sticky="NSEW", padx=3, pady=3, ipadx=5, ipady=5)

        self.edit_time_preset_button = ttk.Button(self.inner_time_preset_frame, text="프리셋 편집", command=lambda: self._open_list_preset_window(apply_position=False))
        self.edit_time_preset_button.grid(row=0, column=1, sticky="EW", padx=3, pady=3, ipadx=5, ipady=5)

        self.reset_time_preset_button = ttk.Button(self.inner_time_preset_frame, text="프리셋 리셋", command=self._reset_preset, style="Critical.TButton")
        self.reset_time_preset_button.grid(row=0, column = 2, sticky="EW", padx=3, pady=3, ipadx=5, ipady=5)

        self.edit_time_preset_button = ttk.Button(self.inner_time_preset_frame, text="PC에 저장")
        self.edit_time_preset_button.grid(row=1, column=1, sticky="EW", padx=3, pady=3, ipadx=5, ipady=5)

        self.edit_time_preset_button = ttk.Button(self.inner_time_preset_frame, text="불러오기")
        self.edit_time_preset_button.grid(row=1, column=2, sticky="EW", padx=3, pady=3, ipadx=5, ipady=5)



        self.cust_preset_frame = ttk.LabelFrame(self.color_frame, text="사용자 설정 프리셋 설정", padding=(10, 10))
        self.cust_preset_frame.pack(fill="x", padx=5, pady=5, expand=True)

        self.inner_cust_preset_frame = ttk.Frame(self.cust_preset_frame)
        self.inner_cust_preset_frame.pack()

        self.add_cust_preset_button = ttk.Button(self.inner_cust_preset_frame, text="프리셋 추가", command=self._open_add_preset_window)
        self.add_cust_preset_button.grid(row=0, column = 0, rowspan = 2, sticky="NSEW", padx=3, pady=3, ipadx=5, ipady=5)

        self.edit_cust_preset_button = ttk.Button(self.inner_cust_preset_frame, text="프리셋 편집", command=self._open_list_preset_window)
        self.edit_cust_preset_button.grid(row=0, column=1, sticky="EW", padx=3, pady=3, ipadx=5, ipady=5)

        self.reset_cust_preset_button = ttk.Button(self.inner_cust_preset_frame, text="프리셋 리셋", command=self._reset_preset, style="Critical.TButton")
        self.reset_cust_preset_button.grid(row=0, column = 2, sticky="EW", padx=3, pady=3, ipadx=5, ipady=5)

        self.edit_cust_preset_button = ttk.Button(self.inner_cust_preset_frame, text="PC에 저장")
        self.edit_cust_preset_button.grid(row=1, column=1, sticky="EW", padx=3, pady=3, ipadx=5, ipady=5)

        self.edit_cust_preset_button = ttk.Button(self.inner_cust_preset_frame, text="불러오기")
        self.edit_cust_preset_button.grid(row=1, column=2, sticky="EW", padx=3, pady=3, ipadx=5, ipady=5)

        # 상태 출력
        self.status_label = tk.Label(
            self.root, text="상태: 대기 중", font=("Arial", 10), fg="#555", bg=default_background)
        self.status_label.grid(row=2, column=0, columnspan=2, pady=10)

    def sync_time_display(self):
        def get_color(position):
            #TODO:아두이노에서 색상얻는 로직, main코드로 옮겨야됨
            return "white"

        self.current_text_text.tag_configure("AM", foreground=get_color("AM"), justify="center")
        self.current_text_text.tag_configure("PM", foreground=get_color("PM"), justify="center")
        self.current_text_text.tag_configure("MID", foreground=get_color("MID"), justify="center")
        self.current_text_text.tag_configure("NOON", foreground=get_color("NOON"), justify="center")
        self.current_text_text.tag_configure("H", foreground=get_color("H"), justify="center")
        self.current_text_text.tag_configure("Hsuf", foreground=get_color("Hsuf"), justify="center")
        self.current_text_text.tag_configure("M", foreground=get_color("M"), justify="center")
        self.current_text_text.tag_configure("Msuf", foreground=get_color("Msuf"), justify="center")

        #TODO: 시간을 한글로 변환하는 로직
        self.current_text_text.insert("end", "Red\n", "AM")
        self.current_text_text.insert("end", "Blue\n", "PM")
        self.current_text_text.insert("end", "Green\n", "MID")



    def _open_add_preset_window(self, apply_position=True, title="프리셋 추가"):
        SUN_val = tk.BooleanVar()
        MON_val = tk.BooleanVar()
        TUE_val = tk.BooleanVar()
        WED_val = tk.BooleanVar()
        THU_val = tk.BooleanVar()
        FRI_val = tk.BooleanVar()
        SAT_val = tk.BooleanVar()

        AM_val = tk.BooleanVar()
        PM_val = tk.BooleanVar()
        MID_val = tk.BooleanVar()
        NOON_val = tk.BooleanVar()
        H_val = tk.BooleanVar()
        Hsuf_val = tk.BooleanVar()
        M_val = tk.BooleanVar()
        Msuf_val = tk.BooleanVar()

        days_list = [
            ["일요일", SUN_val],
            ["월요일", MON_val],
            ["화요일", TUE_val],
            ["수요일", WED_val],
            ["목요일", THU_val],
            ["금요일", FRI_val],
            ["토요일", SAT_val],
        ]

        position_list = [
            ["오전", AM_val],
            ["오후", PM_val],
            ["자정", MID_val],
            ["정오", NOON_val],
            ["시", H_val],
            ["시_접미사", Hsuf_val],
            ["분", M_val],
            ["분_접미사", Msuf_val],
        ]

        for _, day_val in days_list:
            day_val = tk.BooleanVar()
            day_val.set(True)

        for _, position_val in position_list:
            position_val = tk.BooleanVar()
            position_val.set(True)


        """색상 프리셋 추가 창을 엽니다."""
        add_preset_window= tk.Toplevel(self.root)
        add_preset_window.title(title)
        add_preset_window.geometry("530x250" if apply_position else "530x200")
        add_preset_window.resizable(False, False)
        add_preset_window.configure(bg=default_background)

        # Frame for input fields
        add_preset_time_frame = ttk.LabelFrame(add_preset_window, text="적용 일시")
        add_preset_time_frame.pack(fill="x", padx=5, pady=3, side="top")
        inner_add_preset_time_frame = ttk.Frame(add_preset_time_frame)
        inner_add_preset_time_frame.pack()

        # Start Time Comboboxes
        start_hour = ttk.Combobox(inner_add_preset_time_frame, values=[f"{i:02}" for i in range(24)], width=3,
                                  justify="center", state="readonly")
        start_hour.set("00")
        start_hour.grid(row=0, column=1, padx=2)

        colon1 = ttk.Label(inner_add_preset_time_frame, text=":")
        colon1.grid(row=0, column=2)

        start_minute = ttk.Combobox(inner_add_preset_time_frame, values=[f"{i:02}" for i in range(0, 60, 10)], width=3,
                                    justify="center", state="readonly")
        start_minute.set("00")
        start_minute.grid(row=0, column=3, padx=2)

        label_from = ttk.Label(inner_add_preset_time_frame, text="~")
        label_from.grid(row=0, column=4, padx=5)

        # End Time Comboboxes
        end_hour = ttk.Combobox(inner_add_preset_time_frame, values=[f"{i:02}" for i in range(24)], width=3, justify="center",
                                state="readonly")
        end_hour.set("00")
        end_hour.grid(row=0, column=5, padx=2)

        colon2 = ttk.Label(inner_add_preset_time_frame, text=":")
        colon2.grid(row=0, column=6)

        end_minute = ttk.Combobox(inner_add_preset_time_frame, values=[f"{i:02}" for i in range(0, 60, 10)], width=3,
                                  justify="center", state="readonly")
        end_minute.set("00")
        end_minute.grid(row=0, column=7, padx=2)

        inner_add_preset_day_frame = ttk.Frame(add_preset_time_frame)
        inner_add_preset_day_frame.pack()



        for i, (text, variable) in enumerate(days_list):
            checkbox = ttk.Checkbutton(
                inner_add_preset_day_frame,
                text = text,
                style = "TCheckbutton",
                variable = variable,
                onvalue = True,
                offvalue = False,
            )
            checkbox.grid(row=0, column=i, padx=3, pady=5)


        if apply_position:
            add_preset_ledID_frame = ttk.LabelFrame(add_preset_window, text="적용 위치")
            add_preset_ledID_frame.pack(fill="x", padx=5, pady=3, side="top")
            inner_preset_ledID_frame = ttk.Frame(add_preset_ledID_frame)
            inner_preset_ledID_frame.pack()

            for i, (text, variable) in enumerate(position_list):
                checkbox = ttk.Checkbutton(
                    inner_preset_ledID_frame,
                    text=text,
                    style="TCheckbutton",
                    variable=variable,
                    onvalue=True,
                    offvalue=False,
                )
                checkbox.grid(row=0, column=i, padx=3, pady=5)

        add_preset_color_frame = ttk.Frame(add_preset_window)
        add_preset_color_frame.pack(side="top")

        color_preview_canvas = tk.Canvas(add_preset_color_frame, width=30, height=30, bg="white")
        color_preview_canvas.grid(row=0, column=0, padx=5, pady=10)

        choose_color_button = ttk.Button(add_preset_color_frame, text="색상 선택", command=self._choose_color)
        choose_color_button.grid(row=0, column=1, pady=5, ipadx=10, ipady=5)

        add_preset_button_frame = ttk.Frame(add_preset_window)
        add_preset_button_frame.pack(side="bottom", padx=5, pady=5)

        submit_button = ttk.Button(add_preset_button_frame, text="취소", command=add_preset_window.destroy)
        submit_button.pack(side="left", ipadx=15, ipady=5, padx=5, pady=5)

        submit_button = ttk.Button(add_preset_button_frame, text="저장")
        submit_button.pack(side="left", ipadx=15, ipady=5, padx=5, pady=5)

    def _open_list_preset_window(self, apply_position=True, title="프리셋 편집"):
        self.list_preset_window = tk.Toplevel(self.root)
        self.list_preset_window.title(title)
        self.list_preset_window.geometry("800x300" if apply_position else "500x300")
        self.list_preset_window.resizable(True, True)
        self.list_preset_window.configure(bg=default_background)

        # 기본 컬럼 정의
        default_columns = ("priority", "color", "start_time", "end_time", "dayOfWeek")
        additional_columns = ("AM", "PM", "MID", "NOON", "H", "Hsuf", "M", "Msuf") if apply_position else ()
        all_columns = default_columns + additional_columns

        # Treeview 생성
        table = ttk.Treeview(self.list_preset_window, columns=all_columns, show="headings", selectmode="extended")
        xscrollbar = ttk.Scrollbar(self.list_preset_window, orient=tk.HORIZONTAL, command=table.xview)
        yscrollbar = ttk.Scrollbar(self.list_preset_window, orient=tk.VERTICAL, command=table.yview)
        table.configure(xscrollcommand=xscrollbar.set, yscrollcommand=yscrollbar.set)  # 가로 스크롤 설정

        # 헤더 텍스트 설정
        position_width=40
        column_headers_dict = {
            "priority": ("우선순위",60),
            "color": ("색상",40),
            "start_time": ("시작 시간",80),
            "end_time": ("종료 시간",80),
            "dayOfWeek": ("요일",100),
            "AM": ("오전",position_width),
            "PM": ("오후",position_width),
            "MID": ("자정",position_width),
            "NOON": ("정오",position_width),
            "H": ("시",position_width),
            "Hsuf": ("시_접미사",position_width),
            "M": ("분",position_width),
            "Msuf": ("분_접미사",position_width),
        }

        # 컬럼 및 헤더 설정
        for column in all_columns:
            (column_text,column_width)=column_headers_dict[column]
            table.heading(column, text=column_text)  # 헤더 텍스트 설정
            table.column(column, anchor="center", width=column_width)  # 컬럼 속성 설정

        # Treeview 및 스크롤바 배치
        button_frame = ttk.Frame(self.list_preset_window)
        edit_button = ttk.Button(button_frame, text="순위올림")
        edit_button.pack(side="left", ipadx=2, ipady=3)
        edit_button = ttk.Button(button_frame, text="순위내림")
        edit_button.pack(side="left", ipadx=2, ipady=3)
        edit_button = ttk.Button(button_frame, text="복사")
        edit_button.pack(side="right", padx=3, ipadx=5, ipady=5)
        edit_button = ttk.Button(button_frame, text="편집")
        edit_button.pack(side="right", padx=3, ipadx=5, ipady=5)
        delete_button = ttk.Button(button_frame,text="삭제", style="Critical.TButton")
        delete_button.pack(side="right", padx=3, ipadx=5, ipady=5)
        button_frame.pack(side="bottom", padx=5, pady=5, fill="x")

        xscrollbar.pack(side="bottom", fill="x")
        yscrollbar.pack(side="right", fill="y")
        table.pack(side="top", expand=True, fill="both")

        dummy_data = [
            {"priority": 1, "color": "Red", "start_time": "08:00", "end_time": "12:00", "dayOfWeek": "Monday",
             "AM": "Yes", "PM": "No"},
            {"priority": 2, "color": "Blue", "start_time": "12:00", "end_time": "16:00", "dayOfWeek": "Tuesday",
             "AM": "No", "PM": "Yes"},
            {"priority": 3, "color": "Green", "start_time": "16:00", "end_time": "20:00", "dayOfWeek": "Wednesday",
             "AM": "Yes", "PM": "Yes"}
        ]

        for row in dummy_data:
            values = [row.get(column, "") for column in all_columns]
            table.insert("", "end", values=values)


    def _open_list_ntp(self):
        self.list_ntp_window = tk.Toplevel(self.root)
        self.list_ntp_window.title("NTP 서버 선택")
        self.list_ntp_window.geometry("500x300")
        self.list_ntp_window.resizable(True, True)
        self.list_ntp_window.configure(bg=default_background)

        # 기본 컬럼 정의
        default_columns = ("index", "name", "address")

        # Treeview 생성
        table = ttk.Treeview(self.list_ntp_window, columns=default_columns, show="headings")
        xscrollbar = ttk.Scrollbar(self.list_ntp_window, orient=tk.HORIZONTAL, command=table.xview)
        yscrollbar = ttk.Scrollbar(self.list_ntp_window, orient=tk.VERTICAL, command=table.yview)
        table.configure(xscrollcommand=xscrollbar.set, yscrollcommand=yscrollbar.set)  # 가로 스크롤 설정

        # 헤더 텍스트 설정
        position_width = 40
        column_headers_dict = {
            "index": ("순번", 10),
            "name": ("이름", 40),
            "address": ("주소", 200),
        }

        # 컬럼 및 헤더 설정
        for column in default_columns:
            (column_text, column_width) = column_headers_dict[column]
            table.heading(column, text=column_text)  # 헤더 텍스트 설정
            table.column(column, anchor="center", width=column_width)  # 컬럼 속성 설정

        # Treeview 및 스크롤바 배치
        xscrollbar.pack(side="bottom", fill="x")
        yscrollbar.pack(side="right", fill="y")
        table.pack(side="top", expand=True, fill="both")

    def _open_manual_time_window(self):
        """수동 시간 설정 창을 엽니다."""
        self.manual_window = tk.Toplevel(self.root)
        self.manual_window.title("수동 시간 설정")
        self.manual_window.geometry("250x350")
        self.manual_window.resizable(False, False)
        self.manual_window.configure(bg=default_background)

        # 캘린더 위젯
        self.calendar = Calendar(self.manual_window, selectmode='day', date_pattern='yyyy-mm-dd')
        self.calendar.grid(row=0, column=0, columnspan=3, pady=10, padx=10)

        # 시간 입력 위젯
        self.hour_label = ttk.Label(self.manual_window, text="시:")
        self.hour_label.grid(row=1, column=0, padx=5, pady=5)
        self.hour_entry = ttk.Entry(self.manual_window, width=5, font=("Arial", 10), justify="center")
        self.hour_entry.insert(0, "12")
        self.hour_entry.grid(row=1, column=1, padx=5, pady=5)

        self.minute_label = ttk.Label(self.manual_window, text="분:")
        self.minute_label.grid(row=2, column=0, padx=5, pady=5)
        self.minute_entry = ttk.Entry(self.manual_window, width=5, font=("Arial", 10), justify="center")
        self.minute_entry.insert(0, "00")
        self.minute_entry.grid(row=2, column=1, padx=5, pady=5)

        self.second_label = ttk.Label(self.manual_window, text="초:")
        self. second_label.grid(row=3, column=0, padx=5, pady=5)
        self.second_entry = ttk.Entry(self.manual_window, width=5, font=("Arial", 10), justify="center")
        self.second_entry.insert(0, "00")
        self.second_entry.grid(row=3, column=1, padx=5, pady=5)

        def set_manual_time():
            self.selected_date = self.calendar.get_date()
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

    def _run_autosync_thread(self, autosync_checkbox_val):
        pass


# 단독 실행 시 GUI 테스트
def main():
    app = KoreanClockGUI()


if __name__ == "__main__":
    main()
