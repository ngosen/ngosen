# LotusVibe — bản fork fcitx5-lotus dùng hằng ngày

Đây là bản fork của [fcitx5-lotus](https://github.com/LotusInputMethod/fcitx5-lotus), bộ gõ tiếng
Việt cho fcitx5, chạy hằng ngày trên CachyOS + KDE Plasma Wayland và trên một iMac Ubuntu 24.04 +
GNOME X11.

Tên LotusVibe nghĩa là bản này **chỉ sửa bằng vibecode**. Đổi tên kho ngày 17/09/2026 (tên cũ
`nguyenphivn/fcitx5-lotus`, link cũ vẫn tự chuyển về đây). Bên trong mã vẫn giữ tên `lotus` như bản
gốc, nên cài vào máy là thay cho bản Lotus gốc, không cài song song được.

Trang này chỉ ghi **những gì khác với bản gốc**. Hướng dẫn dùng bộ gõ nói chung xem
[README của bản gốc](https://github.com/nguyenphivn/LotusVibe/blob/ban-dung/README.md) hoặc
[trang chủ Lotus](https://lotusinputmethod.github.io/).

- **Mới (24/09): chỉ còn một chế độ uinput.** Ba chế độ `Uinput (Smooth)`, `Uinput (Super Smooth)`
  và `Uinput (Slow)` gộp thành một chế độ `Uinput`, chạy như Super Smooth. Cấu hình cũ tự chuyển,
  không phải chỉnh gì. [Xem bên dưới](#nên-dùng-chế-độ-nào).
- **Vá Messenger (20/09): Messenger trên Facebook hết mất chữ** — bộ gõ bôi đen chữ cần bỏ bằng
  Shift+Mũi tên trái rồi gõ đè, thay vì xoá trước rồi gõ sau. Đo 861 lần thay chữ, 0 lần mất. Bật sẵn từ 26/09.
  [Xem bên dưới](#messenger-trên-facebook-bôi-đen-rồi-gõ-đè-thay-vì-xoá-rồi-gõ-2009).
- **Cài hoặc cập nhật xong phải khởi động lại máy chủ nền:**
  `sudo systemctl restart fcitx5-lotus-server@$(whoami).service`, rồi khởi động lại fcitx5.
  [Vì sao](#cài).
- **Nhánh để dùng:** `ban-dung` (nhánh mặc định) = `dev` của bản gốc + 23 miếng vá.
- **Chi tiết từng vá, số đo, tác giả gốc trả lời ra sao, và hướng dẫn cài:**
  [KHAC-GI-SO-VOI-BAN-GOC.md](https://github.com/nguyenphivn/LotusVibe/blob/ban-dung/KHAC-GI-SO-VOI-BAN-GOC.md)

## Nên dùng chế độ nào

**Khuyên dùng `Uinput` làm chế độ gõ chính.** Đây là chế độ bản này dùng hằng ngày cho trình duyệt
(Firefox, Edge), web app như Lark, và terminal như Alacritty.

- Đặt chung: trong cài đặt Lotus chọn chế độ mặc định là `Uinput`. Nếu sửa tay
  `~/.config/fcitx5/conf/lotus.conf` thì ghi `Mode=Uinput`; ghi sai tên Lotus sẽ lặng lẽ quay về
  Preedit.
- Đặt theo từng app: trong `~/.config/fcitx5/conf/lotus-app-rules.conf` dùng số `2`, ví dụ
  `firefox=2`.
- Kiểu gõ chính là **Telex** (`InputMethod=Telex` trong `lotus.conf`). Mọi lượt đo và dùng hằng ngày
  của bản này đều gõ Telex; VNI và các kiểu gõ khác chưa kiểm.
- Không khuyên chế độ Surrounding Text: bản này không sửa chế độ đó.
- Lời khuyên này chỉ đúng cho bản fork. Super Smooth của bản gốc chưa có vá chống lặp chữ ở thanh
  địa chỉ trình duyệt.

**Chỉ còn một chế độ uinput (24/09).** Ba chế độ `Uinput (Smooth)`, `Uinput (Super Smooth)` và
`Uinput (Slow)` của bản gốc đã gộp thành một chế độ `Uinput`, chạy đúng như Super Smooth. Cấu hình cũ
tự chuyển: tên chế độ cũ trong `lotus.conf` và số `1`, `2`, `3` trong `lotus-app-rules.conf` đều đọc
thành `Uinput`. Ai đang dùng Slow sẽ chờ 4 ms mỗi phím xoá thay vì 8 ms.

## Gõ nhanh hơn bản gốc ở đâu (chế độ uinput)

Cải thiện chính của bản này nằm ở **chế độ uinput**: thay chữ nhanh hơn và
không lặp chữ. Chế độ Surrounding Text **không** được sửa.

Mọi số trong bảng dưới đây đo trên máy gốc (CachyOS, KDE Plasma Wayland). Mỗi dòng đo riêng một thay đổi, ở
thời điểm và app khác nhau, nên **không cộng dồn** thành một con số chung. Chưa có phép đo trọn vẹn
so nhánh `ban-dung` hiện tại với bản gốc.

| Đo cái gì                                             | Bản gốc → bản này                             | Đo ở đâu                                                                        |
| ----------------------------------------------------- | --------------------------------------------- | ------------------------------------------------------------------------------- |
| Thời gian thay một từ, xoá 2 / 3 / 4 chữ              | 14,5 / 21,8 / 31,2 ms → 12,4 / 16,3 / 20,5 ms | Konsole, Smooth, 8/8 mỗi mức, bản trước khi gom (khoảng cách phím xoá còn 2 ms) |
| Khoảng cách giữa hai phím xoá (trung vị)              | 5,2 ms → 0,1 ms                               | Konsole, ô soạn Edge, Firefox, Edge qua XWayland, 8/8                           |
| Chờ vô ích ở app không có surrounding text (trung vị) | 12,8 ms → 6,9 ms                              | Edge chạy qua XWayland, 16/16                                                   |

Nói cho đúng:

- Từng lần chỉ chênh vài mili giây, dưới ngưỡng cảm nhận của một lần gõ.
- App Wayland thuần không đi qua dòng thứ ba nên không đổi.
- Khoảng cách phím xoá 0 ms là lựa chọn riêng của bản này. Máy đo chưa kiểm được trường hợp phím
  xoá chen vào đúng lúc đang gõ nhanh; bằng chứng cho mức này là dùng tay hằng ngày.
- Chế độ uinput chờ app bằng hẹn giờ thay vì bắt cả fcitx5 đứng chờ. Ở bản gốc, khoảng 4% số phím
  làm fcitx5 đứng từ 8 ms trở lên, lâu nhất 22 ms (đo 428 phím). Mức cải thiện của đúng thay đổi này
  chưa đo riêng.
- Trình duyệt dùng Super Smooth. Chủ máy gõ tay thấy nhanh hơn Smooth; chưa có số đo thời gian.

## Khác gì bản gốc

### Messenger trên Facebook: bôi đen rồi gõ đè thay vì xoá rồi gõ (20/09)

Gõ Telex trong ô soạn tin Messenger ở chế độ uinput thì mất chữ: `tieengs vieetj` ra `iếngiệt`
(issue #267 của bản gốc, mở từ 05/2026). Bộ gõ vốn làm hai bước — xoá chữ cũ, rồi gõ chữ có dấu vào.
Giữa hai bước đó Facebook vẽ lại ô soạn tin và nuốt mất chữ vừa gõ vào.

Chữa bằng cách chờ thêm vài chục mili giây rồi mới gõ thì đỡ, nhưng không dứt: máy càng bận càng dễ
lọt, và chờ bao nhiêu là đủ thì không có câu trả lời cố định.

**Cách hiện tại bỏ hẳn bước xoá.** Bộ gõ bấm Shift+Mũi tên trái để bôi đen đúng số chữ cần bỏ, rồi gõ
chữ mới đè lên vùng bôi đen. Ô soạn tin không lúc nào trống và không có khe hở giữa xoá với gõ, nên
Facebook không còn chỗ chen vào. Quan trọng hơn: Edge **báo lại** "đang bôi đen N chữ", nên bộ gõ chờ
đúng tín hiệu đó rồi mới gõ, thay vì chờ đồng hồ.

Đo 20/09 bằng máy gõ tự động, lúc máy bận:

|                              | Chờ theo đồng hồ (40/60 ms) | Bôi đen rồi gõ đè    |
| ---------------------------- | --------------------------- | -------------------- |
| 100 từ đầu tiên của tin nhắn | sai 0                       | sai 0                |
| 50 câu đầy đủ                | sai 1                       | sai 0                |
| Chờ mỗi lần bỏ dấu           | 40 ms, từ đầu 60 ms         | 3–20 ms, thường 6 ms |

Bật sẵn từ 26/09 (`MessengerSelectOvertype` và `WaitSurroundingEvent` mặc định `True`). Muốn tắt thì đặt
`MessengerSelectOvertype=False` trong `~/.config/fcitx5/conf/lotus.conf`. Ba điều cần biết:

- **Chỉ dùng cho ô soạn tin Messenger.** Đo cho thấy các ô khác có khai chữ chung quanh con trỏ nhưng
  không khai lại khi chỉ bôi đen, nên bật ra toàn máy là mất dấu khắp nơi.
- **Ô không xác nhận thì bộ gõ không gõ đè**, mà trả con trỏ về chỗ cũ rồi bỏ lần bỏ dấu đó. Mất dấu
  một chữ thì thấy ngay; gõ đè khi con trỏ đang lùi thì chữ chèn sai chỗ, rối và khó phát hiện.
- **Phần bấm Shift+Mũi tên nằm ở máy chủ nền**, nên cài gói mới phải khởi động lại
  `fcitx5-lotus-server@<user>`. Tắt công tắc trên thì quay về cách chờ theo đồng hồ.
- **Bấm Shift phải, không phải Shift trái (23/09).** fcitx5 mặc định coi chạm Shift trái một mình là
  chuyển sang tiếng Anh, và bản đầu dùng Shift trái thỉnh thoảng làm bộ gõ rơi về tiếng Anh giữa lúc gõ.

Chưa đo Firefox và Chrome. Đã báo cả hai cách ở #267, chưa gửi mã.

### Sửa lỗi gặp thật

- **Hết lặp chữ đầu ở thanh địa chỉ trình duyệt với chế độ uinput** (gõ `tôi` ra `toôi`, `ê` ra
  `eê`, kiểu issue #190). Ở Chromium/Edge đã sửa triệt để, vì trình duyệt báo đúng phần tự điền. Ở
  Firefox sửa theo hình dạng ô nhập, đo 7/7 lần có gợi ý ra đúng. Mọi ô khác vẫn bỏ lá chắn
  để gõ nhanh như Super Smooth của bản gốc. Chưa gửi lên bản gốc.
- **Bỏ khoảng chờ vô ích ở app không có surrounding text.** Edge chạy qua XWayland: độ trễ trung vị
  12,8 → 6,9 ms. Bản gốc từ chối (#490).
- **Chữ V hết màu đen trên panel tối của KDE** (issue #374, gặp ngay với giao diện mặc định Fedora
  44). Lotus đọc màu của panel thay vì màu cửa sổ ứng dụng. Chụp trước và sau trên máy thật: V đen
  thành V trắng. Mã sửa đã vào bản gốc (PR #497); bản này chỉ giữ thêm bài kiểm.
- **Chữ V hết màu đen trên thanh trên cùng của GNOME** (Ubuntu Yaru, theme WhiteSur). Cùng lỗi với
  KDE: Lotus hỏi màu ứng dụng thay vì màu thanh. Giờ đọc thẳng theme của GNOME Shell. Chưa gửi lên bản
  gốc.
- **Gõ đúng trong LibreOffice với chế độ uinput** (issue #162: `chao` + `f` ra `chaà`). Nguyên nhân
  không phải máy chậm: LibreOffice xử lý phím xoá theo kiểu hẹn sau, còn chữ mới chèn ngay nên vượt
  mặt. Bản này xoá bằng surrounding text riêng cho LibreOffice. Đo trên Writer: 30–36/60 từ sai →
  0/60; Calc, Impress 0/60. Đã báo ở #162, chưa gửi mã.
- **Gõ được Messenger trên Facebook với chế độ uinput** — bốn nguyên nhân và cách chữa ở
  [mục trên](#messenger-trên-facebook-bôi-đen-rồi-gõ-đè-thay-vì-xoá-rồi-gõ-2009).
- **Chờ 4 ms mỗi phím xoá thay vì 2** ở chế độ uinput. Máy tải nặng: 2 ms đúng 39–46/60
  câu, 4 ms đúng 58–60/60. Máy rảnh không khác.

### Đang chờ tác giả gốc trả lời

- **Chờ sự kiện surrounding text thay vì ngủ theo một hằng số đoán trước** — mặc định TẮT.
- **Chế độ Smooth chờ bằng hẹn giờ** thay vì chặn cả vòng lặp của fcitx5.

Cả hai nằm ở [PR #492](https://github.com/LotusInputMethod/fcitx5-lotus/pull/492).

### Dọn dẹp và hạ tầng

- Siết dịch vụ systemd của máy chủ: `systemd-analyze security` từ 7.0 xuống 2.0.
- Máy chủ bàn phím ảo chỉ mở chuột và bàn chạm, không còn đọc được bàn phím; ra khỏi nhóm `input`.
- Máy chủ và mô-đun kiểm nhau bằng tài khoản thay vì tên chương trình; bỏ `CAP_SYS_PTRACE`.
- Máy chủ bỏ qua con số ngoài khoảng ±1024 thay vì xin bộ nhớ tuỳ ý.
- Gỡ công tắc `FixUinputWithAck` vốn mặc định tắt.
- Biến môi trường `LOTUS_SOCKET_NAMESPACE` cho máy chủ, `LOTUS_BACKSPACE_GAP_MS`.
- Khoảng cách giữa hai phím xoá mặc định 0 ms thay vì 5 ms. Đây là lựa chọn riêng của bản này.

### Kiểm thử

- 26 bài kiểm thay vì 9: thêm kiểm bất biến trên chuỗi phím ngẫu nhiên, kiểm màu panel KDE và GNOME, tái
  hiện lỗi giữ phím của issue #472, tám bài cho lỗi Messenger trên Facebook, và ba bài cho máy chủ
  bàn phím ảo.

## Bản này KHÔNG sửa

- Chế độ Surrounding Text vẫn lỗi, nhất là trên Firefox và LibreOffice Writer (60/60 từ sai).
- Máy chủ bàn phím ảo chết giữa lúc thay chữ thì bàn phím chết theo.

## Cài

Làm theo mục
[Cài sang máy khác](https://github.com/nguyenphivn/LotusVibe/blob/ban-dung/KHAC-GI-SO-VOI-BAN-GOC.md#cài-sang-máy-khác).
Nhớ gỡ bản Lotus đóng gói sẵn trước, và cài vào `/usr`.

**Cài xong, hoặc mỗi lần cập nhật, phải khởi động lại máy chủ nền** (chương trình chạy ngầm bấm
phím xoá thay bộ gõ), rồi khởi động lại fcitx5:

```
sudo systemctl restart fcitx5-lotus-server@$(whoami).service
fcitx5 -rd
```

Cài gói mới chỉ thay tệp trên đĩa. Máy chủ đang chạy vẫn là bản cũ cho tới khi khởi động lại. Bản
fork thêm lệnh bôi đen cho vá Messenger; máy chủ bản gốc không hiểu lệnh này, nó xoá nhầm một chữ
rồi đếm sai số phím xoá, làm chữ bị sai ở mọi ứng dụng cho tới khi khởi động lại.
