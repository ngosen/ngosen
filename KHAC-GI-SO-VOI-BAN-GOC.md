# LotusVibe khác gì so với Lotus gốc

LotusVibe là bản fork của [fcitx5-lotus](https://github.com/LotusInputMethod/fcitx5-lotus), dùng hằng
ngày trên hai máy: CachyOS + KDE Plasma Wayland, và một iMac chạy Ubuntu 24.04 + GNOME X11 (máy tính
tiền kiêm máy chạy CI). Tệp này ghi lại **từng miếng vá**: vá gì, vì sao, đã
gửi ngược lên chưa, và tác giả trả lời ra sao.

## Lấy bản nào

**Nhánh `ban-dung`.** Đó là nhánh duy nhất nên lấy để dùng hoặc để thử trên máy khác.

```
git clone https://github.com/nguyenphivn/LotusVibe.git
cd LotusVibe
git checkout ban-dung
```

`ban-dung` = `upstream/dev` + đúng **23 miếng vá**, không thiếu commit nào của tác giả. Từ
13/09/2026 đây cũng là **nhánh mặc định** của fork.

Mấy nhánh khác là nhánh làm việc, **đừng lấy**:

| Nhánh | Là gì | Có nên lấy |
| --- | --- | --- |
| `ban-dung` | Bản gom gọn, đang dùng hằng ngày | **Có** |
| `pr/*`, `fix/*`, `do/*`, `proto/*` | Từng nhánh nhỏ để gửi PR hoặc để đo | Không |
| `main`, `dev` | Chép theo kho gốc | Không |

Các nhánh gom cũ `tong-hop`, `tong-hop-v2` và `thu/bo-fixack` **đã xoá ngày 13/09/2026**. Mọi vá
còn giá trị của chúng đều nằm trong `ban-dung`.

## Cài sang máy khác

Các bước dưới đây cho Arch và CachyOS. Distro khác thì gói cần cài lấy ở mục "Yêu cầu hệ thống"
trong `README.md`, các bước còn lại giống hệt.

**1. Gỡ bản Lotus đóng gói sẵn, nếu máy đã có.** Không gỡ thì tệp của hai bản đè lên nhau, và lần
cập nhật hệ thống sau sẽ báo lỗi tệp xung đột.

```
pacman -Qs fcitx5-lotus
sudo pacman -R fcitx5-lotus
```

**2. Cài công cụ dựng.**

```
sudo pacman -S --needed cmake extra-cmake-modules gcc go git python make pkgconf acl fcitx5 libinput hicolor-icon-theme python-qtpy python-dbus librsvg
```

**3. Tải mã và dựng.** Bắt buộc cài vào `/usr`: dịch vụ nền ghi cứng đường
`/usr/bin/fcitx5-lotus-server`, cài chỗ khác thì máy chủ bàn phím ảo không chạy. Nhớ
`--recurse-submodules`, vì lõi bộ gõ nằm ở kho con `bamboo-core`.

```
git clone --recurse-submodules -b ban-dung https://github.com/nguyenphivn/LotusVibe.git
cd LotusVibe
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=/usr/lib
cmake --build build -j8
sudo cmake --install build
```

`CMAKE_BUILD_TYPE=Release` là để bật tối ưu tốc độ. Bỏ cờ này thì bản dựng không được tối ưu.

**4. Bật máy chủ bàn phím ảo cho tài khoản của mình.**

Bốn lệnh đầu tạo người dùng hệ thống `uinput_proxy`, nạp mô-đun bàn phím ảo và nạp luật quyền truy
cập vừa cài, để khỏi phải khởi động lại máy.

```
sudo systemd-sysusers
sudo modprobe uinput
sudo udevadm control --reload-rules
sudo udevadm trigger --subsystem-match=misc --subsystem-match=input
sudo systemctl daemon-reload
sudo systemctl enable --now fcitx5-lotus-server@$(whoami).service
sudo systemctl restart fcitx5-lotus-server@$(whoami).service
systemctl status fcitx5-lotus-server@$(whoami).service       # phải thấy active (running)
```

Lệnh `restart` là bắt buộc nếu máy từng chạy Lotus: `enable --now` không đụng tới máy chủ đang chạy,
nên máy chủ bản gốc cũ vẫn ở lại. Máy chủ đó không hiểu lệnh bôi đen của vá Messenger, xoá nhầm chữ
rồi đếm sai số phím xoá ở mọi ứng dụng. Mỗi lần cập nhật bản fork cũng chạy lại lệnh `restart` này.

**5. Thêm bộ gõ.** Khởi động lại fcitx5 (hoặc đăng xuất rồi vào lại), mở "Fcitx5 Configuration" và
thêm Lotus. Trên KDE Wayland: System Settings → Virtual Keyboard → chọn "Fcitx 5".

**6. Đặt luật theo app giống máy gốc.** Sửa `~/.config/fcitx5/conf/lotus-app-rules.conf` (hoặc đặt
trong cửa sổ cài đặt Lotus), rồi khởi động lại fcitx5:

```
firefox=2
microsoft-edge=2
Alacritty=2
```

Luật `firefox=2` chỉ an toàn trên bản này, vì cần vá lá chắn thanh địa chỉ ở nhóm E. Lá chắn nhận
Firefox theo tên chương trình: bản Firefox mang tên khác (LibreWolf, Firefox Developer Edition)
**chưa** được nhận. Trình duyệt họ Chromium khác (Chromium, Brave) tự khai ô địa chỉ giống Edge nên
nhiều khả năng chạy, nhưng **chưa đo**.

Từ 26/09 các tuỳ chọn cho Messenger bật sẵn, không cần đặt trong `~/.config/fcitx5/conf/lotus.conf`:
`WaitSurroundingEvent=True` (vá nhóm B), `MessengerSelectOvertype=True` (bôi đen rồi gõ đè, vá nhóm E),
`WaitSurroundingSettleMs=40` và `WaitSurroundingSettleFirstWordMs=60` (đường lùi khi tắt bôi đen).
Vá thanh địa chỉ không phụ thuộc các tuỳ chọn này.

**Cập nhật bản mới về sau:** trong thư mục `LotusVibe`, chạy `git pull --recurse-submodules`,
lặp lại bước 3, rồi `sudo systemctl restart fcitx5-lotus-server@$(whoami).service` và khởi động lại
fcitx5.

## Dựng và chạy bộ kiểm

Phần kiểm thử mặc định TẮT trong CMake, phải bật bằng `-DBUILD_TESTING=ON`, không thì `ctest` báo 0
bài:

```
cmake -B build-test -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build-test -j8
unshare -Urn ctest --test-dir build-test        # phải ra 21/21 (đếm 24/09)
```

Bản gốc `dev` chạy 9 bài. Bản này 21: thêm hai bài kiểm ở nhóm D, bài kiểm của vá icon KDE ở
nhóm E (mã vá đã vào bản gốc, bài kiểm thì chưa), bài kiểm của vá icon GNOME, và các bài kiểm của vá
Messenger ở nhóm E.

Mỗi bài tự chép từ điển chính tả trong mã nguồn (`data/dictionaries/vietnamese.cm.dict`) vào thư mục
tạm riêng của nó (`e5dd085`). Trước đó bài kiểm lén đọc từ điển **đã cài trên máy**: máy có cài Lotus
thì xanh, hộp đóng gói sạch thì `super_smooth_settle_first_word` đỏ, vì thiếu từ điển bộ gõ coi "chư"
là chữ lạ và trả về "uw" khi gõ dấu cách. Đo 24/09 trong hộp Fedora 44, cùng một bản build: không có
file đỏ 5/5, có file xanh 5/5. Đây không phải lỗi giờ giấc, nới giờ chờ không chữa được.

## Hiệu năng so với bản gốc (chế độ uinput)

Bảng này cũng có ở trang đầu `.github/README.md`; sửa số thì sửa cả hai. Mã trong ngoặc là mục
trong `RADAR.md` của repo workbench, nơi ghi cách đo.

| Đo cái gì | Bản gốc → bản này | Điều kiện |
| --- | --- | --- |
| Thay một từ, từ lúc gửi phím xoá tới lúc gõ chữ mới: xoá 2 / 3 / 4 chữ | 14,5 / 21,8 / 31,2 → 12,4 / 16,3 / 20,5 ms (trung bình hai lượt) | Konsole, Smooth, 8/8 mỗi bản, bản gốc chạy cạnh bản mình cùng khung đo, 12/09/2026. Bản mình lúc đó chưa gom: khoảng cách phím xoá 2 ms, còn vá #489. Chỉ số này không tính thời gian bên trong máy chủ bàn phím ảo (B43ac) |
| Khoảng cách giữa hai phím xoá, trung vị | 5,2 → 0,08–0,10 ms | Konsole, ô soạn Edge, Firefox hồ sơ riêng, Edge qua XWayland, 8/8; mức 2 ms là 2,18–2,21, mức 1 ms là 1,18–1,21 (B43ab, B43af, B43ai, B43aj) |
| Chờ vô ích ở app không khai surrounding text, trung vị | 12,8 → 6,9 ms (xoá 4 chữ: 21,2 → 14,9 ms) | Edge qua XWayland, 16/16 lượt vào đúng nhánh vá; app Wayland thuần 0/16 nên không đổi (B43ae bị B43ag lật) |

**Không cộng dồn các dòng trên:** mỗi dòng đo một thay đổi, ở bản dựng và app khác nhau. **Chưa có
phép đo trọn vẹn** so nhánh `ban-dung` hiện tại (khoảng cách 0 ms) với bản gốc.

**Lời khai kèm:** từng lần chênh vài mili giây, dưới ngưỡng cảm nhận. Mức 0 ms là lựa chọn riêng của
máy này (đề xuất cho bản gốc là 2 ms); bàn phím ảo trong bộ đo gõ khoảng 25 ms một phím nên chưa bao
giờ chạm trường hợp phím xoá chen vào lúc người gõ nhanh. Thay `sleep_for` bằng hẹn giờ ở chế độ
Smooth (`f5d97db`) chưa đo riêng; số nền của bản gốc là 4% phím làm fcitx5 đứng ≥ 8 ms, lâu nhất
22,2 ms, trên 428 phím (B33). Super Smooth và Smooth chưa từng đo khác nhau về thời gian (B43v);
"Super Smooth gõ tốt hơn" là chủ máy gõ tay.

## Nhóm A — lỗi gặp thật, đã báo, tác giả từ chối vá

Hai miếng này tác giả đã đóng issue mà không sửa mã, và cả hai vẫn đứng vững. Mục thứ ba bên
dưới là một miếng vá **đã bị rút lại** vì mình báo sai — giữ lại để khỏi ai làm lại.

### ĐÃ RÚT LẠI — `769caad` máy chủ đừng tự bật chạm-để-bấm (issue #494)

**Miếng vá này đã bị gỡ khỏi `ban-dung`.** Giữ mục này lại làm bài học, đừng làm lại.

Mình báo issue #494 với tiền đề "máy mình tắt chạm-để-bấm", rồi viết thêm một bình luận phản biện
cũng dựa trên tiền đề đó. **Tiền đề đó SAI.** Chủ máy chưa bao giờ tắt chạm-để-bấm; mình tự suy
ra mà không hỏi. Tác giả upstream đúng ngay từ phản hồi đầu tiên.

Đo lại bằng hai dụng cụ, mỗi cái nhìn một ngữ cảnh:

| Nơi | chạm-để-bấm | tắt-khi-đang-gõ |
| --- | --- | --- |
| KWin, tức desktop thật (`busctl` hỏi `org.kde.KWin`) | BẬT, và mặc định cũng BẬT | BẬT |
| Ngữ cảnh libinput riêng dựng y như máy chủ, không ép gì | TẮT | BẬT |

Nên hai dòng ép bật tap trong máy chủ đang làm ngữ cảnh riêng **KHỚP** với desktop, chứ không
phải ghi đè lên nó. Gỡ chúng đi mới là làm Lotus lệch khỏi phần còn lại của máy: người dùng chạm
để dời con trỏ, hệ thống bấm thật, mà Lotus không reset từ đang gõ dở.

**Sai ở đâu, cụ thể:** số đo "16 cú bấm xuống còn 2" là đúng, nhưng mình rút ra kết luận sai từ
nó. Nó chỉ chứng minh **mặc định của THIẾT BỊ** là tắt tap. Nó không nói gì về **cài đặt hiệu lực
của người dùng**, thứ nằm trong ngữ cảnh riêng của compositor và không có cách nào suy ra từ ngữ
cảnh khác. Mình còn suy thêm một tầng nữa cũng sai: thấy `kcminputrc` không có khoá `TapToClick`
rồi kết luận là tắt, trong khi không có khoá nghĩa là **theo mặc định của KDE**, mà mặc định đó
là BẬT.

**Bài học, áp cho mọi lần sau:** cài đặt hiệu lực của người dùng thì **hỏi chủ máy hoặc hỏi
compositor**, đừng suy từ tệp cấu hình và đừng suy từ mặc định của thiết bị. Câu lệnh hỏi thẳng:

```
busctl --user get-property org.kde.KWin \
  /org/kde/KWin/InputDevice/eventN org.kde.KWin.InputDevice tapToClick
```

**Triệu chứng gốc vẫn chưa có lời giải chắc chắn.** Chủ máy báo gõ `Nguyễn Trãi` trong Lark ra
`Nguyễn Traix`, chập chờn. Giả thuyết còn lại mạnh nhất: Lark chạy trong Firefox, mà lúc đó luật
đặt `firefox=3` tức Super Smooth — chế độ uinput duy nhất bị tắt lá chắn chống nhân đôi chữ. Nếu
vậy thì touchpad vô can từ đầu. **Chưa kiểm chứng.** Lưu ý: từ vá lá chắn thanh địa chỉ ở nhóm E,
Lark vẫn chạy Super Smooth KHÔNG có lá chắn (bộ lọc cố ý loại ô Lark), nên nếu `Traix` quay lại
thì giả thuyết này đứng, còn nếu không quay lại thì nó yếu đi.

### `7f3ff67` — bỏ chờ retry vô ích ở app không có surrounding text

**Nguyên nhân:** đường uinput chờ thêm 6 ms để app gửi lại surrounding text, ngay cả với app đã
tự khai là không có. Cái chờ đó không thể thành công.

**Đo:** trên Edge chạy qua XWayland, độ trễ trung vị 12,8 → 6,9 ms, 16/16 lượt đi vào đúng
nhánh được vá.

**Upstream:** [issue #490](https://github.com/LotusInputMethod/fcitx5-lotus/issues/490) — đóng
kiểu NOT_PLANNED.

**Ghi chú quan trọng:** lần đo ĐẦU kết luận sai là "không đổi gì", vì bộ đích chỉ có app Wayland
thuần. App Wayland luôn khai có surrounding text nên không bao giờ đi vào nhánh này. Phải đo
trên app qua XWayland mới thấy. Đừng lặp lại lỗi đó.

### ĐÃ THAY BẰNG BẢN GỐC — `5fa19be` nhật ký xả đĩa mỗi dòng, không chỉ khi có cảnh báo

**24/09/2026:** bản gốc bỏ hẳn tệp nhật ký, máy chủ ghi qua `syslog()` (`744201d`); fork lấy theo nên
vá này không còn. Phần dưới giữ làm lịch sử.

**Nguyên nhân:** bản gốc chỉ xả đĩa khi mức log từ WARN trở lên. Mọi dòng quan trọng lúc khởi
động đều là mức INFO, nên nhật ký đứng yên trong khi máy chủ vẫn chạy bình thường. Người đọc
không phân biệt được nhật ký cũ với máy chủ đã chết.

**Giá phải trả:** vài dòng mỗi phút, không đo được.

**Upstream:** [issue #468](https://github.com/LotusInputMethod/fcitx5-lotus/issues/468) — đóng
kiểu NOT_PLANNED.

## Nhóm B — đang chờ tác giả trả lời

Hai commit, gửi chung ở [PR #492](https://github.com/LotusInputMethod/fcitx5-lotus/pull/492)
(bản nháp). Liên quan hai issue còn mở, #487 và #488.

- **`208d1b3`** — chờ SỰ KIỆN surrounding text thay vì ngủ theo một hằng số đoán trước.
  **Mặc định TẮT**, phải bật trong cấu hình mới có tác dụng.
- **`f5d97db`** — ở nhánh dự phòng của chế độ Smooth, chờ app bằng hẹn giờ thay vì `sleep_for`.
  `sleep_for` chặn vòng lặp sự kiện của cả fcitx5, không riêng Lotus.

**Khe hở đã khai thẳng trong PR, đừng rút lại:** nếu người dùng đổi cửa sổ trong vòng 50 ms thì
ô cũ có thể mất chữ. Cửa sổ quá hẹp nên thực tế không gặp.

## Nhóm C — dọn dẹp và hạ tầng, chưa gửi upstream

Vá bỏ phụ thuộc X11 từng nằm ở đây đã vào bản gốc, nên không còn trong `ban-dung`.


- **`5563540` siết cứng dịch vụ systemd.** `systemd-analyze security` từ 7.0 MEDIUM xuống 2.0
  OK, và đã cài chạy thật để chắc dịch vụ không hỏng. Hai chỉ thị cố ý KHÔNG bật vì cả hai đều
  làm hỏng dịch vụ: `PrivateNetwork` (udev gửi sự kiện qua netlink, netlink theo từng không gian
  mạng) và `ProtectProc` (cổng xác thực phải đọc `/proc/<pid>/exe` của tiến trình thuộc người
  dùng khác). Lưu ý `PrivateTmp` làm nhật ký chuyển vào `/tmp/systemd-private-*/`, đọc phải có
  quyền root và đường cũ ngừng cập nhật.
- **Máy chủ kiểm con số nhận từ mô-đun (26/09, issue #463).** Chỉ nhận đúng một số nguyên từ 1 tới
  1024 (phím xoá) hoặc từ -1 tới -1024 (bôi đen); số khác bị bỏ qua kèm cảnh báo. Đọc mã thì
  trước đó một số âm khổng lồ khiến máy chủ xin bộ nhớ cho hàng trăm triệu phím và sẽ chết, kéo
  bàn phím chết theo (chưa chạy thử đầu-cuối). Đã soi 7
  chỗ mô-đun gửi: không chỗ nào gửi 0. Bài `server_key_request` đỏ 4 ca khi gỡ giới hạn.
- **`4a54d17` gỡ `FixUinputWithAck`, cờ Chromium và tệp `src/ack-apps.h`.** Công tắc này vốn mặc
  định TẮT nên gỡ đi hành vi không đổi. Đã soi cả 5 chỗ dùng để chắc mỗi chỗ đặc biệt hoá đúng
  cho nhánh tắt.
- **`44433d6` lấy đường dẫn máy chủ từ CMake thay vì viết cứng `/usr/bin`.**
  ⚠️ **Gửi lẻ commit này lên upstream sẽ làm gói Nix GÃY**, vì Nix dùng
  `substituteInPlace --replace-fail` trên đúng chuỗi literal đó. Phải gửi kèm bản sửa tệp Nix
  trong cùng một PR.
- **`94dd9f1` biến môi trường `LOTUS_SERVER_PATH`** để chỉ định máy chủ mong đợi. Thiếu biến này
  thì mô-đun từ chối socket chuột, và tính năng bấm chuột ngắt từ chết âm thầm.
- **`07cca04` máy chủ hiểu `LOTUS_SOCKET_NAMESPACE`** giống mô-đun. Nhờ vậy chạy được một cặp
  mô-đun + máy chủ riêng bên cạnh bản đóng gói sẵn, không giẫm chân nhau.
- **`7795460` núm vặn `LOTUS_BACKSPACE_GAP_MS`** để đo nhịp gửi phím xoá.
- **Gộp ba chế độ uinput thành một chế độ `Uinput` (24/09).** Chạy đúng như Super Smooth: chờ 4 ms
  mỗi phím xoá, bỏ lá chắn tự điền trừ thanh địa chỉ. Người đang dùng Slow mất mức chờ 8 ms. Cấu hình
  cũ tự chuyển khi nạp (tên trong `lotus.conf`, số `1`/`3` trong luật theo app, tên trong
  `ModeOrder`); bài `uinput_mode_migration` đỏ đúng 5 chỗ trên mã cũ. Bỏ tuỳ chọn
  `ShowModeSmooth`, `ShortcutSmooth`, `ShowModeSuperSmooth`, `ShortcutSuperSmooth`; phím tắt mặc
  định của `Uinput` thành `1`. Không gửi upstream: bản gốc giữ ba chế độ. Sửa 26/09: `ModeOrder` cũ
  đổi ba tên thành ba chữ `Uinput` nên bảng chọn chế độ hiện `Uinput` ba lần; nay bỏ tên trùng
  ngay lúc nạp, bài `uinput_mode_migration` đỏ đúng 2 chỗ mới trên mã trước đó.
- **`42d7529` khoảng cách phím xoá mặc định 0 ms thay vì 5.** Đo `khoang_cach_xoa.py macdinh`:
  0,10 ms, 8/8 trên bốn đích. **Đây là lựa chọn riêng của máy này, không phải đề xuất cho
  upstream** — mức đề xuất cho upstream là 2 ms, vì mức 0 bỏ hẳn yêu cầu khe im lặng. Dấu hiệu
  DUY NHẤT để quay lại mức 2 là **sót chữ hoặc thừa chữ khi gõ nhanh**, không phải cảm giác
  nhanh chậm: chênh 0 với 2 chỉ khoảng 6 ms mỗi lần xoá 4 chữ, dưới ngưỡng cảm nhận.

## Nhóm D — bộ kiểm thêm vào

Sáu commit, cộng một vá hạ tầng kiểm (`e5dd085`, từ điển, xem mục "Dựng và chạy bộ kiểm").

- **`1ed7339`** bài kiểm bất biến trên chuỗi phím ngẫu nhiên.
- **`581fa75`** đối chứng dương cho các bất biến P2 đến P5, tức chứng minh bài kiểm THẤY được
  lỗi khi lỗi có mặt, chứ không phải xanh vì nó không kiểm gì.
- **`098e89f`** tái hiện vòng lặp giữ phím của issue #472.
- **`585b529`** mô hình đúng phím thô mà cửa sổ nhận được ở chế độ Smooth.
- **`d4082ee`** tách socket riêng cho `smooth_buffered_key_replay` để hai bài kiểm không giẫm
  chân nhau.
- **`028ae25`** hoà giải các nhánh đã gộp với bộ khung kiểm hiện tại của `dev`.

## Nhóm E — sửa lỗi gặp thật, chưa gửi mã lên upstream

### Super Smooth có lá chắn chống lặp chữ, nhưng CHỈ ở thanh địa chỉ trình duyệt

**Triệu chứng:** ở chế độ Super Smooth, gõ tiếng Việt vào thanh địa chỉ bị lặp chữ đầu đúng kiểu
issue #190 (gõ `tôi` ra `toôi`), trên cả Firefox lẫn Edge. Chế độ Smooth không lặp, nhưng chủ máy
gõ thử thấy Super Smooth tốt hơn ở gần như mọi chỗ khác.

**Nguyên nhân, trong mã gốc:** hai chế độ chỉ khác nhau đúng một chỗ. `performReplacement` bọc lá
chắn `isAutofillCertain` bằng `realMode != LotusMode::SuperSmooth`, tức Super Smooth bỏ lá chắn ở
mọi ô.

**Vá:** Super Smooth vẫn bỏ lá chắn, trừ khi ô đang gõ là thanh địa chỉ trình duyệt:

- **Edge/Chromium** tự khai `CapabilityFlag::Url` cho thanh địa chỉ, và báo đúng phần tự điền
  đang bôi đen.
- **Firefox** không khai cờ đó, và báo cho bộ gõ là KHÔNG bôi đen dù trên màn hình có tô. Nhận ra
  theo hình dạng: sau con trỏ có chữ, không xuống dòng, và toàn ký tự của địa chỉ web. Ở đó lá chắn
  bắn khi phép đoán gốc đồng ý, **hoặc** khi phần trước con trỏ đúng bằng chữ đang gõ (chữ đầu tiên
  của ô). Nhánh thứ hai cần vì phép đoán gốc so với một độ dài tự đếm, mà độ dài đó giữ số của lần
  gõ trước sau khi xoá trắng ô. Phím xoá thừa ở đầu ô không xoá gì nên đoán sai cũng vô hại.

**Đo 13/09/2026** bằng dòng ghi tạm, chỉ số đếm, không lưu chữ, đã gỡ khỏi bản dùng:

| Ô | Lá chắn được xét | Kết quả chủ máy gõ |
| --- | --- | --- |
| Thanh địa chỉ Edge | có, nhờ cờ Url; 6/6 lần có gợi ý thì bắn | hết lặp |
| Thanh địa chỉ Firefox | có, nhờ hình dạng đuôi; 2/2 lần có gợi ý thì bắn | hết lặp |
| Lark trong Firefox, ô web Edge, Alacritty | không, Super Smooth nguyên bản | ổn |

**Bẫy đã gặp, đừng nới bộ lọc:** bản đầu chỉ cấm khoảng trắng sau con trỏ. Lark luôn giữ 3 ký tự
vô hình sau con trỏ nên lọt qua, và đã bắn lá chắn nhầm một lần trong Lark. Bộ lọc theo tập ký tự
địa chỉ web loại cả 19/19 lần thay chữ ở Lark.

**Bài học đo:** bảng trên lúc đầu chỉ có 2 lần Firefox có gợi ý. Dùng thật thì còn lặp khoảng 27%
(bộ đo tự đọc lại ô sau mỗi lần sửa dấu: 6/22 lần lặp, 5 trong số đó do độ dài tự đếm cũ). Sau khi
thêm nhánh "chữ đầu tiên của ô": 7/7 lần có gợi ý ra đúng, 0 lặp, 0 mất chữ, chủ máy gõ thật báo
hết lặp. Mẫu vẫn nhỏ.

**Còn sót có thể:** nếu Firefox chưa kịp báo phần tự điền lúc bộ gõ quyết định thì không nhánh nào
nhận ra. Lượt đo đầu gặp 1/22 lần, lượt sau không gặp.

**Cần luật:** `firefox=3`, `microsoft-edge=3` (xem mục cấu hình bên dưới).

**Lỗi lặp chữ đầu ở thanh địa chỉ Chromium (`eê`, `toôi`) với chế độ uinput: đã sửa triệt để.**
Chromium báo đúng phần tự điền đang bôi đen, nên ở Edge lá chắn đi theo cơ chế chắc chắn, không phải
phép đoán. Chủ máy xác nhận ngày 13/09/2026. Chế độ Surrounding Text **không** được sửa: ở chế độ đó
lỗi này vẫn là lỗi của Chromium (đã báo Chromium số 557316480), xem mục "không sửa".

### Icon chữ V màu đen trên panel tối của KDE (issue #374)

**Triệu chứng:** trên KDE Plasma, màu icon để `Auto`, chữ V ở khay hệ thống màu đen nằm trên panel
màu đen, gần như không thấy. Gặp ngay với giao diện mặc định của Fedora 44.

**Nguyên nhân, trong mã gốc:** `isDarkMode()` hỏi portal (`org.freedesktop.appearance
color-scheme`). Trên KDE, portal trả lời theo màu cửa sổ ứng dụng (`kdeglobals`), còn panel do
Plasma Style (`plasmarc`) tô. Giao diện mặc định Fedora đặt `ColorScheme=BreezeLight` nhưng Plasma
Style là `breeze-dark`, ghi ở `~/.config/kdedefaults`. Portal trả 2 (sáng) nên Lotus chọn icon đen.

**Vá:** tệp mới `src/lotus-plasma-theme.cpp`. Khi `XDG_CURRENT_DESKTOP` có `KDE`, Lotus đọc thẳng
tệp cấu hình, không gọi tiến trình con:

- Lấy tên Plasma Style theo thứ tự đè của KConfig: thư mục người dùng, rồi `kdedefaults`, rồi
  `/etc/xdg`.
- Giao diện có tệp `colors` riêng thì lấy màu nền cửa sổ trong đó. Không có (Breeze `default`) thì
  theo bảng màu hệ thống.
- Ngưỡng tối/sáng giống portal KDE: độ xám dưới 192 là tối. Đọc không ra thì rơi về cách cũ.

**Đo 14/09/2026:**

- Bài kiểm `plasma_panel_theme`, 15 trường hợp. Khai trước là bản rỗng phải đỏ đúng 12 bài (P1–P9,
  S1, E1, E2): ra đúng 12. Mã thật 15/15 xanh, cả bộ 12/12.
- Máy thật Fedora 44 KDE Wayland, chụp khay trước và sau khi cài: V đen thành V trắng.

**Bẫy đo đã gặp:** `strings liblotus.so | grep desktoptheme` ra 0 dù mã đã vào, vì trình biên dịch
nhét chuỗi ngắn thẳng vào lệnh máy. Tìm theo tên hàm bằng `nm -C` mới đúng (đối chứng: tệp kiểm
thấy 2).

**Chưa kiểm:** Kubuntu, Plasma Style của bên thứ ba, đổi giao diện khi Lotus đang chạy (icon đổi ở
lần khay cập nhật kế tiếp, kết quả lưu tạm 5 giây).

**Đã vào bản gốc:** PR #497 được gộp vào `dev` (commit gộp `79d5706`). Khi gom lại 16/09/2026, phần
mã lấy nguyên của bản gốc; bản này chỉ còn giữ bài kiểm `plasma_panel_theme` (`44c02e4`), vì bản gốc
nhận mã mà không mang bài kiểm.

### LibreOffice: chế độ uinput xoá bằng surrounding text thay cho phím xoá (`a1dd658`)

**Triệu chứng:** gõ Telex trong LibreOffice Writer ở Smooth hay Super Smooth, chữ thay từ 2 ký tự
trở lên ra sai: `chao` + `f` thành `chaà`, `trương` + `f` thành `trươờn`. Thay 1 ký tự thì luôn đúng,
nên ai bỏ dấu sớm ít gặp. Đây là issue #162 của bản gốc, mở từ tháng 3/2026; tác giả cho là do máy
chậm.

**Nguyên nhân, trong mã LibreOffice:** Writer gán Backspace thành phím tắt `.uno:SwBackspace`, và mọi
phím tắt chạy **hẹn sau** (`svtools/source/misc/acceleratorexecute.cxx:223`,
`AsyncAccelExec::execAsync()`; master vẫn vậy). Chữ commit thì chèn ngay. Nên chữ mới vượt mặt những
phím xoá còn trong hàng, không phải do chậm.

**Đã loại bằng số đo:** Lotus gửi đủ phím xoá và commit tới sau 19 ms (dbus-monitor); bật
`FCITX_ENABLE_SYNC_MODE` vẫn sai 9–27/60; dùng `forwardKey` vẫn sai 21–22/60; chờ lâu hơn không cứu.

**Vá:** khi chương trình là `soffice` (Writer, Calc, Impress) và app khai có surrounding text, xoá bằng
`deleteSurroundingText`. Writer xoá ngay trên văn bản thật, tính từ con trỏ (`SwEditWin::
DeleteSurroundingText`). Chỉ dùng để xoá, không dựng lại trạng thái từ surrounding text, nên không dính
lỗi thiếu dấu cách của chế độ Surrounding Text. App khác giữ nguyên.

**Đo 16/09/2026** trên iMac Ubuntu 24.04 GNOME X11, LibreOffice 24.2.7, bàn phím ảo gõ giống người vào
đúng thiết bị bàn phím, 60 từ mỗi lượt, đếm từ sai. Bộ đo: `probes/x11/writer_tay.py` ở workbench.

| Ô | Trước | Sau |
| --- | --- | --- |
| Writer cuối câu, Smooth, 150 / 350 ms mỗi phím | 30 / 36 sai | 0 / 0 |
| Writer cuối câu, Super Smooth, 150 / 350 ms | 34 / 36 sai | 0 / 0 |
| Writer, 70 ms, cả hai chế độ; giữa câu; gõ đè phím | chưa đo | 0; 0; 0 |
| Calc, Impress, 150 ms | chưa đo | 0; 0 |

Chủ máy gõ tay trước khi vá: 2/20 từ sai. Sau vá chủ máy gõ thử thấy được.

**Bẫy đo đã gặp:** luật theo app (`soffice=3`) đè lên chế độ đặt qua D-Bus, nên mấy lượt tưởng là
Preedit/Slow thật ra chạy Smooth. Bộ đo mới sửa thẳng luật rồi đọc lại. Bộ đo đầu (đọc lại bằng AT-SPI,
đoạn văn dài) cũng thổi phồng lỗi so với gõ tay.

**Upstream:** bình luận vào issue #162 kèm nguyên nhân và số đo, chưa gửi PR.

### ĐÃ BỎ — máy chủ gửi phím xoá qua đúng bàn phím người dùng đang gõ (`c4dc98d`, issue #506)

**Gỡ khỏi `ban-dung` ngày 17/09/2026** bằng commit revert. Giữ mục này để khỏi ai làm lại.

**Lỗi có thật, nhưng chỉ trên GNOME X11:** gõ tiếng Việt làm cửa sổ GTK3 đứng 2–3 giây, vì mỗi lần phím
đổi qua lại giữa bàn phím thật và bàn phím ảo của Lotus thì gnome-shell nạp lại bảng phím (~300 ms).
Vá cũ ghi phím xoá thẳng vào `/dev/input/eventN` của bàn phím thật: 2048–2668 ms → 201 ms.

**Vì sao bỏ:** đo trên chính iMac đã chuyển sang GNOME Wayland (driver nouveau), tắt vá bằng
`LOTUS_BACKSPACE_VIA_DEVICE=0`: lần đứng lâu nhất ~204 ms, bằng lúc bật; đếm phím xoá qua node bàn
phím xác nhận đúng đường (420 khi tắt, 630 khi bật); đối chứng cho app đứng 1,5 s đo ra 1557 ms.
GNOME X11 đã bị bỏ ở các bản mới, máy này cũng không còn dùng X11, nên không đáng giữ thêm mã ghi vào
thiết bị thật. Còn hở: số X11 đo với driver nvidia 470, chưa đo X11 với nouveau.

### Máy chủ xử lý hết hàng sự kiện libinput mỗi vòng (`02b68d0`, issue #507)

**Nguyên nhân:** `libinput_dispatch()` chạy mọi vòng và rút cạn sự kiện vào hàng riêng, nhưng vòng rút
hàng chỉ chạy khi `poll()` báo có dữ liệu. Lúc đang gửi phím xoá, `poll()` hết giờ chờ mà không báo gì,
nên sự kiện nằm đó tới lần có phím kế tiếp. Lộ ra lúc làm vá #506 (đã bỏ, ở trên); vá này vẫn giữ vì
với tính năng bấm chuột ngắt từ thì cú bấm bị xử lý trễ.

**Vá:** rút hàng vô điều kiện ngay sau `libinput_dispatch()`.

**Bản gốc đã tự vá y hệt** ngày 18/09 (`c43c02b`). Lần lấy 24/09 giữ bản của fork, nhặt commit đó ra rỗng.

### Chờ 4 ms mỗi phím xoá ở Smooth và Super Smooth, thay vì 2 (`ae9d213`)

**Đo trên gnome-terminal X11, 60 câu mỗi mức:** máy rảnh thì 2 ms và 4 ms đều đúng hết. Máy tải nặng
(6 lõi bận ở `nice 15`, load khoảng 10): 2 ms đúng 39–46/60, 4 ms đúng 58–60/60, 8 ms 60/60. Máy tính
tiền kiêm chạy CI nên tải nặng là chuyện có thật. Đã ghi trong bình luận ở issue #506.

### Messenger trên Facebook mất chữ ở chế độ uinput (`60144f9`, `ab5b307`, vá bổ sung 19/09 ×2)

> **Cập nhật 20/09: đã bỏ được đồng hồ ở ô soạn tin Messenger.** Thay vì xoá trước rồi giao chữ sau,
> bộ gõ bôi đen phần cần bỏ bằng Shift+Left rồi gõ đè lên vùng chọn. Ô không lúc nào trống và không có
> khe hở giữa xoá và giao, nên hai nguyên nhân (2) và (4) mất chỗ bám; quan trọng hơn, Edge **xác nhận**
> vùng bôi đen nên bộ gõ chờ theo tín hiệu thật chứ không theo thời gian. Đo 20/09: 861 lần thay chữ,
> 0 lần mất chữ, xác nhận về sau 3–20 ms (trung vị 6), không lần nào chạm hạn. Đoạn dưới nói về đường
> cũ (`WaitSurroundingSettleMs`), vẫn giữ làm đường lùi.
>
> **Về ĐƯỜNG LÙI (chờ theo đồng hồ) — cách chữa tạm, không hết lỗi 100%, chủ máy chốt 19/09.** Nguyên nhân (1) và (3) là lỗi kiểm "đã xoá
> xong" của bộ gõ, đã vá dứt điểm. Nguyên nhân (2) và (4) nhiều khả năng nằm ở phía Facebook: ô soạn tin
> nuốt chữ tới lúc nó đang vẽ lại mà không phát tín hiệu nào, nên bộ gõ chỉ né được bằng cách chờ. Mức chọn
> (giữa câu 40 ms, từ đầu 60 ms) lúc máy bận còn sai khoảng 1/50 câu; chủ máy chấp nhận phần lọt đó là do
> các sự kiện của Facebook trong lúc chat, không tránh được, thay vì chờ lâu hơn ở mọi lần bỏ dấu.

**Triệu chứng:** gõ Telex trong ô soạn tin Messenger trên facebook.com, Edge, KDE Wayland, Super Smooth:
`tieengs vieetj` ra `iếngiệt`. Chữ có dấu vẫn đúng nhưng mất chữ phía trước (`t`, dấu cách, `v`). Lark
và ô soạn thảo Lexical trơn (cùng bộ soạn thảo Messenger dùng) không bị.

**Xuất phát từ đâu:** lỗi gốc có từ bản chính. Issue
[#267](https://github.com/LotusInputMethod/fcitx5-lotus/issues/267) mở ngày 06/05/2026 (bản 3.0.3),
nhiều người xác nhận; #268 bị đóng vì trùng, #302 (Messenger trên Chrome) bị đóng không lời. Lỗi chỉ
gặp trên các trang của Meta (Facebook, Messenger, Threads), ở cả Firefox lẫn họ Chromium. Tác giả cho là
Meta đổi cách xử lý nhập liệu, khuyên tạm dùng Uinput (Slow) hoặc Preedit. Issue vẫn mở. Bản fix tác giả
đẩy lên `dev` ngày 28–29/08 (`aaacb63`, `9d56be6`) là tuỳ chọn `useSurroundingTextIfPossible`, mặc định
tắt, chính tác giả ghi là "chưa ổn định"; máy này không bật nó.

Trên máy này, đoạn mã hỏng là của fork: đường chờ sự kiện surrounding text (`WaitSurroundingEvent`,
nhóm B), chỉ fork có. Fork cũng rút ngắn thời gian chờ so với bản gốc (khoảng cách phím xoá 5 → 0 ms,
bỏ chờ retry), nên nhiều khả năng lỗi dễ lộ hơn. **Chưa đo** bản gốc hay đường mặc định của fork
(`WaitSurroundingEvent=False`) trên Messenger, nên chưa nói được fork làm lỗi nặng thêm bao nhiêu.

**Nguyên nhân, đo bằng dòng log tạm in ảnh ô chữ Edge báo về (đã gỡ, nằm ở nhánh `do/messenger-edge`):**

1. **Ảnh nửa vời.** Mỗi phím xoá, Messenger báo con trỏ lùi trước, chữ trong ô xoá sau: `tie\n\n`
   con trỏ 2, vài ms sau mới `ti\n\n`. `deletionLooksDone` chỉ nhìn phần trước con trỏ nên tưởng đã xoá xong
   và giao chữ. Trang vứt chữ đó, rồi phím xoá của lần thay sau ăn vào chữ thật. Đo 17/09: 3/3 lần giao
   trên ảnh nửa vời mất chữ, 3/3 lần giao trên ảnh khớp vào đủ. Ô Lexical trơn không báo kiểu này: nó
   bôi đen chữ sắp xoá (con trỏ và neo lệch nhau) rồi mới xoá.
2. **Facebook vẽ lại ô soạn tin sau khi ảnh đã báo xoá xong.** Sửa (1) xong, chủ máy gõ tay ba lần vẫn
   mất chữ: 5/5 lần giao ở mốc 3–12 ms tính từ phím xoá cuối bị mất, 6/6 lần giao ở mốc 17 hoặc 51 ms
   vào đủ. Lần vẽ lại này không phát sự kiện surrounding text nào, nên chỉ chờ được theo thời gian. Khớp
   với nhận xét của kimxuanhong ở #267 ("timing chưa đúng, xoá xong cần đợi thêm rồi mới commit").

3. **Con trỏ nằm GIỮA phần cần xoá (nguyên nhân chính, tìm ra 19/09).** Vá (1) chỉ so chữ ngay sau con
   trỏ với chữ ĐẦU của phần cần xoá. Thay từ hai chữ trở lên (gõ `đ` là xoá cả từ: `do` → `đo`) thì
   giữa hai nhịp Edge báo `do\n\n` con trỏ 1, tức con trỏ đứng giữa `d` và `o`, chữ chưa xoá. Ảnh đó
   lọt qua vá (1), bộ gõ giao chữ sau 3–10 ms và mất chữ. Log 19/09 (bản khoanh vùng, chờ 80 ms): 4/4
   lần mất chữ đều là ảnh này, không lần nào kịp chờ. Câu thử `tieengs vieetj` chỉ thay 1–2 chữ nên
   ít gặp; câu nhiều chữ `đ` gặp ngay.
4. **Ô soạn tin vừa trống còn đang nạp lại (tìm ra 19/09, máy gõ tự động).** Sửa (3) xong vẫn mất chữ lác
   đác, gần như chỉ ở từ đầu tiên của tin nhắn: 32/36 lần mất nằm trong nửa giây sau khi ô trống. Gõ
   `dd` ở đầu tin là nặng nhất: xoá chữ `d` duy nhất làm ô trống hẳn, Facebook vẽ lại dòng gợi ý, chữ
   `đ` tới lúc đó bị nuốt. Chữ bị nuốt không bao giờ hiện trong ảnh, và không có dấu hiệu nào báo trước:
   nhiều lần ô đã im 21 ms rồi vẫn mất. Thử "chờ tới khi ô im N ms" (đếm lại mỗi lần ô báo đổi) không
   cứu được những lần đó nên bỏ. Chữ giao thành công cũng có lúc 40 ms mới hiện, nên không bắt được lỗi
   sau khi xảy ra để gửi lại mà không sợ chữ đôi.

**Vá:**

- `60144f9`: chữ ngay sau con trỏ vẫn là chữ đầu của phần cần xoá thì ảnh chưa xong, chờ tiếp.
- `ab5b307`: tuỳ chọn `WaitSurroundingSettleMs`. Ảnh báo xoá xong thì hẹn giao chữ sau N ms, dùng lại
  đúng trạng thái chờ hẹn giờ có sẵn (phím chen vào thì chờ nốt rồi giao). Mặc định 0, hành vi không đổi.
- Vá 19/09: (a) mở rộng vá (1) ra mọi vị trí con trỏ bên trong phần cần xoá: trước con trỏ là phần đầu
  cộng k chữ đầu của phần cần xoá, sau con trỏ là chữ thứ k ⇒ chưa xong. (b) Chỉ chờ lắng ở ô có hình
  ô soạn tin Messenger: sau con trỏ đúng `\n\n`, hoặc cả ảnh chỉ là `\n` (ô vừa bị xoá trống). Ô khác
  giao ngay như trước khi có `ab5b307`. Khoanh theo hình ảnh chứ không theo tên chương trình, để
  Messenger trong Chrome cũng được chữa còn Lark mở trong Edge không phải chờ. Khớp nhầm thì chỉ chậm
  thêm, không sai chữ.
- Vá 19/09 lần hai: tuỳ chọn `WaitSurroundingSettleFirstWordMs`. Ở ô soạn tin Messenger, khi trước con
  trỏ chưa có dấu cách hay xuống dòng (đang gõ từ đầu tiên) thì chờ lắng theo mức này thay cho
  `WaitSurroundingSettleMs`. Các từ sau giữ mức thường. Mặc định 0, hành vi không đổi.

**Đo:** máy Fedora 44, KDE Wayland, Edge 153 flatpak, Super Smooth, `WaitSurroundingEvent=True`, chủ máy gõ
tay. Các dòng 17/09 đo trên bản CÒN lỗi (3), nên con số mức chờ ở đó không đáng tin:

| Ô | Bản | Kết quả |
| --- | --- | --- |
| Messenger, chỉ vá (1) | 17/09 | vẫn mất chữ |
| Messenger, `WaitSurroundingSettleMs=15`, bản có log đo | 17/09, còn lỗi (3) | gõ đúng một lượt |
| Messenger, `=15` / `=25` / `=40`, gói RPM | 17/09, còn lỗi (3) | mất chữ / sai ~1/10 / đúng, sai ~1/50 lúc máy bận |
| Messenger, khoanh vùng, `=80` | 19/09, còn lỗi (3) | 4 lần giao ở mốc 3–10 ms, mất chữ |
| Messenger, vá 19/09, `=0` | 19/09 | mất chữ; giao ở mốc 4–17 ms |
| Messenger, vá 19/09, `=4` | 19/09 | mất ngay chữ đầu; giao ở mốc 9–22 ms |
| Messenger, vá 19/09, `=20` (hai lượt, có lúc máy bận, gõ nhanh) | 19/09 | 350 lần thay chữ, 0 lần giao sớm, không thấy dòng sai; giao ở mốc 24–43 ms |
| Ô thường (không có `\n\n`), vá 19/09, `=20` | 19/09 | giao khi ô báo xoá xong, không chờ thêm |
| Ô Lexical trơn, bàn phím ảo, 10 lượt, 50 ms/phím, chỉ vá (1) | 17/09 | 10/10 |

**Đo bằng máy gõ tự động (19/09):** bàn phím ảo gõ vào ô soạn tin Messenger (chat với chính mình, không
bấm Enter), mỗi lượt xoá trắng ô bằng Ctrl+A rồi Backspace; chấm bằng ảnh cuối Edge báo về (bản có dòng
log tạm in ảnh, không phát hành). Kịch bản ở `LotusVibe-logs/trang-thu/do_messenger.py`, chế độ `TU_DAU=1`
chỉ gõ một từ đầu tiên.

| Bản | Câu đầy đủ | Chỉ từ đầu tiên |
| --- | --- | --- |
| một mức chờ, `=20`, máy rảnh | sai 1/100 (`chữ` → `cữ`) | |
| một mức chờ, `=20` | | sai 30/156 |
| một mức chờ, `=40` | | sai 4/100 |
| một mức chờ, `=60` / `=100` | | sai 0/100 / 0/100 |
| từ đầu `=60`, giữa câu `=20`, máy bận | sai 8/50 | sai 0/100 |
| từ đầu `=60`, giữa câu `=40`, máy bận | sai 1/50 | |
| từ đầu `=60`, giữa câu `=60`, máy bận | sai 0/50 | |

"Máy bận": tải trung bình 5–8 (một tiến trình `gh` ăn 350% CPU, máy chạy CI, máy chủ Next.js). Cùng mức 20 ms,
máy rảnh mất 1/1320 lần thay chữ, máy bận 8/50 câu: chữ giữa câu mất hay không tuỳ máy có bận không.

**Mức chờ chọn: giữa câu 40 ms, từ đầu 60 ms (chủ máy chốt 19/09):** 0 và 4 ms mất chữ, nên nguyên nhân (2) có thật. Ranh giới quanh mốc 17 ms từ lúc
bắn phím xoá (17/09: giao trước 12 ms mất, từ 17 ms vào). 20 ms đẩy lần giao sớm nhất lên 24 ms. Hạ tiếp
chỉ lợi vài ms mỗi lần thay chữ, mà chỉ ô Messenger phải chờ. Từ đầu tiên cần 60 ms (40 ms còn sai 4/100).
Giữa câu 20 ms đủ lúc máy rảnh nhưng máy bận thì sai 8/50 câu, 40 ms còn 1/50, 60 ms 0/50. Chủ máy
chọn 40 ms: phần còn lọt coi là do sự kiện của Facebook trong lúc chat, không tránh được; chờ 60 ms ở mọi
lần bỏ dấu không đáng. Từ đầu giữ 60 ms nhờ `WaitSurroundingSettleFirstWordMs`, vì ở 40 ms từ đầu còn
sai 4/100.

**Bẫy thước đo phụ:** đếm "chữ có hiện trong ảnh trước lần thay kế tiếp không" chỉ đúng khi mức chờ ngắn.
Ở 60 ms phím kế tiếp thường tới trong lúc chờ nên ảnh xác nhận ra sau mốc đó, thước báo mất 36/660 lần
thay chữ trong khi cả 50 câu đều đúng. Chấm bằng chữ cuối cùng trong ô.

**Bẫy đo riêng của vá này:** tăng mức chờ mỗi lần thấy lỗi thì lần nào cũng "đỡ hơn", vì chờ lâu cho Edge
thêm thời gian báo ảnh đúng nên ít gặp lỗi (3). Đó là che triệu chứng: ở 80 ms vẫn mất chữ vì bộ gõ không
chờ chút nào. Muốn biết mức chờ đúng phải soi ẢNH lúc giao, không nhìn số dòng sai. Giả thuyết 17/09 "log
làm chậm nên 15 ms trên bản có log mới đúng" không cần nữa: lỗi (3) ra ngẫu nhiên theo nhịp Edge.

ctest 20/20. `super_smooth_settle_first_word` (19/09 lần hai): từ đầu tiên chưa được giao sau 45 ms khi
mức từ đầu là 80 (đỏ đúng bước này trên mã trước), từ thứ hai phải giao trong 45 ms (đỏ đúng bước này khi
coi mọi từ là từ đầu). Bài mới 19/09: `super_smooth_half_updated_mid_word` (đỏ trên mã cũ ở đúng bước ảnh `d|o`),
`super_smooth_settle_plain_field` (ô thường phải giao trong 15 ms; đỏ khi còn chờ mọi ô),
`super_smooth_settle_empty_composer` (ô trống `\n` vẫn chờ; đỏ đúng bài này khi gỡ dòng nhận `\n`).

**Còn hở:** chưa đo Firefox và Chrome, chưa đo đường `WaitSurroundingEvent=False` (chỉ so vị trí con trỏ nên
có thể mắc đúng lỗi (1) và (3)). Chưa kiểm ô soạn thảo khác trên Chromium có hình `\n\n` (Gmail, Zalo web)
để biết có khớp nhầm không.

**Bẫy đo đã gặp:** trang thử đo bằng bàn phím ảo từng cho 0/10 và 1/10 mà không phải lỗi bộ gõ. Cửa sổ
Edge mới chưa bật Lotus. Trang tự xoá ô nên Lotus còn nhớ từ lượt trước (phải bấm một phím di chuyển con
trỏ giữa các lượt). Phím Esc làm ô Lexical mất chọn. Bộ đo (trang Lexical tự ghi sự kiện, máy chủ nhỏ,
kịch bản chỉ gõ khi trang đang được chọn) chưa đưa vào workbench.

### Bôi đen rồi gõ đè trong ô soạn tin Messenger (20/09)

**Ý tưởng:** không xoá rồi giao nữa. Bắn Shift+Left cho đủ số chữ cần bỏ, chờ ảnh báo vùng chọn đúng độ
dài (`anchor` lệch `cursor` đúng N), rồi `commitString` đè lên vùng chọn.

**Vá:**
- Máy chủ uinput nhận **số âm** trên cùng socket = bôi đen |n| chữ: giữ `KEY_RIGHTSHIFT` (Shift phải, xem dưới), bắn `KEY_LEFT`
  n lần, nhả Shift, tất cả trong một lần ghi. Phải đăng ký thêm hai mã phím đó với `UI_SET_KEYBIT`.
- Addon: `MessengerSelectOvertype` (mặc định bật từ 26/09), chỉ áp cho ô có hình ô soạn tin Messenger.
- Phím Shift+Left do chính mình bắn quay lại fcitx: phải `forward()` thẳng tới ứng dụng và KHÔNG được
  coi là người dùng di con trỏ.
- "Van an toàn" cũ (`current_backspace_count_ >= expected_backspaces_` ở phím kế tiếp) phải bỏ qua khi
  đang chờ vùng chọn: đường này không gửi phím xoá nào nên van luôn thấy "đủ" và tắt trạng thái ngay.
- Hết hạn 150 ms mà ô không xác nhận: **không gõ đè** (con trỏ đang lùi N chữ, gõ vào là chữ chèn sai chỗ
  và rối), trả con trỏ về rồi bỏ lần thay đó — mất dấu một chữ, thấy ngay.

**Đo (máy gõ tự động, Edge, tải máy 5–7, 20/09):**

| Bản | Chỉ từ đầu tiên | Câu đầy đủ |
| --- | --- | --- |
| chờ đồng hồ 40 ms, từ đầu 60 ms | sai 0/100 | sai 1/50 |
| bôi đen rồi gõ đè | sai 0/100 | sai 0/50 |

861 lần thay chữ, 0 lần chạm hạn; xác nhận về sau 3 ms (nhanh nhất), 6 ms (trung vị), 20 ms (chậm nhất).

**Vì sao chỉ Messenger:** bật cho mọi ô thì hỏng ngay — các ô khác có khai chữ chung quanh nhưng không
khai lại khi chỉ bôi đen, nên 150 ms trôi qua không một tin nào và mọi chữ đều mất dấu (đo 20/09, đường
lùi hoạt động đúng: mất dấu chứ không rối chữ). Suy đoán chưa kiểm: trình duyệt vốn phải theo dõi vùng
chọn cho JavaScript của trang nên khai sẵn, còn ứng dụng thường chỉ khai khi chữ đổi.

**Kiểm thử:** `super_smooth_messenger_select_overtype` — ô Messenger phải gửi số âm thay vì phím xoá (đỏ
trên mã trước ở đúng bước này); không được gõ khi chưa có xác nhận, kể cả khi ô báo ảnh khác trong lúc chờ
(đỏ khi nhận bừa mọi tin); hết hạn thì không gõ và trả con trỏ (đỏ khi vẫn gõ đè); ô thường vẫn gửi phím
xoá. ctest 21/21.

**Cài đặt:** phần bôi đen nằm ở máy chủ nền, nên gói mới phải kèm khởi động lại
`fcitx5-lotus-server@<user>`. Addon mới nói chuyện với máy chủ cũ thì số âm bị hiểu sai (máy chủ cũ bắn
một phím xoá lạc); addon cũ với máy chủ mới thì không sao.

**Shift phải, không phải Shift trái (23/09).** Bản đầu giữ Shift trái. Dùng thật thì thỉnh thoảng đang
gõ trong ô Messenger, bộ gõ tự rơi về tiếng Anh, phải bấm phím tắt bật lại. Nguyên nhân: fcitx5 mặc định
có `AltTriggerKeys=Shift_L` — thấy Shift trái nhấn rồi nhả mà ở giữa không có phím nào khác thì chuyển
tạm sang bộ gõ đầu tiên (`keyboard-us`). Mã fcitx5 5.1.22 (`instance.cpp`, watcher pha `InputMethod`)
chỉ không chuyển nếu nó thấy phím mũi tên ở giữa; nghi có lúc mũi tên không đi qua fcitx5 (chưa tái
hiện được theo ý muốn). Shift phải không nằm trong danh sách phím chuyển mặc định nên không thể dính,
bất kể mũi tên đi đường nào. Người đã tự đặt Shift phải làm phím chuyển thì vẫn dính.

Đối chứng trên máy (bàn phím ảo, ô soạn tin Messenger trên Edge đang được chọn): chạm Shift trái một
mình ⇒ trạng thái 2 → 1 (rơi về tiếng Anh); chạm Shift phải ⇒ 2 → 2. Lần đầu chạy khi không ô nào được
chọn thì cả hai đều 2 → 2 — phép thử mù, không tính. Máy gõ 50 câu với máy chủ Shift phải: bôi đen xác
nhận 659/660 lần, bộ gõ ở tiếng Việt suốt lượt; sai 1/50 do lỗi vứt phím dưới đây, không do Shift.

**Hết hạn thì trả lại phím gõ trong lúc chờ (23/09).** Đo 23/09, câu 32: Facebook chậm tới mức chữ `c`
vừa gõ chưa hiện, ô không báo vùng chọn trong 150 ms ⇒ đường lùi bỏ lần thêm dấu (`đươc`, đúng thiết kế),
nhưng `abandonOvertype()` xoá luôn `buffered_keys_` nên dấu cách gõ trong 150 ms đó mất theo (`đươcđêm`).
Lượt 20/09 không có lần chạm hạn nào nên chưa lộ. Vá: đường lùi gọi `replayBufferedKeys()` sau khi trả con
trỏ, như đường xác nhận. Phím bỏ dấu nằm trong hàng thì ra chữ thường, vì bộ gõ đã quên từ trước đó.

Kiểm: `super_smooth_messenger_select_overtype` gõ dấu cách và `d` trong lúc chờ một vùng chọn không bao
giờ được xác nhận, đòi sau `ư` phải ra đúng `" d"`. Trên mã cũ đỏ đúng bước đó (`commits=['ư']`); đặt lại
`buffered_keys_.clear()` cũng đỏ đúng bước đó. ctest 21/21. Máy gõ 50 câu: sai 0/50, 660/660 vùng chọn
được xác nhận — lượt này không có lần chạm hạn, nên đường trả phím mới chỉ được bài kiểm chứng minh.

**Ô đăng bài Facebook, sửa giữa đoạn (24/09).** Dán một đoạn vào ô đăng bài rồi gõ "đây là bản fork"
chèn trước một dòng có sẵn thì ra "ây l bn fork". Log có ghi ảnh ô: sau con trỏ còn cả phần bài phía
sau, nên hình "chỉ có `\n\n` ngay sau con trỏ" không khớp, bộ gõ đi đường xoá rồi giao. Facebook báo
xoá xong, chữ giao 4–5 ms sau thì mất (`đ`, `ả`, `à`, `đi`, `á`). Gõ ở cuối cùng bài đó thì khớp hình,
đi đường bôi đen, Edge báo vùng chọn sau 7 ms và chữ vào đúng. Nên ô này **có** báo lại khi bôi đen,
chỉ là không được nhận ra. Trên 5.000 ảnh của ô soạn Facebook trong log, ảnh nào cũng kết thúc bằng
`\n\n` dù con trỏ ở đâu (2787 giữa đoạn, 2207 cuối đoạn); 27 ảnh không khớp đều là danh sách tin
nhắn. Vá: quyết định bôi đen nhận ô theo **đuôi cả ô** (`looksLikeFacebookComposer`), không theo phần sau
con trỏ. Mức chờ lắng theo giờ vẫn giữ hình cũ, không nới. Bài kiểm thêm bước "ô đăng bài, giữa
đoạn": mã cũ đỏ đúng bước đó (máy chủ nhận 2 thay vì -1); cho hàm nhận mọi ô thì đỏ ở bước "ô
thường vẫn xoá bằng phím xoá". Chưa đo trên máy gõ tự động.

**Upstream:** đã báo cách khắc phục ở #267 (17/09; cập nhật 19/09: nguyên nhân (3) và (4), đề xuất 60 ms; fork tự dùng 40 ms giữa câu, 60 ms từ đầu; 20/09: bình luận mới về cách bôi đen rồi gõ đè), chưa gửi mã.

### Icon chữ V màu đen trên thanh trên cùng của GNOME

**Triệu chứng:** iMac Ubuntu 24.04, GNOME 46 X11, màu icon để `Auto`: chữ V trên thanh trên cùng màu
đen, gần như không thấy. Cùng họ với lỗi KDE ở trên.

**Nguyên nhân:** vá KDE chỉ chạy khi desktop là KDE. Trên GNOME, Lotus vẫn hỏi portal, mà portal trả
lời theo giao diện ứng dụng (`color-scheme`, máy này là `default` = sáng). Thanh trên cùng thì do theme
của GNOME Shell tô: Yaru của Ubuntu tô `#131313` kể cả khi giao diện sáng; WhiteSur (máy này, qua tiện
ích User Themes) tô nền gần trong suốt và chữ trắng ở cả bản Dark lẫn Light.

**Vá:** tệp mới `src/lotus-gnome-theme.cpp`. Khi `XDG_CURRENT_DESKTOP` có `GNOME` (trừ Budgie, Pantheon,
GNOME Flashback và mấy desktop tự vẽ thanh riêng), Lotus tìm tệp CSS của shell giống cách GNOME làm:

- Tiện ích User Themes đang bật và có tên theme: tìm `~/.themes`, `$XDG_DATA_HOME/themes`, rồi thư mục
  hệ thống, đúng thứ tự trong `util.js` của tiện ích.
- Không có: lấy `stylesheetName` trong `gnome-shell/modes/$GNOME_SHELL_SESSION_MODE.json` (Ubuntu:
  `Yaru/gnome-shell.css`).
- Đọc luật `#panel`: có màu chữ thì theo màu chữ (chữ sáng là thanh tối), vì thanh trong suốt lấy nền
  từ hình nền nên chỉ màu chữ mới nói được ý của theme. Không có thì theo màu nền nếu nền đục.
- GNOME gốc (theme Adwaita đóng gói trong GResource) không đọc được thì giữ cách cũ.

Đọc `gsettings` bằng tiến trình con như đoạn mã cũ vẫn làm; kết quả vẫn lưu tạm 5 giây.

**Đo 16/09/2026:**

- Bài kiểm `gnome_panel_theme`, 20 trường hợp, CSS chép từ WhiteSur và Yaru thật. Khai trước là bản
  rỗng phải đỏ đúng 12: ra đúng 12. Mã thật 20/20, cả bộ 13/13.
- Chạy phần nhận màu với môi trường thật của fcitx5 trên iMac: tìm ra
  `~/.themes/WhiteSur-Dark/gnome-shell/gnome-shell.css`, trả lời thanh tối (trước vá: portal trả 0 = sáng).

**Chưa kiểm:** nhìn icon trên màn hình sau vá (lúc dựng màn hình đang khoá, GNOME tắt khay khi khoá);
GNOME gốc Fedora/Arch; GNOME Wayland; tiện ích làm thanh trong suốt khác; `clang-format` (máy không có).

**Upstream:** issue #508, chưa gửi PR.

## Cấu hình nên đặt kèm

**Khuyên dùng `Uinput` làm chế độ gõ chính** (chủ máy chốt 13/09/2026, gộp còn một chế độ 24/09).
Trong `lotus.conf` ghi `Mode=Uinput`; tên sai khác thì Lotus lặng lẽ quay về Preedit (ba tên cũ
`Uinput (Smooth)`, `Uinput (Slow)`, `Uinput (Super Smooth)` vẫn đọc được). Trong
`lotus-app-rules.conf` dùng số `2`; số `1` và `3` cũ cũng đọc thành `Uinput`.

**Kiểu gõ chính là Telex** (`InputMethod=Telex` trong `lotus.conf`), trên cả hai máy. Mọi số đo trong
tệp này đều gõ Telex. Kiểu gõ ảnh hưởng tới lỗi: Telex bỏ dấu sau cả chữ nên hay phải thay nhiều ký tự một lúc, đúng
trường hợp LibreOffice ở nhóm E sai. Tác giả gốc dùng VNI, và ở #162 nói nhờ vậy ít gặp lỗi của chế độ
Surrounding Text. VNI chưa kiểm trên bản này.

Luật theo app đang dùng trên máy này (ghi trước khi gộp, số `3` giờ đọc thành `Uinput`):

- **`firefox=3`** và **`microsoft-edge=3`**. Dùng bản Lotus gốc thì đặt `firefox=1` (Smooth), vì
  Super Smooth gốc không có lá chắn chống lặp chữ ở thanh địa chỉ. Lark chạy trong Firefox nên cũng
  theo luật này.
- **`Alacritty=3`**. Lá chắn chỉ liên quan ô có tự điền, mà cửa sổ dòng lệnh không có tự điền.
- **`soffice=3`** trên máy iMac.

## Những gì bản này KHÔNG sửa

Nói rõ để khỏi mất công thử lại:

- **Chế độ Surrounding Text vẫn lỗi.** Cơ chế surrounding text vốn không đáng tin: Firefox đẩy
  văn bản xung quanh trễ 58 đến 184 ms. Đã thử hướng "tin vào bộ đệm" và bị hồi quy ở ô soạn
  thảo giàu, đã rút lại. Chế độ 4 hỏng trên Firefox cũng thuộc nhóm này. Trên LibreOffice Writer chế
  độ này sai 60/60 từ (đo 16/09/2026), do LibreOffice báo thiếu dấu cách như tác giả đã nói ở #162;
  vá LibreOffice ở nhóm E chỉ áp cho chế độ uinput.
- **Máy chủ uinput chết giữa lúc thay chữ làm bàn phím chết theo.** Đã tái hiện được, bộ đo nằm
  ở `probes/vong_go/may_chu_chet_giua_chung.py` trong repo workbench. Chưa vá, đang trong hàng
  đợi gửi issue.

## Quy ước khi gom lại lần sau

Tác giả đẩy mã rất nhanh, khoảng 163 commit mỗi 30 ngày. Khi cần cập nhật:

**Dựng nhánh MỚI từ `upstream/dev` rồi nhặt lại từng vá.** Đừng gộp chồng lên nhánh cũ. Gộp
chồng làm bản mình tụt lại sau upstream mà không ai để ý, đúng như nhánh `tong-hop` cũ: nó hơn
`dev` 27 commit nhưng lại THIẾU 1 commit của tác giả.

**Trang đầu của fork là `.github/README.md`**, không phải `README.md`. GitHub ưu tiên hiện tệp trong
`.github`, nhờ vậy `README.md` của tác giả giữ nguyên và không gây xung đột mỗi lần gom lại. Trang
đầu chỉ tóm tắt mỗi vá một dòng. **Thêm hoặc bỏ vá thì sửa cả tệp này lẫn trang đầu, trong cùng
commit.**

Sau khi gom xong, phép kiểm bắt buộc là **so mã băm cây mã** với nhánh trước đó. Giống nhau thì
việc gom đúng, khác một byte cũng là hỏng:

```
git rev-parse ban-dung^{tree}
git rev-parse <nhánh cũ>^{tree}
```

Lần gom 13/09/2026: 27 commit rút còn 19, bỏ 4 cặp làm-rồi-rút-lại, cây mã trùng khít
`ca93438c9247d503736bf4ce002d2d799fd252a9`, bộ kiểm 11/11.

Lần gom 16/09/2026, nền `79d5706`: vá bỏ X11 và mã vá icon đã vào bản gốc nên rơi ra; thêm 4 vá mới
(LibreOffice, gửi phím xoá qua bàn phím thật, rút hàng libinput, chờ 4 ms). Hai chỗ xung đột: dòng
`#include` ở `lotus-engine.cpp` (giữ `lotus-plasma-theme.h`, bỏ `ack-apps.h`) và vá icon (lấy mã của
bản gốc, giữ bài kiểm). Bài kiểm icon phải sửa theo hàm bản gốc đổi sang nhận `std::string`. Mã của bản
dựng mới khớp 0 dòng khác với mã máy chủ đang cài và mô-đun đã đo trên iMac. Bộ kiểm 12/12.

Lần lấy 24/09/2026 — **ngoại lệ, không dựng lại từ `dev`:** dựng từ `dev` sẽ kéo theo hai thứ chủ fork chọn
bỏ: chế độ gõ mới *Uinput (Surrounding Text)* (`90beecd`, xoá luôn tuỳ chọn `useSurroundingTextIfPossible`
mà vá LibreOffice đang dùng) và vá hoãn giao chữ cho GTK4 qua dbus (`2ca89a5`, đụng đúng chỗ giao chữ
fork đã viết lại). Nên nhặt thẳng 11 commit của `dev` vào `ban-dung` bằng `cherry-pick -x`: đóng gói
chạy bài kiểm, bản 3.5.10, nix, CI, dọn mã máy chủ, nhật ký qua `syslog()`, tài liệu musl, bỏ bộ
chọn icon riêng. Commit rút hàng libinput ra rỗng (fork đã có). Hai chỗ xung đột: `lotus-logger.cpp`
(lấy của bản gốc) và dòng `#include` ở `lotus-engine.cpp` (giữ `lotus-gnome-theme.h`, bỏ
`lotus-icon-resolver.h` và `ack-apps.h`). Bộ kiểm 21/21. Lần gom sau vẫn phải quyết lại hai commit bỏ
qua ở trên.

**Bộ chọn icon riêng đã đảo lại cùng ngày (`90c0e27`).** Bỏ nó thì Lotus chỉ báo cho khay **tên**
icon (`fcitx-lotus-default`), để KDE tự tra theo bộ icon đang dùng. Máy chủ fork dùng bộ Colloid:
bộ này không có tên đó nhưng có `fcitx`, KDE cắt dần đuôi tên (`fcitx-lotus` rồi `fcitx`) và khớp
vào icon bàn phím chung của Colloid trước khi tìm tới `hicolor`. Khay mất chữ V, hiện ô ⌘ viền tròn
(kiểm qua D-Bus: `IconName` đúng là `fcitx-lotus-default`; Colloid có `status/22/fcitx.svg`, không
có `fcitx-lotus*`). Bộ chọn icon trả đường dẫn thẳng tới file nên không phải tra tên. Bản gốc bỏ nó
không kèm PR hay issue nêu lý do. Lần gom sau: bỏ qua `334becf`, trừ khi bản gốc có cách khác
chữa được trường hợp này.

Mã băm commit trong tệp này đổi theo mỗi lần gom; tìm theo tiêu đề commit nếu không khớp.

**Không đổi tên trong mã.** Ngày 17/09/2026 chỉ đổi tên KHO trên GitHub thành `LotusVibe` (link cũ tự
chuyển); mọi tên bên trong mã, gói, dịch vụ vẫn là `lotus`. Chữ `lotus` nằm 1592 chỗ ở 95 tệp, và 4 tệp tác giả sửa nhiều nhất
chính là 4 tệp việc đổi tên phải cày nát, nên đổi tên là tự chuốc xung đột mỗi lần cập nhật.
Gói Nix dùng `--replace-fail` nên đổi tên là gãy bản dựng chứ không phải cảnh báo. Tính lại khi
số vá bị từ chối vượt 8 đến 10 cái.
