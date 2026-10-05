# Nhật ký thay đổi

Mọi thay đổi đáng kể của Ngó Sen (tên cũ LotusVibe), tách ra từ
[fcitx5-lotus](https://github.com/LotusInputMethod/fcitx5-lotus), được ghi ở đây. Cách ghi theo
[Keep a Changelog](https://keepachangelog.com/vi/1.1.0/). Mỗi dòng nói người dùng thấy gì thay đổi;
lý do và số đo của từng miếng vá nằm trong [KHAC-GI-SO-VOI-BAN-GOC.md](KHAC-GI-SO-VOI-BAN-GOC.md).

Số phiên bản gồm hai phần: `3.5.10` là số của bản gốc lúc tách ra, còn nằm trong `CMakeLists.txt`; số
sau dấu gạch là lần đóng gói của fork. Mỗi bản phát hành mang nhãn `ngosen-<phiên bản>`.

## [Chưa phát hành]

### Thêm

- README có mục "Gỡ": tắt máy chủ nền trước, lệnh gỡ gói cho từng bản phân phối, lệnh gỡ bản tự dựng,
  và hai thứ còn lại trên máy sau khi gỡ là tệp cấu hình và tài khoản `uinput_proxy` (#27).
- Trang Giới thiệu trong cửa sổ cài đặt có lại hai nút "Báo cáo lỗi" và "Đề xuất tính năng"; chúng mở
  mẫu tương ứng ở mục Issues của kho `ngosen/ngosen` (#31).

### Thay đổi

- Mục Issues của kho được bật để nhận báo lỗi. Mẫu báo lỗi hỏi phiên bản và cách cài của Ngó Sen thay
  cho các cách cài của bản gốc; README chỉ tới mục Issues (#30).

### Bỏ

- Chế độ Minecraft. Đặt nhầm chế độ này cho app thường thì app còn sót một chữ cũ. Cấu hình cũ đặt
  Minecraft (`Mode=Minecraft`, số `8` trong luật theo app) tự đọc thành `Uinput`; chế độ này cũng biến
  khỏi bảng chọn chế độ và cửa sổ cài đặt (#29).

### Sửa

- Trên KDE Plasma (Wayland), bộ gõ tự gửi phím xoá cho ứng dụng thay vì nhờ máy chủ uinput, rồi chờ ứng
  dụng báo đã xoá xong mới gõ chữ mới. Ứng dụng X11 chạy qua XWayland không còn bị kẹt, nuốt hết phím
  sau một lần thay chữ. Ô soạn tin Facebook trên KDE xoá rồi gõ lại như ô thường, không bôi đen rồi gõ
  đè nữa (#33).
- Trên GNOME, gõ nhanh trong Firefox không còn mất chữ ("viet" ra "v"), ô soạn tin Facebook trên Edge
  không còn ra "i" thay cho "đi", và chuyển từ app khác sang terminal không còn làm rơi chữ. Luật theo
  app nhận cả tên GNOME báo kèm đuôi `.desktop` (như `firefox.desktop`) (#33).

### Tài liệu

- Nhật ký thay đổi có mục riêng cho bản `3.5.10-4`; những gì gộp sau bản đó nằm dưới "Chưa phát hành"
  (#28).

### Kiểm thử

- Bốn bài kiểm thử khoảng chờ của Messenger không còn hỏng khi máy chạy thử bị khựng vài chục phần
  nghìn giây: khoảng chờ trong bài nới từ 40 lên 500 ms, và vòng lặp sự kiện của bài chạy nốt việc
  đã tới hạn trước khi kiểm. Không đổi gì ở bộ gõ (#32).

## [3.5.10-4] — 04/10/2026

Bản phát hành đầu tiên có gói dựng sẵn:
[ngosen-3.5.10-4](https://github.com/ngosen/ngosen/releases/tag/ngosen-3.5.10-4).

### Thêm

- Kịch bản cài một dòng `install.sh`: nhận ra bản phân phối, tải gói dựng sẵn từ bản phát hành mới
  nhất, đối chiếu mã băm, hỏi lại rồi cài và bật máy chủ nền. Thay được gói `fcitx5-lotus` đang có,
  kể cả bản gốc số cao hơn, và ghi đè được bản cài từ mã trên Arch. README hướng dẫn cài theo cách
  này; hướng dẫn cập nhật bản tự dựng được bổ sung các lệnh nạp lại luật quyền (#26).

### Thay đổi

- Trang Giới thiệu trong cửa sổ cài đặt trỏ về kho `ngosen/ngosen` và ghi "Dựa trên fcitx5-lotus của"
  kèm tên các tác giả gốc. Bỏ hai nút "Báo cáo lỗi" và "Đề xuất tính năng", vì chúng mở trang báo lỗi
  của bản gốc (#20).
- Bộ gõ đổi tên hiển thị thành **Ngó Sen**: tên trong danh sách bộ gõ và addon của fcitx5, nhãn "Ngó
  Sen - Tắt", cửa sổ và mục menu "Cài đặt Ngó Sen", trang Giới thiệu. Tệp cấu hình và tên bên trong
  giữ nguyên, nên cấu hình cũ dùng tiếp được (#15).
- Gói Fedora đổi tên thành `fcitx5-ngosen`. Cài gói này thì bản `fcitx5-lotus` cũ của fork được thay
  tự động; gói này và `fcitx5-lotus` của bản gốc không cài chung được (#16).
- Gói Debian/Ubuntu đổi tên thành `fcitx5-ngosen` và thay gói `fcitx5-lotus` khi cài. Khi cài hoặc
  cập nhật, gói tự gỡ tài khoản `uinput_proxy` khỏi nhóm `input` và áp luật quyền cho chuột, bàn chạm
  đang cắm, giống gói Fedora (#23).
- Gói openSUSE đổi tên thành `fcitx5-ngosen`, có cùng các bước sau cài như gói Fedora. Thêm công thức
  gói Arch (`packaging/arch/PKGBUILD`) cho ra gói pacman `fcitx5-ngosen`; quy trình dựng thử trên
  GitHub nay dựng gói này thay cho tệp nén thô (#24).
- Quy trình phát hành viết lại: gắn nhãn `ngosen-<phiên bản>` thì GitHub kiểm số phiên bản trong các
  tệp đóng gói, dựng gói cho Fedora, Debian, Ubuntu, openSUSE và Arch, rồi tạo bản phát hành nháp kèm
  `SHA256SUMS`. Bỏ kho gói có chữ ký và chìa khoá công khai của bản gốc. Các gói lên `3.5.10-4` (#25).
- Kho chuyển về địa chỉ `github.com/ngosen/ngosen` (link cũ tự chuyển). Trang giới thiệu viết lại theo
  tên Ngó Sen và nằm ở `README.md`; README của bản gốc không còn trong kho, xem ở kho fcitx5-lotus
  (#17).
- Cập nhật bamboo-core (lõi bộ gõ Telex/VNI) theo bản gốc (#11).

### Bỏ

- Gói Nix (`flake.nix`, thư mục `nix/`) và hai quy trình kiểm Nix. Công thức này tải mã của bản gốc
  về dựng chứ không dựng mã của Ngó Sen, nên chưa bao giờ cho ra đúng bản này (#21).
- Biến môi trường `LOTUS_SERVER_PATH`: mô-đun không còn kiểm đường dẫn của máy chủ nền (#9).
- Giao diện cài đặt: bỏ dòng và lời giải thích còn sót của tuỳ chọn `FixUinputWithAck` đã gỡ. Giao
  diện vốn không hiện tuỳ chọn này, vì addon không còn khai báo nó (#14).

### Sửa lỗi

- Gói Fedora: khi cập nhật, tài khoản `uinput_proxy` tự được gỡ khỏi nhóm `input`, và luật quyền cho
  chuột, bàn chạm được áp ngay cho thiết bị đang cắm, không phải khởi động lại máy hay cắm lại (#19).
- Máy chủ nền giữ tối đa 1024 phím xoá trong hàng chờ và bỏ hàng chờ khi bộ gõ ngắt kết nối. Trước đây
  một chương trình gửi dồn dập có thể khiến máy chủ tiếp tục xoá chữ rất lâu sau khi nó đã dừng (#18).
- Khi bộ gõ khởi động lại và nối lại với máy chủ nền đúng lúc kết nối cũ vừa đứt, máy chủ không còn
  ngắt nhầm kết nối mới, và không gõ tiếp các phím xoá còn dở của kết nối cũ (#22).
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
