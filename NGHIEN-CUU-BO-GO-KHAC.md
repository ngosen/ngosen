# Các bộ gõ tiếng Việt khác trên Linux đưa chữ vào ứng dụng thế nào (24/09/2026)

Khảo sát 18 bộ gõ trong [danh sách cộng đồng](https://docs.google.com/spreadsheets/d/1DhW_jVM3IPSR1fX2CJyZstNFqoHDL3u6aLOrj2tYDk4/edit?gid=1097134275).
Câu hỏi đặt ra: khi gõ `as` ra `á`, bộ gõ phải xoá `a` rồi viết `á`. Bộ nào làm việc đó theo cách khác
Ngó Sen và đáng học?

Cách đọc: chỉ đọc mã, không cài, không chạy. Chỗ nào ghi "đã kiểm" là đã mở đúng file, đúng dòng ở
commit ghi trong bảng cuối. Số đo do tác giả khác tự công bố thì ghi rõ là "theo README của họ".

## Vì sao phải hỏi câu này

Chế độ Super Smooth của Ngó Sen (uinput) đi **hai đường**: bàn phím ảo bấm phím xoá, còn chữ mới
đi đường riêng của bộ gõ (commit). Mọi lỗi mất chữ ở Messenger và ô đăng bài Facebook đều từ chuyện
hai đường này lệch nhịp (xem [KHAC-GI-SO-VOI-BAN-GOC.md](KHAC-GI-SO-VOI-BAN-GOC.md)). Bộ gõ nào
làm được mà **chỉ một đường** là thứ đáng học.

## Tóm tắt: 18 bộ gõ chia làm 5 cách

| Cách đưa chữ vào                                                                     | Bộ gõ                                                                                 |
| ------------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------- |
| Một đường: nhờ ứng dụng xoá chữ quanh con trỏ (surrounding text), rồi chèn chữ mới   | **Funput**, **Unikey-Wayland-Final**, **CanType**, pinakey (chế độ 1), vnkey, telebit |
| Chữ gạch chân tạm (preedit) nhưng tắt gạch chân cho giống chữ thường                 | **pinakey** (mặc định), **TypeVN**, vietc                                             |
| Tự làm bộ gõ Wayland riêng, gõ bằng bàn phím ảo mang bảng phím chữ Việt              | **vi-ime**                                                                            |
| Hai đường: uinput bấm xoá (hoặc bôi đen), rồi commit, căn bằng đồng hồ hoặc tín hiệu | skey, ArecaIME, VMK, fcitx5-lilypad, Ngó Sen, fcitx5-lotus                            |
| X11: đổi bảng phím từng chữ rồi bấm                                                  | Unikey-Wayland (bản cũ), nhánh X11 của Unikey-Wayland-Final                           |

Không có mã hoặc không mở được: UniLume (không có mã, chuyển thành CanType), codekeyvn (404).

## Đáng học nhất

### 1. Funput: một đường, rồi đọc lại ô để biết ứng dụng có làm đúng không

- **Cách gõ (đã kiểm):** xoá bằng `deleteSurroundingText`, rồi `commitString`, cùng đường bộ gõ, không
  có uinput (`platforms/linux/fcitx5/src/funput_client.cpp:113-127`). Có cả bản IBus dùng chung lõi.
- **Chống mất chữ (đã kiểm):** không chờ trước khi gõ. Nó gõ luôn, rồi đọc nội dung ô ứng dụng báo lại
  (SurroundingTextUpdated) và phân ba loại: làm đúng, ứng dụng bỏ lệnh xoá, hoặc chưa trả lời
  (`common/compose/composer/nonpreedit.cpp:24-50`). Ứng dụng nào bỏ lệnh xoá thì **riêng ứng dụng đó**
  bị chuyển về gõ chữ gạch chân, và nó nhớ ứng dụng đó theo tên để lần sau khỏi mất chữ nữa.
- **Số đo theo README của họ (chưa kiểm lại):** ứng dụng chỉ trả lời 61% lần commit; chờ từng lần sẽ
  chậm khoảng 25 ms mỗi phím; xoá và chèn dồn liền nhau không khoảng nghỉ thì hỏng chữ
  (`README.md:475-487`).
- **Học được gì:** ý "đo xong rồi xếp loại ứng dụng" thay vì đoán trước. Ngó Sen hiện nhận diện ô
  bằng hình dạng (đuôi `\n\n`); Funput nhận diện bằng hành vi thật của ô.

### 2. Unikey-Wayland-Final: giữ phím kế tiếp lại tới khi ứng dụng xác nhận

- **Cách gõ (đã kiểm):** bộ gõ Wayland riêng dùng giao thức input-method-v1, **đúng giao thức KWin có**
  (`wayland-client/src/main.cpp:226-233`). Xoá rồi chèn, một đường.
- **Chống mất chữ (đã kiểm), đây là ý chính:** sau một lần thay chữ, nó ghi "đang chờ", và **mọi phím
  gõ tiếp bị xếp hàng** (`main.cpp:584-590`). Chỉ thả hàng khi ô báo lại đúng đuôi chữ mong đợi
  (`main.cpp:642-689`). Nếu thấy dấu hiệu lệnh xoá bị bỏ thì làm lại, tối đa 3 lần (`main.cpp:256-288`).
  Đồng hồ 750 ms chỉ là lưới an toàn (`main.cpp:172-189`).
- **Một ghi chú khớp với số đo của mình (đã kiểm, `main.cpp:664-665`):** "KWin exposes the delete and
  commit as separate text-input-v3 transactions". Tức là trên KDE, xoá và chèn tới ứng dụng thành hai
  lần riêng, giống điều em thấy ở fcitx5 5.1.22 (`waylandimserverv2.cpp`).
- **Chất lượng mã:** kho chứa cả file cài đặt đã build (Setup.exe, .deb, .rpm, a.out), build trỏ vào
  đường dẫn trên máy tác giả. Học ý, đừng chép mã.
- **Học được gì:** cái chốt "chưa xác nhận thì chưa xử lý phím kế" là thứ Ngó Sen chưa có. Ngó Sen
  đang chờ trong từng lần thay chữ; cái chốt này chặn cả chuỗi phím sau.

### 3. vi-ime: bàn phím ảo gõ thẳng chữ Việt, xoá và chèn gửi một lần

- **Cách gõ (đã kiểm):** bộ gõ Wayland viết bằng Rust, dùng input-method-v2 và bàn phím ảo của Wayland
  (zwp_virtual_keyboard_v1). Khi thay chữ, nó gửi lệnh xoá, lệnh chèn rồi **một** lệnh chốt, nên ứng
  dụng nhận cả hai cùng lúc (`crates/vi-daemon/src/wayland/state.rs:252-260`).
- **Bảng phím chữ Việt (đã kiểm, đầu file `viet_typer.rs`):** một bảng phím cố định, 8 mức, trên 36 phím
  hàng chữ an toàn, khoảng 280 chỗ, nạp một lần lúc đầu. Đây là **đúng ý tưởng** của kế hoạch "gõ thẳng
  ra chữ Việt" (workbench `probes/phim-truc-tiep/KE-HOACH.md`), và nó đã chạy được ở chỗ khác.
- **Bài học họ ghi lại, áp thẳng vào kế hoạch đó:**
  - Phím đặc biệt "ăn" chữ: mã 107 là phím End, 162 cũng hỏng. Kế hoạch của mình đang dùng F13–F15,
    nên phải đo xem ứng dụng có coi F13+ là phím chức năng không. Có thể phải dùng mức 3–8 trên phím chữ
    thường như họ.
  - Đổi bảng phím giữa chừng thì Chrome/Edge áp dụng trễ ("ấ" rơi vào phím Enter). ⇒ bảng phím phải cố
    định từ đầu, đúng như kế hoạch.
- **Không chạy trên KDE:** KWin (Plasma 6.7.5) không có input-method-v2 và không có bàn phím ảo Wayland
  (đã kiểm bằng `wayland-info` trên máy chính). Chỉ chạy trên Sway, Hyprland và các môi trường wlroots.

### 4. pinakey và TypeVN: chữ gạch chân nhưng tắt gạch chân

- **pinakey (đã kiểm):** mặc định gõ bằng chữ tạm với cờ "không gạch chân" (`fcitx5/src/pinakey.cpp:296`).
  Chế độ uinput có nhưng tắt mặc định, phải bật bằng biến môi trường `PINAKEY_UINPUT=1`
  (`:331-349`). Lý do họ ghi: trên GNOME, đường D-Bus không bảo đảm thứ tự, "kể cả uinput+ACK kiểu Lotus".
- **TypeVN (đã kiểm):** IBus, chữ tạm không gạch chân ở chế độ tự chốt (`crates/ibus-typevn/c/engine.c:77-93`),
  tự chốt sau 800 ms không gõ hoặc khi rời ô.
- **Chưa kiểm:** ứng dụng có thật sự bỏ gạch chân khi được yêu cầu không. Tuỳ từng ứng dụng, phải thử.
- **Học được gì:** một đường dự phòng rẻ cho ô khó (Messenger) nếu nó nhìn đủ giống chữ thường.

### 5. CanType: không biết ô có gì thì không xoá

- **Đã kiểm:** trước khi xoá, nó kiểm phần trước con trỏ có kết thúc bằng đúng chữ nó vừa gõ không
  (`src/phien.rs:359-362`). Ứng dụng không báo nội dung ô thì chỉ chèn, không bao giờ xoá.
- **Học được gì:** luật an toàn đơn giản, đáng thêm vào đường surrounding text của Ngó Sen.

## Cùng họ với Ngó Sen (hai đường, không có gì mới đáng lấy)

- **skey:** fcitx5 + lõi Rust + uinput. Có vài chi tiết kỹ: bấm xoá thừa một phím làm mốc, đồng hồ tự
  điều chỉnh theo trung bình, theo dõi ô qua AT-SPI, kiểm người gọi socket bằng SO_PEERCRED, bộ test
  1481 dòng. Điểm trừ: luật polkit cho **mọi người** quyền nạp lại dịch vụ.
- **ArecaIME:** 5 cách thay chữ đổi được, đều canh bằng đồng hồ (đồng hồ tự tăng 5 ms mỗi bước tới
  50 ms, `adaptive_wait.h:12-50`). Có cách "bôi đen bằng Shift+Trái rồi gõ đè" ghi rõ là để trị
  React/Facebook, giống hướng Ngó Sen đã chọn, nhưng nó chốt bằng đồng hồ chứ không chờ ô xác nhận.
  Luật udev cho người dùng quyền đọc cả chuột và bàn di. Lõi Go để dạng file nhị phân build sẵn.
- **VMK:** bấm xoá thừa một phím + chờ cố định 20 ms. Quyền uinput mở rộng không an toàn. Bỏ qua.
- **fcitx5-lilypad:** bản Lotus đổi tên từ đầu tháng 8, thêm chế độ gõ theo đồng hồ và tự gửi xác nhận.
  Đánh giá riêng trong phiên 24/09: không có gì đáng lấy.
- **Unikey-Wayland (bản cũ):** X11, đổi bảng phím cho từng chữ bằng `XChangeKeyboardMapping` rồi bấm,
  có ngủ giữa các bước. Không áp được cho Wayland.
- **vietc, vnkey, telebit:** một đường surrounding text hoặc chữ tạm, không có cơ chế chống mất chữ
  gì khác đáng kể.

## Quyết định (24/09)

Chỉ đi tiếp hướng **gõ thẳng chữ Việt bằng bàn phím ảo** (học từ vi-ime). Bài học của vi-ime đã ghi
vào kế hoạch ở workbench, `probes/phim-truc-tiep/KE-HOACH.md`, mục "Bài học từ vi-ime". Các ý khác
(giữ phím kế tiếp, nhớ ô hay mất chữ, chữ tạm không gạch chân) chủ fork quyết định không làm.

## Các commit đã đọc

| Bộ gõ                | Commit, ngày          |
| -------------------- | --------------------- |
| Funput               | `a0278f1`, 2026-09-23 |
| Unikey-Wayland-Final | `1d7c107`, 2026-09-04 |
| TypeVN               | `1fd1910`, 2026-09-10 |
| CanType              | `603a20a`, 2026-08-07 |
| ArecaIME             | `5c6e34a`, 2026-09-13 |
| vi-ime               | `71cebcb`, 2026-07-16 |
| Unikey-Wayland       | `4323ffb`, 2026-08-09 |
| skey                 | `915246f`, 2026-09-16 |
| vietc                | `e98e4f0`, 2026-09-14 |
| pinakey              | `3068680`, 2026-08-31 |
| vnkey                | `b75cdc0`, 2026-04-03 |
| telebit              | `969efe2`, 2026-09-15 |
| VMK                  | `4973ec3`, 2026-09-20 |
