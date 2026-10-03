# Nhật ký thay đổi

Mọi thay đổi đáng kể của LotusVibe, bản fork của
[fcitx5-lotus](https://github.com/LotusInputMethod/fcitx5-lotus), được ghi ở đây. Cách ghi theo
[Keep a Changelog](https://keepachangelog.com/vi/1.1.0/). Mỗi dòng nói người dùng thấy gì thay đổi;
lý do và số đo của từng miếng vá nằm trong [KHAC-GI-SO-VOI-BAN-GOC.md](KHAC-GI-SO-VOI-BAN-GOC.md).

Fork chưa có số phiên bản riêng. `CMakeLists.txt` vẫn giữ số phiên bản của bản gốc lúc tách ra.

## [Chưa phát hành]

### Thay đổi

- Cập nhật bamboo-core (lõi bộ gõ Telex/VNI) theo bản gốc (#11).

### Bỏ

- Biến môi trường `LOTUS_SERVER_PATH`: mô-đun không còn kiểm đường dẫn của máy chủ nền (#9).
- Giao diện cài đặt: bỏ dòng và lời giải thích còn sót của tuỳ chọn `FixUinputWithAck` đã gỡ. Giao
  diện vốn không hiện tuỳ chọn này, vì addon không còn khai báo nó (#14).

### Sửa lỗi

- Luật udev không còn cấp `/dev/uinput` và mọi thiết bị nhập cho cả nhóm `input`, khớp bản gốc
  (LotusInputMethod/fcitx5-lotus#525) (#10).
- Máy chủ nền và mô-đun bộ gõ nhận nhau theo tài khoản (uid) của tiến trình bên kia, thay cho đường
  dẫn chương trình. Máy chủ không còn giữ quyền `CAP_SYS_PTRACE`. Mô-đun nay kiểm cả socket phím
  xoá, nên chương trình chiếm tên socket trước không đọc được độ dài từng từ (#9).
- Máy chủ nền chỉ mở chuột, bàn chạm và núm trỏ, ở chế độ chỉ đọc; không mở bàn phím nữa, và tài
  khoản `uinput_proxy` ra khỏi nhóm `input`. Chuột kiêm bàn phím mất tính năng bấm chuột để ngắt từ.
  Máy đã cài từ trước cần chạy một lần `sudo gpasswd -d uinput_proxy input` (#8).
- Máy chủ nền chỉ nhận số phím từ 1 tới 1024 (backspace) hoặc từ -1 tới -1024 (bôi đen); số khác bị
  bỏ qua và ghi log. Trước đây một số âm rất lớn làm máy chủ chết, bàn phím chết theo (#7).
- Chế độ Uinput, app nhận chữ qua dbus (fcitx5-gtk): chữ thay thế được commit ngay sau khi phím xoá
  xử lý xong thay vì trong lúc xử lý, để Ghostty và foot không làm rơi chữ. Lấy từ bản gốc
  (LotusInputMethod/fcitx5-lotus#510), chưa tái hiện được lỗi trên máy thử (#12).
- Chế độ Preedit: bấm phím mở menu chế độ khi đang gõ dở một chữ thì chữ đó được commit và bộ gõ về
  trạng thái đầu. Lấy từ bản gốc (#11).
- Chế độ Preedit: xử lý phím theo độ dài chữ đang soạn thay vì độ dài phím vừa bấm, và chặn lỗi ở
  macro Tab. Lấy từ bản gốc (#11).

## Mốc khởi đầu của fork — 26/09/2026

`ban-dung` ở commit `b121f3c`, dựa trên nhánh `dev` của bản gốc tại `79d5706` (15/09/2026), cộng
các commit lấy thêm từ bản gốc ngày 24/09/2026. Mọi mục dưới đây là chỗ fork khác bản gốc đó.

### Thay đổi

- Gộp ba chế độ uinput (Slow, Smooth, Super Smooth) thành một chế độ **Uinput**. Cấu hình cũ, luật
  theo app (chế độ 1–3) và thứ tự chế độ tự chuyển sang; menu chế độ chỉ hiện nó một lần (#6).
- Ở chế độ Uinput, sau khi gửi backspace, bộ gõ chờ app báo surrounding text đã đổi (bằng timer)
  thay vì ngủ một khoảng cố định (`WaitSurroundingEvent`, bật sẵn).
- App không báo surrounding text (terminal, Chromium) bỏ qua bước chờ thử lại.
- Server gửi backspace liền nhau; `LOTUS_BACKSPACE_GAP_MS` (0–50) để chèn khoảng nghỉ.
- Các bản sửa cho Messenger và ô soạn bài Facebook bật sẵn (#5).
- Tên biến, comment và log trong mã riêng của fork viết tiếng Anh, theo [AGENTS.md](AGENTS.md) (#4).

### Thêm

- Messenger và ô soạn bài Facebook: bôi đen chữ cũ bằng Shift phải + mũi tên trái rồi gõ đè, thay
  vì xoá trước, nên ô không bao giờ bị trống (`MessengerSelectOvertype`). Phím gõ trong lúc bôi đen
  bị quá giờ được gõ lại.
- Messenger: không commit khi surrounding text mới cập nhật một nửa, và chờ thêm một chút sau khi đã
  xoá xong (`WaitSurroundingSettleMs` 40 ms, chữ đầu tiên của tin nhắn 60 ms).
- LibreOffice: chế độ Uinput xoá qua surrounding text thay vì gửi phím backspace.
- Icon khay đổi màu theo thanh trên cùng của GNOME Shell. Khôi phục cách tìm đường dẫn icon (bản gốc
  đã bỏ) cho panel của KDE Plasma.
- Unit systemd của server được siết quyền.
- Server nhận `LOTUS_SOCKET_NAMESPACE` như addon; `LOTUS_SERVER_PATH` chỉ định server mà monitor chờ
  (lấy từ CMake thay vì ghi cứng `/usr/bin`).
- Test: kiểm bất biến trên chuỗi phím ngẫu nhiên, giữ phím ở Smooth
  (LotusInputMethod/fcitx5-lotus#472), icon trên panel KDE (LotusInputMethod/fcitx5-lotus#374), và
  kiểm chính tả bằng từ điển trong mã nguồn thay vì từ điển cài trên máy.

### Bỏ

- `FixUinputWithAck` và cách lách gợi ý của Chromium.

### Sửa lỗi

- Server xử lý hết hàng sự kiện libinput ở mỗi vòng lặp, không để sự kiện nằm chờ
  (LotusInputMethod/fcitx5-lotus#507).
- Lá chắn chống lặp chữ do gợi ý tự điền chỉ bật ở thanh địa chỉ trình duyệt.

### Tài liệu

- [KHAC-GI-SO-VOI-BAN-GOC.md](KHAC-GI-SO-VOI-BAN-GOC.md): từng miếng vá, vì sao có, đã gửi lên bản
  gốc chưa.
- [AGENTS.md](AGENTS.md): quy định khi sửa fork và khi gửi vá lên bản gốc (#3).
- README: cài sang máy khác, chế độ Uinput duy nhất, khởi động lại server sau khi cập nhật.
