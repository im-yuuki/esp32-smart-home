# Checklist phát triển Smart Home ESP32-S3

> Cập nhật ngày 27/07/2026 dựa trên mã nguồn và tài liệu trong repository.
> `[x]` nghĩa là chức năng đã có trong mã nguồn; các kiểm thử cần phần cứng thật được tách riêng bên dưới.

## Đã hoàn thành - Giai đoạn 1 (MVP)

### Firmware ESP32-S3

- [x] Khởi tạo node theo thứ tự an toàn: NVS, relay, nút bấm, cảm biến, Wi-Fi và MQTT.
- [x] Điều khiển hai relay bằng hàng đợi single-writer, hỗ trợ `set`/`toggle` và lưu trạng thái vào NVS.
- [x] Khôi phục trạng thái relay sau khi mất điện trước khi kết nối mạng.
- [x] Điều khiển relay bằng nút vật lý khi Wi-Fi/MQTT không hoạt động.
- [x] Kết nối Wi-Fi với cơ chế retry/backoff, hỗ trợ mạng chính và mạng dự phòng.
- [x] Hỗ trợ DHCP và cấu hình IP tĩnh có kiểm tra subnet, gateway và địa chỉ broadcast.
- [x] Kết nối MQTT, LWT `offline`, retained state, discovery, lệnh relay và lệnh reboot.
- [x] Đồng bộ lại discovery và trạng thái relay sau khi MQTT kết nối lại.
- [x] Tích hợp driver SHT31 qua I2C, kiểm tra CRC và gửi nhiệt độ/độ ẩm định kỳ khi có cảm biến.
- [x] Cho phép node hoạt động bình thường và không quảng bá capability cảm biến khi không có SHT31.
- [x] Cung cấp recovery portal qua SoftAP/captive portal khi chưa cấu hình, mất Wi-Fi lâu hoặc giữ nút BOOT 5 giây.
- [x] Bảo vệ portal bằng đăng nhập, session trong RAM, rate limit, giới hạn SoftAP subnet và giới hạn JSON lồng nhau.
- [x] Cho phép portal quét Wi-Fi, sửa cấu hình mạng/MQTT, đổi mật khẩu, điều khiển relay và lưu rồi reboot.
- [x] Build firmware ESP-IDF v6.0.2 cho target `esp32s3` và tạo binary.

### Backend Spring Boot

- [x] Kết nối MQTT và định tuyến các topic status, discovery, relay state và sensor state.
- [x] Tự động đăng ký/cập nhật node và reconcile capability từ retained discovery message.
- [x] Lưu node, capability và dữ liệu cảm biến trong MySQL 8.4 bằng Flyway baseline.
- [x] Cung cấp REST API lấy danh sách/chi tiết node và gửi lệnh relay với phản hồi `202 Accepted`.
- [x] Giữ state topic làm nguồn sự thật, không tự giả định relay đã đổi trạng thái sau khi gửi lệnh.
- [x] Cung cấp API lấy telemetry mới nhất và lịch sử thô trong 24 giờ, giới hạn 10.000 bản ghi.
- [x] Phát sự kiện realtime qua STOMP/SockJS cho trạng thái node, relay và cảm biến.
- [x] Cấu hình heartbeat WebSocket 10 giây và xử lý payload MQTT lỗi mà không làm dừng luồng nhận.
- [x] Có unit test cho MQTT topic parser và discovery payload.

### Web UI

- [x] Hiển thị dashboard theo phòng với trạng thái loading, lỗi, trống và online/offline.
- [x] Hiển thị trang chi tiết node, metadata, relay và cảm biến.
- [x] Điều khiển relay theo luồng command -> chờ state xác nhận -> cập nhật giao diện.
- [x] Timeout sau 5 giây, rollback và thông báo lỗi khi node không xác nhận lệnh.
- [x] Vô hiệu hóa điều khiển khi node offline, WebSocket mất kết nối hoặc lệnh đang chờ.
- [x] Tự kết nối lại WebSocket với exponential backoff và refetch node sau khi kết nối lại.
- [x] Hiển thị lịch sử nhiệt độ/độ ẩm bằng ECharts, dual-axis, LTTB và pinch zoom.
- [x] Có mock mode để chạy giao diện và kiểm tra luồng offline/timeout khi không có phần cứng.
- [x] Build production và type-check bằng `npm run build`.

### Triển khai và CI

- [x] Docker Compose cho MySQL 8.4, Mosquitto, backend và web UI/nginx.
- [x] Nginx phục vụ SPA và reverse proxy cùng origin cho `/api` và `/ws`.
- [x] Mosquitto dùng username/password, persistence và không cho phép anonymous.
- [x] Có hướng dẫn build/run riêng cho firmware, backend, web UI và deployment.
- [x] Có pipeline GitHub Actions và GitLab CI để build/test backend, web UI và firmware.

