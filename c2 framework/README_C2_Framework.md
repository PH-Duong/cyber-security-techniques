# Hệ thống Command and Control (C2)

## Mô tả
Đây là một framework Command and Control đơn giản được viết bằng C++ cho mục đích nghiên cứu và học tập. Hệ thống bao gồm một máy chủ có khả năng quản lý nhiều máy khách cùng lúc và một ứng dụng khách tương thích với cả Windows và Linux.

## Tính năng
- Máy chủ có khả năng quản lý nhiều máy khách cùng lúc
- Giao tiếp qua REST API với định dạng JSON
- Cơ chế heartbeat để kiểm tra trạng thái kết nối của máy khách
- Hàng đợi lệnh cho phép gửi nhiều lệnh đến các máy khách
- Ứng dụng khách tương thích với cả Windows và Linux
- Thu thập thông tin hệ thống cơ bản (tên máy tính, tên người dùng, địa chỉ IP, hệ điều hành)
- Thực thi lệnh từ xa và gửi kết quả về máy chủ

## Cách sử dụng
1. Biên dịch mã nguồn server.cpp thành tệp thực thi server
2. Biên dịch mã nguồn client.cpp thành tệp thực thi client
3. Chạy server trước, mặc định lắng nghe trên cổng 8888
4. Chạy client trên các máy đích, cung cấp địa chỉ IP của server
5. Sử dụng giao diện dòng lệnh của server để quản lý các máy khách:
   - `list`: Liệt kê tất cả các máy khách đã kết nối
   - `send <client_id> <command>`: Gửi lệnh đến một máy khách cụ thể
   - `help`: Hiển thị danh sách lệnh có sẵn
   - `exit`: Thoát khỏi server

## Miễn trừ trách nhiệm

**LƯU Ý QUAN TRỌNG**: Công cụ này được phát triển CHỈ cho mục đích nghiên cứu và học tập. Việc sử dụng cho mục đích trái phép là vi phạm pháp luật. Tác giả không chịu trách nhiệm về bất kỳ hậu quả nào từ việc lạm dụng công cụ này.