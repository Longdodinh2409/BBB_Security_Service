# Kế hoạch cập nhật luồng thêm vân tay mới - Phía BeagleBone Black (BBB)

Dựa trên kiến trúc giao tiếp dựa trên trạng thái (State-based) mới của STM32F4, dưới đây là các công việc cần cập nhật trên BBB để đồng bộ hoàn toàn với STM32F4.

## 1. Cập nhật Parser nhận lệnh qua UART1
Trình phân tích (parser) trên luồng `handle_SearchMember_thread()` cần được cập nhật để bắt các frame dạng `State-based` thay vì `Cmd`.

*   **Sự kiện cần bắt:** Frame `#State=<FSM_ENROLL_REQUEST_ID>`.
*   **Hành động:** Khi nhận được chuỗi State này (không kèm ID), BBB hiểu rằng STM32F4 đang yêu cầu cấp một `member ID` mới cho quá trình enroll.

## 2. Truy vấn Database tìm ID trống (Lấp chỗ trống)
*   Tạo hàm `get_lowest_available_id()` thao tác với SQLite trên file `memberdb.db`.
*   **Logic SQL:** Truy vấn tìm ID nhỏ nhất bị khuyết trong cột `ID` của bảng `memberlst`.
    ```sql
    SELECT MIN(t1.ID + 1) AS NextID
    FROM memberlst t1
    LEFT JOIN memberlst t2 ON t1.ID + 1 = t2.ID
    WHERE t2.ID IS NULL;
    ```
    *Lưu ý:* Nếu bảng `memberlst` đang trống, hàm mặc định trả về ID = 1. Giá trị tìm được ở DB gọi là `DB_ID`.

## 3. Tính toán Offset và Phản hồi cấp ID
Theo kiến trúc hiện tại, ID giao tiếp qua UART luôn nhỏ hơn ID thực tế trong Database một đơn vị.

*   **Tính UART_ID:** `UART_ID = DB_ID - OFFSET_MEMBER_ID` (với `OFFSET_MEMBER_ID = 1`).
*   **Tạm giữ ID:** Lưu biến trạng thái `s_pending_add_id = UART_ID` để BBB biết ID này đang trong quá trình chờ (pending).
*   **Gửi phản hồi:** Định dạng chuỗi phản hồi **bắt buộc phải tuân theo format parser mới của STM32F4**:
    ```text
    #State=<FSM_ENROLL_REQUEST_ID>,#ID=<UART_ID>
    ```
    *Ghi chú:* Việc dùng lại chính State `FSM_ENROLL_REQUEST_ID` kèm theo `#ID` sẽ giúp STM32F4 lấy được ID đang available.

## 4. Xử lý giải phóng ID khi STM32F4 báo lỗi (Rollback)
STM32F4 đã áp dụng cơ chế timeout và xử lý lỗi chặt chẽ. Nếu có lỗi, nó sẽ gửi frame báo lỗi về BBB.

*   **Sự kiện lỗi:** Nhận frame `#State=<FSM_ENROLL_ID_ERROR>` (hoặc các mã lỗi khác nếu có trong FSM).
*   **Hành động Rollback:**
    *   Xóa biến tạm: `s_pending_add_id = -1;`
    *   Điều này giúp hệ thống "nhả" ID vừa cấp ra, đảm bảo lần bấm nút tiếp theo STM32F4 sẽ vẫn nhận được chính ID trống đó thay vì bị nhảy số.

## 5. Xác nhận lưu vân tay thành công
Bước này xử lý khi cảm biến vân tay đã chạy qua các bước `STORE_MODEL` và báo thành công.

*   **Sự kiện thành công:** STM32F4 gửi frame `#State=<FSM_NEW_FINGERPRINT_ADDED>,#ID=<UART_ID>`.
*   **Hành động của BBB:**
    1. So sánh `<UART_ID>` nhận được có khớp với `s_pending_add_id` đang giữ hay không.
    2. Nếu khớp, in ra console yêu cầu admin hoàn thiện thông tin: 
       `Hay go lenh: add <UART_ID + 1> <Ten_thanh_vien> de luu Member vao Database!`
    3. Khi Admin gõ lệnh hợp lệ từ `handle_CLI_thread()`, gọi `add_new_member()` để `INSERT` vào SQLite (có sử dụng `g_db_mutex` để an toàn).
    4. Ghi log sự kiện vào `Security_history_log.txt`.
    5. Reset `s_pending_add_id = -1`.