## Cần hoàn thiện hoặc xác minh cho Giai đoạn 1

### Kiểm thử phần cứng và end-to-end

- [ ] Flash lên board thật và xác nhận dung lượng flash thực tế; cấu hình hiện đang giả định 4 MB.
- [ ] Kiểm tra hai relay và hai nút vật lý trên phần cứng, gồm debounce và điều khiển khi mất mạng.
- [ ] Kiểm tra khôi phục trạng thái relay thật sau khi mất điện.
- [ ] Gắn SHT31 thật và xác minh probe `0x44`/`0x45`, dữ liệu, CRC và chu kỳ gửi telemetry.
- [ ] Xác nhận node xuất hiện trên web UI trong tối đa 10 giây sau khi khởi động.
- [ ] Xác nhận lệnh relay từ UI phản hồi trong dưới 1 giây trên LAN.
- [ ] Xác nhận node chuyển offline trong tối đa 90 giây và đồng bộ đúng sau khi kết nối lại.
- [ ] Chạy liên tục để xác minh biểu đồ nhiệt độ/độ ẩm đủ cửa sổ 24 giờ.
- [ ] Chạy acceptance test toàn stack từ fresh clone bằng `docker compose up -d`.
- [ ] Kiểm tra giao diện trên màn hình 375 px và điện thoại thật trong cùng mạng LAN.

### Chất lượng và tự động hóa kiểm thử

- [ ] Bổ sung test backend cho repository, service, controller và MQTT integration; migration MySQL đã có Testcontainers smoke test.
- [ ] Bổ sung unit/component test cho Pinia store, relay timeout/rollback và xử lý WebSocket reconnect.
- [ ] Bổ sung test end-to-end tự động cho luồng discovery -> dashboard -> relay command -> state event.
- [ ] Bổ sung test firmware hoặc hardware-in-the-loop cho relay, nút bấm, NVS, Wi-Fi reconnect và MQTT reconnect.
- [ ] Bổ sung script lint/format cho web UI; hiện chỉ có build và type-check.

## Đã hoàn thành - quản lý khu vực và audit

- [x] Tên hiển thị Unicode NFC cho node và capability, tách khỏi tên discovery của firmware.
- [x] Cây thư mục khu vực một node/một folder, quyền RBAC kế thừa từ folder cha.
- [x] Browse sidebar responsive và map preset ngoài trời, tòa nhà, tầng, hành lang, phòng.
- [x] Loại thiết bị, tag và vị trí map theo capability điều khiển được.
- [x] Bulk relay action theo folder/subtree, loại thiết bị và tag, có preview và idempotency.
- [x] Audit từng lệnh điều khiển trước/sau publish MQTT và quyền `AUDIT_VIEW` theo subtree.
- [x] Trang xem log có filter actor, folder, node, action và khoảng thời gian.

## Triển khai sau - Giai đoạn 2

- [x] Thêm xác thực session và phân quyền người dùng cho backend và web UI.
- [ ] Giới hạn CORS/WebSocket allowed origins thay cho wildcard dùng trong môi trường phát triển.
- [ ] Bật TLS cho kết nối MQTT và quản lý certificate/secret khi triển khai.
- [ ] Thực hiện aggregation telemetry theo tham số `bucket`; hiện API nhận nhưng bỏ qua tham số này.
- [x] Cung cấp OpenAPI tĩnh đầy đủ cho REST/auth/health và hiển thị bằng Swagger UI.

## Triển khai sau - Giai đoạn 3 và mở rộng

- [ ] Thêm MQTT ACL theo từng node/người dùng; hiện mọi tài khoản đã xác thực có thể publish/subscribe mọi topic.
- [ ] Xây dựng provisioning với MQTT credential do server sinh trên recovery portal hiện có.
- [ ] Thêm factory reset khi giữ nút BOOT lâu hơn; mốc giữ 15 giây hiện mới là placeholder.
- [ ] Thiết kế OTA và partition `ota_0`/`ota_1` để cập nhật firmware từ xa.
- [ ] Triển khai driver DHT22 qua `sensor_driver` interface nếu cần hỗ trợ thêm loại cảm biến.

## Giới hạn hiện tại cần lưu ý

- [ ] Hệ thống hiện chỉ phục vụ LAN và chưa có đăng nhập người dùng.
- [ ] MQTT có mật khẩu nhưng chưa có TLS và ACL theo node.
- [ ] Thứ tự replay retained message giữa các topic không được MQTT đảm bảo; backend hiện bỏ qua state của node chưa discovery và chờ lần replay/reconnect tiếp theo.
- [ ] Dữ liệu mock chỉ xác minh luồng giao diện, không thay thế kiểm thử với thiết bị thật.
