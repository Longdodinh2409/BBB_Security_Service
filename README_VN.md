# BBB_Security_Service

## Tóm tắt dự án

Dự án này là một hệ thống bảo mật trên nền BeagleBone Black (BBB), tích hợp nhận dạng vân tay và quản lý danh sách thành viên thông qua cơ sở dữ liệu SQLite. Ứng dụng chạy trên Linux, giao tiếp qua UART với module xử lý vân tay / MCU STM32F4, đồng thời hỗ trợ thao tác quản trị qua giao diện dòng lệnh (CLI).

Mục tiêu chính của hệ thống là:

- Nhận dạng vân tay và tra cứu danh tính thành viên
- Lưu trữ thông tin người dùng trong cơ sở dữ liệu
- Quản lý trạng thái khóa/mở khóa do sai vân tay quá nhiều lần
- Ghi lịch sử sự kiện vào file log
- Cung cấp giao diện quản trị để thêm/xóa thành viên

---

## Kiến trúc hệ thống

Project gồm các thành phần chính sau:

- `main.c`: điểm khởi động chương trình, khởi tạo UART, SQLite, và các thread chính
- `main.h`: định nghĩa hằng số, enum trạng thái FSM, biến toàn cục, prototype hàm
- `uart.c`: cấu hình và quản lý giao tiếp UART `/dev/ttyS1`
- `database.c`: truy vấn SQLite để tìm, thêm, xóa thành viên
- `threads.c`: chứa các thread xử lý thời gian, nhận dạng vân tay và CLI quản trị
- `Makefile`: build cross-compile cho ARM, ngoài ra tự động copy file binary ra máy BBB qua SCP

### Luồng xử lý chính

1. Ứng dụng khởi tạo UART và SQLite.
2. Tạo các thread:
   - thread gửi timestamp qua UART để hiển thị thời gian (Giờ Phút + Ngày Tháng Năm ở màn hình Standby)
   - thread nhận dữ liệu liên quan đến vân tay từ STM32F4
   - thread xử lý lệnh Admin trên CLI
3. Khi STM32F4 gửi trạng thái và ID vân tay, chương trình:
   - tra cứu tên tương ứng trong database
   - gửi lại thông tin về phía module
   - log sự kiện vào file lịch sử
4. Nếu phát hiện vân tay mới, hệ thống chờ lệnh `add <ID> <Tên>` để lưu người dùng mới.
5. Nếu người dùng sai vân tay nhiều lần, hệ thống chuyển sang trạng thái khóa theo 5 phút, 10 phút, hoặc khóa vĩnh viễn tùy mức độ.

---

## Các tính năng chính

### 1. Quản lý vân tay

Hệ thống xử lý nhiều trạng thái FSM của module vân tay, bao gồm:

- `FSM_FINGER_SEND_GENIMG`
- `FSM_FINGER_WAIT_GENIMG`
- `FSM_FINGER_SEND_IMG2TZ`
- `FSM_FINGER_WAIT_IMG2TZ`
- `FSM_FINGER_SEND_SEARCH`
- `FSM_FINGER_WAIT_SEARCH`
- `FSM_FINGER_DELAY`
- `FSM_FINGER_BLOCK_5M`
- `FSM_FINGER_BLOCK_10M`
- `FSM_FINGER_BLOCK_INF`
- `FSM_FINGER_UNBLOCK`
- `FSM_SYSTEM_STM32F4_WAKEUP`
- `FSM_NEW_FINGERPRINT_ADDED`
- `FSM_REMOVE_SPECIFIC_FINGERPRINT`

Các trạng thái này cho phép hệ thống điều phối quy trình thu nhận, so khớp và quản lý trạng thái khóa/mở khóa module vân tay.

### 2. Giao tiếp UART

- Mở cổng `/dev/ttyS1`
- Cấu hình baudrate 115200, 8N1
- Gửi thời gian hệ thống theo định dạng `#TS=<timestamp>;`
- Nhận các frame trạng thái từ module và trả lời theo định dạng `#State=x,#Name=...;`

### 3. Quản lý cơ sở dữ liệu SQLite

- Database mặc định: `memberdb.db`
- Bảng mặc định: `memberlst`
- Chức năng:
  - `get_user_name(id, output_name)`: lấy tên theo ID
  - `add_new_member(id, name)`: thêm thành viên mới
  - `delete_member(id)`: xóa thành viên theo ID

### 4. Hỗ trợ đa luồng

Ứng dụng sử dụng `pthread` để chạy đồng thời các nhiệm vụ:

- `handle_TimeDisplay_thread`: gửi timestamp định kỳ qua UART
- `handle_SearchMember_thread`: đọc dữ liệu UART, parse trạng thái, so khớp vân tay và truy vấn database
- `handle_CLI_thread`: nhận lệnh quản trị từ terminal

### 5. Quản trị qua CLI

Hệ thống hỗ trợ các lệnh quản trị như:

- `add <ID> <Tên>`: thêm thành viên mới sau khi module phát hiện vân tay mới
- `delete <ID>`: xóa thành viên khỏi database
- `exit`: có thể tắt hệ thống nếu không bị comment/unused trong code hiện tại

Ví dụ:

```bash
BBB_Admin> add 2 Nguyen Van A
BBB_Admin> delete 5
```

### 6. Ghi log lịch sử

Mỗi sự kiện quan trọng đều được lưu vào file:

- `Security_history_log.txt`

Các sự kiện có thể gồm:

- người dùng được nhận diện
- người dùng không xác định
- module bị khóa
- hệ thống được mở khóa
- thêm/xóa thành viên

---

## Cấu trúc dữ liệu và hằng số chính

Một số hằng số quan trọng được định nghĩa trong `main.h`:

- `MAX_NAME_LEN = 16`
- `LOG_BUFFER_SIZE = 128`
- `UART_DEVICE = "/dev/ttyS1"`
- `DATABASE_NAME = "memberdb.db"`
- `DATABASE_TABLE_NAME = "memberlst"`
- `NONE_FINGER_PRINT_ID = 99`
- `OFFSET_MEMBER_ID = 1`

Cấu hình này cho thấy project đang làm việc với ID vân tay có độ lệch nhất định so với ID trong database, và có cơ chế gán ID mặc định 99 cho trường hợp chưa có vân tay.

---

## Yêu cầu môi trường

- Hệ điều hành Linux
- Thiết bị BeagleBone Black hoặc nền ARM tương thích
- Cổng UART `/dev/ttyS1`
- Thư viện SQLite3
- GCC cross compile cho ARM (`arm-linux-gnueabihf-gcc`)

---

## Mục tiêu ứng dụng thực tế

Dự án này phù hợp cho các hệ thống kiểm soát ra/vào bằng vân tay như:

- cổng ra vào văn phòng
- kiểm soát cửa tự động
- hệ thống nhân sự / chấm công
- ứng dụng bảo mật tại khu vực có yêu cầu xác thực nhân thân

---

## Tài liệu tham khảo nội bộ

Các file chính cần xem thêm khi phát triển tiếp:

- `main.h` – định nghĩa trạng thái và API chung
- `database.c` – thao tác dữ liệu
- `uart.c` – cấu hình giao tiếp serial
- `threads.c` – luồng xử lý nghiệp vụ chính

---

## Kết luận

BBB_Security_Service là một hệ thống xác thực và quản lý thành viên bằng vân tay chạy trên BeagleBone Black, kết hợp UART, SQLite, đa luồng và CLI. Đây là một nền tảng phù hợp để phát triển tiếp thành một sản phẩm kiểm soát truy cập thực tế với tính năng bảo mật, log lịch sử và quản trị dễ dàng.
