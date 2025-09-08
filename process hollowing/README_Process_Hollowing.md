# Process Hollowing Implementation

## Mô tả
Đây là một cài đặt kỹ thuật Process Hollowing bằng C++ cho mục đích nghiên cứu bảo mật và học tập. Kỹ thuật này cho phép chạy một tệp thực thi trong không gian bộ nhớ của một tiến trình hợp pháp khác (như notepad.exe), giúp hiểu rõ hơn về cách thức hoạt động của một số kỹ thuật tránh phát hiện.

## Tính năng
- Tạo một tiến trình ở trạng thái tạm dừng (suspended state)
- Giải phóng không gian bộ nhớ gốc của tiến trình
- Phân tích cấu trúc PE của tệp payload
- Ánh xạ các section của payload vào tiến trình đích
- Thiết lập quyền bảo vệ phù hợp cho từng section
- Cập nhật context của thread để trỏ đến entry point mới
- Hỗ trợ cả kiến trúc 32-bit và 64-bit

## Cách sử dụng
1. Biên dịch mã nguồn process_hollowing.cpp
2. Chạy chương trình với cú pháp:
   ```
   process_hollowing.exe [đường_dẫn_tiến_trình_đích] [đường_dẫn_payload]
   ```
3. Mặc định, nếu không có đối số được cung cấp, chương trình sẽ sử dụng:
   - Tiến trình đích: C:\Windows\System32\notepad.exe
   - Payload: messagebox.exe (trong cùng thư mục với chương trình)

## Ghi chú kỹ thuật
- Chương trình kiểm tra tính hợp lệ của tệp PE trước khi thực hiện
- Có cơ chế ghi log chi tiết để theo dõi lỗi (hollowing_error.log)
- Xử lý các tình huống không mong muốn (section không hợp lệ, lỗi cấp phát bộ nhớ)

## Miễn trừ trách nhiệm

**LƯU Ý QUAN TRỌNG**: Công cụ này được phát triển CHỈ cho mục đích nghiên cứu và học tập. Việc sử dụng cho mục đích trái phép là vi phạm pháp luật. Tác giả không chịu trách nhiệm về bất kỳ hậu quả nào từ việc lạm dụng công cụ này.