# Nhật ký thay đổi

Mọi thay đổi đáng kể của Ngó Sen (tên cũ LotusVibe), tách ra từ
[fcitx5-lotus](https://github.com/LotusInputMethod/fcitx5-lotus), được ghi ở đây. Cách ghi theo
[Keep a Changelog](https://keepachangelog.com/vi/1.1.0/). Mỗi dòng nói người dùng thấy gì thay đổi.

Số phiên bản gồm hai phần: phần trước dấu gạch là số của Ngó Sen (nằm trong `CMakeLists.txt`), phần sau
là lần đóng gói. Từ `0.5.0` Ngó Sen đánh số riêng; các bản trước mang số `3.5.10` của bản gốc lúc tách ra.
Gói có thêm "epoch" 1 để trình quản lý gói vẫn coi `0.5.0` mới hơn `3.5.10`. Mỗi bản phát hành mang nhãn
`ngosen-<phiên bản>`.

## [Chưa phát hành]

### Thêm

- Nhật ký gõ: vừa gõ ra chữ sai thì chọn "Lưu nhật ký gõ" trong menu fcitx5 ở khay hệ thống. Ngó Sen lưu
  vài trăm phím gần nhất, những gì bộ gõ gửi cho app và những gì app báo lại vào một file trong
  `~/.local/state/ngosen/`, để gửi kèm khi báo lỗi. Nhật ký chỉ nằm trong bộ nhớ cho tới lúc lưu, và
  không ghi gì gõ trong ô mật khẩu ([#93](https://github.com/ngosen/ngosen-fork-archive/pull/93)).

### Thay đổi

- Lõi gõ Rust, lõi mà mọi gói cài đặt đang dùng, lấy bamboo_core 0.3.26 thẳng từ tác giả gốc: bản đã gộp
  mọi bản sửa của Ngó Sen và nhanh hơn ở Backspace cùng vài chữ có dấu. Chữ gõ ra giống hệt bản trước ([#97](https://github.com/ngosen/ngosen-fork-archive/pull/97)).

### Kiểm thử

- Test gõ ngẫu nhiên kiểu Gõ Sen vào các app giả có tật như app thật: báo chữ chậm, báo chữ cũ, không báo,
  nhận phím chậm. Chữ cuối cùng trong app phải giống hệt chữ Telex của các phím đã gõ. `core_replay` chạy
  lại một file nhật ký gõ trên app giả để tìm lỗi người dùng gửi về ([#94](https://github.com/ngosen/ngosen-fork-archive/pull/94)).
- Test gõ ngẫu nhiên kiểu Gõ Sen xen lẫn click chuột vào giữa chữ cũ, Alt+Tab và click sang app khác, trên
  app X11, Wayland và XIM. Chữ gõ sau mỗi cú click phải giống như gõ thong thả tại đúng chỗ đó ([#95](https://github.com/ngosen/ngosen-fork-archive/pull/95)).
- Phần quyết định giữ hay bỏ chữ đang gõ khi rời ô gõ rồi quay lại được chuyển vào lõi bộ gõ, để test trên
  app giả chạy đúng đoạn code fcitx5 chạy. Gõ không thay đổi gì ([#96](https://github.com/ngosen/ngosen-fork-archive/pull/96)).

### Tài liệu

- Repo được tạo lại ngoài mạng fork của fcitx5-lotus, giữ nguyên lịch sử commit và các bản phát hành.
  Pull request và issue đến số 98 nằm ở kho lưu
  [ngosen/ngosen-fork-archive](https://github.com/ngosen/ngosen-fork-archive); các link PR cũ trong file
  này trỏ về đó (#1).
- Câu giới thiệu trong README nay là "Bộ gõ tiếng Việt cho fcitx5 trên Linux." thay cho "tối ưu cho
  Linux", và điểm đầu tiên nay là "Không cần quyền đặc biệt" (#3).

## [1.0.0-1] — 09/10/2026

Bản 1.0: Ngó Sen chỉ làm cho fcitx5. Click sang ô khác rồi gõ nay ra đúng trong LibreOffice Calc, WPS Office
và Google Sheets. Tải ở [ngosen-1.0.0-1](https://github.com/ngosen/ngosen/releases/tag/ngosen-1.0.0-1).

### Thêm

- Gói Nix cho NixOS, dựng từ mã của Ngó Sen (gói Nix cũ tải mã bản gốc nên đã gỡ ở [#21](https://github.com/ngosen/ngosen-fork-archive/pull/21)). Cách cài nằm
  trong README. CI dựng gói, chạy test và nạp nó vào fcitx5 trên màn hình X ảo ([#82](https://github.com/ngosen/ngosen-fork-archive/pull/82)).
- Hướng dẫn tự dựng trên NixOS trong `TU-DUNG.md`, và `nix develop` mở shell có đủ công cụ dựng ([#83](https://github.com/ngosen/ngosen-fork-archive/pull/83)).

### Sửa

- Google Sheets trên Wayland: click chuột sang ô khác rồi gõ, sau vài ô thì chữ ra lẫn chữ của ô trước và
  không thành tiếng Việt (vd. `afoc`, `ieengs`). Nay mỗi ô mới bắt đầu một chữ mới ([#86](https://github.com/ngosen/ngosen-fork-archive/pull/86)).
- LibreOffice Calc trên Wayland: click chuột sang ô khác rồi gõ, sau chữ có dấu thì bộ gõ xoá nhầm và mở
  hộp thoại "Delete Contents", từ đó không gõ được tiếng Việt nữa. Nay phím đầu ở ô mới bắt đầu một chữ
  mới ([#87](https://github.com/ngosen/ngosen-fork-archive/pull/87)).
- LibreOffice Calc mở thẳng trên Wayland (không qua module Qt của fcitx): click sang ô khác rồi gõ thì chữ
  ra lẫn chữ của ô trước, hoặc mở hộp thoại "Delete Contents". Nay bộ gõ hỏi Calc đang ở ô nào trước khi
  sửa chữ, nên mỗi ô mới bắt đầu một chữ mới ([#90](https://github.com/ngosen/ngosen-fork-archive/pull/90)).
- WPS Office: gõ hai phím giống nhau liền nhau (`dd`, `ee`, `oo`, `aa`, `ww`) thì phím thứ hai bị mất, nên
  `dd` không ra `đ` và `oo` không ra `ô`. Nay chỉ app gõ qua XIM mới bị coi là "gửi trả phím" ([#90](https://github.com/ngosen/ngosen-fork-archive/pull/90)).
- App X11 chạy trên phiên Wayland (WPS Office qua Xwayland): click chuột sang ô khác rồi gõ thì chữ ra lẫn
  chữ của ô trước. Nay bộ gõ nghe cú click trên cửa sổ Xwayland như trên phiên X11 ([#90](https://github.com/ngosen/ngosen-fork-archive/pull/90)).

### Thay đổi

- Phần xử lý gõ được tách khỏi fcitx5 và sắp xếp lại. Người dùng không thấy gì khác; phần này giờ có test
  riêng, chạy được trên máy không cài fcitx5 ([#77](https://github.com/ngosen/ngosen-fork-archive/pull/77), [#78](https://github.com/ngosen/ngosen-fork-archive/pull/78), [#79](https://github.com/ngosen/ngosen-fork-archive/pull/79)).

### Tài liệu

- Thêm `RELEASING.md`: những chỗ cần cập nhật sau mỗi PR và mỗi lần ra bản mới; mẫu PR nay đúng với
  fork (nhánh `main`, `ctest`, dòng CHANGELOG) ([#80](https://github.com/ngosen/ngosen-fork-archive/pull/80)).
- README không còn gọi Hyprland là compositor wlroots: Hyprland đã bỏ wlroots từ bản 0.42, nhưng vẫn
  có giao thức input-method-v2 mà Ngó Sen cần ([#81](https://github.com/ngosen/ngosen-fork-archive/pull/81)).
- README bỏ câu "không chạy ngầm với quyền root": uinput server của bản cũ chạy bằng tài khoản
  `uinput_proxy`, không phải root. Điểm đầu tiên nay chỉ ghi "Bỏ hẳn uinput server" ([#85](https://github.com/ngosen/ngosen-fork-archive/pull/85)).
- README bỏ điểm "Chuyển từ fcitx5-lotus không mất gì" khỏi danh sách đầu trang; mục Cài vẫn ghi gói tự
  thay bản cũ và giữ cấu hình ([#88](https://github.com/ngosen/ngosen-fork-archive/pull/88)).
- README bỏ roadmap bản IBus và bản chạy thẳng trên Sway, Hyprland: Ngó Sen chỉ làm cho fcitx5. Nhánh
  `feat/ibus-engine` giữ lại, không cập nhật nữa ([#89](https://github.com/ngosen/ngosen-fork-archive/pull/89)).

## [0.5.1-1] — 09/10/2026

Sửa lỗi Gõ Sen trên Sway và Hyprland, và cài mới thì dùng Gõ Sen luôn. Tải ở
[ngosen-0.5.1-1](https://github.com/ngosen/ngosen/releases/tag/ngosen-0.5.1-1).

### Thay đổi

- Cài mới thì chế độ mặc định là Gõ Sen thay cho Preedit, khớp với cửa sổ cài đặt. Ai đã chọn chế độ thì
  giữ nguyên ([#74](https://github.com/ngosen/ngosen-fork-archive/pull/74)).

### Sửa

- Chế độ Gõ Sen gõ được trên Sway, Hyprland và các WM dùng input-method-v2. Trước đây chữ đầu tiên cần
  thêm dấu làm bộ gõ kẹt, mọi phím sau đó không ra chữ ([#73](https://github.com/ngosen/ngosen-fork-archive/pull/73)).

### Tài liệu

- README mở đầu bằng những điểm chính của Ngó Sen và roadmap tới bản IBus và bản cho wlroots không cần
  fcitx5 ([#75](https://github.com/ngosen/ngosen-fork-archive/pull/75)). README gọi máy chủ cũ là uinput server, như trên website ([#76](https://github.com/ngosen/ngosen-fork-archive/pull/76)).

## [0.5.0-1] — 08/10/2026

Bản đầu tiên đánh số riêng; số `0.5` ứng với khoảng nửa chặng đường tới bản 1.0 có bản IBus cho GNOME. Tải ở
[ngosen-0.5.0-1](https://github.com/ngosen/ngosen/releases/tag/ngosen-0.5.0-1).

### Thêm

- README có mục cho VS Code trên Ubuntu: nên cài bằng gói `.deb` hoặc Flatpak; bản snap cần sửa lối tắt
  để gõ được tiếng Việt qua XIM ([#56](https://github.com/ngosen/ngosen-fork-archive/pull/56), [#62](https://github.com/ngosen/ngosen-fork-archive/pull/62)).
- README có mục "Gỡ": tắt máy chủ nền trước, lệnh gỡ gói cho từng bản phân phối, lệnh gỡ bản tự dựng,
  và hai thứ còn lại trên máy sau khi gỡ là tệp cấu hình và tài khoản `uinput_proxy` ([#27](https://github.com/ngosen/ngosen-fork-archive/pull/27)).
- Trang Giới thiệu trong cửa sổ cài đặt có lại hai nút "Báo cáo lỗi" và "Đề xuất tính năng"; chúng mở
  mẫu tương ứng ở mục Issues của kho `ngosen/ngosen` ([#31](https://github.com/ngosen/ngosen-fork-archive/pull/31)).
- Công cụ `misc/core-compare` gõ cùng một bộ phím vào lõi ghép dấu hiện tại (Go) và bản Rust
  `bamboo-core`, rồi chỉ ra chỗ hai bên ra chữ khác nhau. Chỉ dùng khi phát triển, không đi kèm bộ gõ
  ([#68](https://github.com/ngosen/ngosen-fork-archive/pull/68)).

### Thay đổi

- Phần mã gửi chữ, phím và lệnh xoá ra app đi qua một lớp trung gian riêng, bước đầu để sau này chạy
  được trên IBus. Người dùng không thấy gì khác ([#47](https://github.com/ngosen/ngosen-fork-archive/pull/47)).
- Phần mã đọc chữ quanh con trỏ, loại ô nhập và tên app cũng đi qua lớp trung gian đó; các chỗ nhận
  diện app riêng (LibreOffice, Firefox, Chromium, SDL, GTK4) gom về một tệp. Người dùng không thấy gì
  khác ([#48](https://github.com/ngosen/ngosen-fork-archive/pull/48)).
- Chữ tạm (chế độ Preedit và Emoji) và việc làm mới bảng gợi ý cũng đi qua lớp trung gian. Người dùng
  không thấy gì khác ([#49](https://github.com/ngosen/ngosen-fork-archive/pull/49)).
- Bảng chọn emoji cũng đi qua lớp trung gian, kèm bài kiểm cho các phím của bảng chọn (số, Space, Tab,
  mũi tên, lật trang, Enter, Esc). Người dùng không thấy gì khác ([#50](https://github.com/ngosen/ngosen-fork-archive/pull/50)).
- Phần gõ đọc cài đặt từ một bản sao riêng, cập nhật mỗi khi đổi cài đặt (cửa sổ cài đặt, nút bật/tắt
  trên menu, menu bảng mã). Người dùng không thấy gì khác ([#51](https://github.com/ngosen/ngosen-fork-archive/pull/51)).
- Hẹn giờ và việc nghe app báo chữ quanh con trỏ cũng đi qua lớp trung gian. Người dùng không thấy gì
  khác ([#52](https://github.com/ngosen/ngosen-fork-archive/pull/52)).
- Phần gõ có hàm riêng để đọc chữ UTF-8, xem giờ và ghi log, không dùng hàm của fcitx5 nữa. Người dùng
  không thấy gì khác, kể cả dòng log ([#53](https://github.com/ngosen/ngosen-fork-archive/pull/53)).
- Phím bấm tới phần gõ qua lớp trung gian, không còn là sự kiện phím của fcitx5; kèm bài kiểm cho việc
  đọc và đổi phím (phím bổ trợ, Shift, tên phím, tự viết hoa). Người dùng không thấy gì khác ([#54](https://github.com/ngosen/ngosen-fork-archive/pull/54)).
- Phần gõ lấy từ điển, bảng gõ tắt, bảng phím tự đặt và danh sách emoji qua lớp trung gian thay vì đọc
  thẳng từ engine fcitx5; kèm bài kiểm cho bảng phím tự đặt. Người dùng không thấy gì khác ([#55](https://github.com/ngosen/ngosen-fork-archive/pull/55)).
- Phần gõ dùng tên phím riêng thay cho tên phím của fcitx5; lúc dựng, máy tự so từng phím với số của
  fcitx5. Người dùng không thấy gì khác ([#63](https://github.com/ngosen/ngosen-fork-archive/pull/63)).
- Phần gõ của mỗi ô nhập không còn là một kiểu dữ liệu của fcitx5; fcitx5 chỉ giữ nó trong một lớp vỏ
  mỏng. Người dùng không thấy gì khác ([#64](https://github.com/ngosen/ngosen-fork-archive/pull/64)).
- Phần gõ nay dựng được mà không cần tệp nào của fcitx5: danh sách chế độ gõ, các cờ dùng chung và các
  hàm xử lý chữ đã dời sang tệp riêng; dòng "Trang 1/2" của bảng emoji nhờ lớp trung gian dịch. Người
  dùng không thấy gì khác ([#65](https://github.com/ngosen/ngosen-fork-archive/pull/65)).
- Phần gõ dời vào thư mục `src/core/` và được dựng thành thư viện riêng không nối với fcitx5; lỡ dùng
  tệp của fcitx5 ở đó thì bản dựng báo lỗi ngay. Người dùng không thấy gì khác ([#66](https://github.com/ngosen/ngosen-fork-archive/pull/66)).
- Tệp phần gõ (khoảng 1.800 dòng) được chia thành năm tệp theo việc: nhận phím, emoji, chờ app, sửa
  chữ cũ, gõ tắt. Thân các hàm giữ nguyên. Người dùng không thấy gì khác ([#67](https://github.com/ngosen/ngosen-fork-archive/pull/67)).
- Thêm lõi ghép dấu viết bằng Rust (thư viện `bamboo-core`), chưa bật: bản dựng vẫn dùng lõi Go. Muốn
  thử thì dựng với `-DNGOSEN_RUST_CORE=ON` (cần Rust 1.88+, không cần mạng vì thư viện phụ thuộc nằm sẵn
  trong `bamboo-rs/vendor`). Người dùng không thấy gì khác
  ([#69](https://github.com/ngosen/ngosen-fork-archive/pull/69)).
- Gói cho Arch, Fedora và openSUSE dùng lõi ghép dấu Rust, không cần Go để dựng nữa. Gói Debian và
  Ubuntu vẫn dùng lõi Go vì Rust có sẵn ở đó còn cũ. Gõ chữ không khác gì ([#70](https://github.com/ngosen/ngosen-fork-archive/pull/70)).
- Gói `.deb` cho Ubuntu 24.04, 25.10, 26.04 và Debian testing/unstable cũng dùng lõi Rust. Debian 12, 13
  và Ubuntu 22.04 vẫn dùng lõi Go ([#71](https://github.com/ngosen/ngosen-fork-archive/pull/71)).
- Chế độ `Uinput` đổi tên thành **Gõ Sen** (`Mode=Sen` trong `lotus.conf`). Cấu hình cũ, thứ tự chế độ,
  phím tắt và tuỳ chọn ẩn/hiện của chế độ cũ tự chuyển sang tên mới ([#43](https://github.com/ngosen/ngosen-fork-archive/pull/43)).
- Nhánh chính đổi tên từ `ban-dung` thành `main`. Đường dẫn cũ trên GitHub tự chuyển sang tên mới. Ai
  đã tải mã về máy thì chạy `git branch -m ban-dung main && git fetch origin && git branch -u origin/main main` ([#36](https://github.com/ngosen/ngosen-fork-archive/pull/36)).
- Mục Issues của kho được bật để nhận báo lỗi. Mẫu báo lỗi hỏi phiên bản và cách cài của Ngó Sen thay
  cho các cách cài của bản gốc; README chỉ tới mục Issues ([#30](https://github.com/ngosen/ngosen-fork-archive/pull/30)).
- Trên phiên X11 thật (Xfce, Cinnamon, MATE…), app nối với bộ gõ qua XIM cũng xoá chữ bằng phím gửi hộ,
  không cần máy chủ uinput; trước đây chỉ làm vậy dưới XWayland ([#35](https://github.com/ngosen/ngosen-fork-archive/pull/35)).
- App cài bằng snap (Firefox, Chromium…) nối với bộ gõ qua mô-đun fcitx đời cũ có sẵn trong gói snap;
  giờ chúng cũng xoá chữ bằng phím gửi hộ, không cần máy chủ uinput. Game dùng SDL 2.0.12 trở về trước
  nói cùng giao thức đó vẫn đi máy chủ ([#35](https://github.com/ngosen/ngosen-fork-archive/pull/35)).

### Bỏ

- Máy chủ nền `fcitx5-lotus-server`. Bộ gõ xoá chữ cũ bằng phím gửi hộ qua fcitx5, hoặc bấm phím qua
  XTEST trên phiên X11, nên không còn chương trình chạy ngầm, tài khoản `uinput_proxy` hay quyền thiết
  bị. Cập nhật từ bản cũ thì gói tự tắt dịch vụ cũ và xoá tài khoản đó. Game dùng SDL ngoài X11 không
  nhận phím gửi hộ nên gõ ra chữ không dấu ([#43](https://github.com/ngosen/ngosen-fork-archive/pull/43)).
- Chế độ Surrounding Text, gộp vào Gõ Sen: Gõ Sen đã tự xoá chữ qua surrounding text ở app cần cách
  đó. Cấu hình cũ đặt Surrounding Text (`Mode=Surrounding Text`, số `4` trong luật theo app) tự đọc
  thành Gõ Sen. Còn lại các chế độ Gõ Sen, Preedit, Emoji và OFF ([#43](https://github.com/ngosen/ngosen-fork-archive/pull/43)).
- Chế độ Minecraft. Đặt nhầm chế độ này cho app thường thì app còn sót một chữ cũ. Cấu hình cũ đặt
  Minecraft (`Mode=Minecraft`, số `8` trong luật theo app) tự đọc thành `Uinput`; chế độ này cũng biến
  khỏi bảng chọn chế độ và cửa sổ cài đặt ([#29](https://github.com/ngosen/ngosen-fork-archive/pull/29)).

### Sửa

- VS Code bản Flatpak, và bản `.deb` chạy Wayland, gõ được tiếng Việt: bộ gõ không còn coi việc VS Code
  nhích con trỏ ngay sau mỗi phím là một cú bấm chuột ([#58](https://github.com/ngosen/ngosen-fork-archive/pull/58)).
- VS Code bản snap (gõ qua XIM) ít gõ sai hơn, như "nười", "đôồn": bộ gõ không đếm hai lần một phím mà
  VS Code gửi lại ([#60](https://github.com/ngosen/ngosen-fork-archive/pull/60), [#61](https://github.com/ngosen/ngosen-fork-archive/pull/61), [#62](https://github.com/ngosen/ngosen-fork-archive/pull/62)).
- Cửa sổ không báo tên app: chế độ gõ chọn riêng cho cửa sổ đó giờ được xoá khi đóng cửa sổ, nên cửa
  sổ mở sau không còn có thể nhận nhầm chế độ ấy, và danh sách quy tắc không dài thêm suốt phiên ([#59](https://github.com/ngosen/ngosen-fork-archive/pull/59)).
- Trên Wayland, bấm chuột sang chỗ khác trong ô rồi gõ tiếp giờ bắt đầu từ mới ngay cả khi máy chủ
  nền không chạy: bộ gõ nhận ra cú bấm qua việc con trỏ đổi chỗ mà chữ không đổi ([#40](https://github.com/ngosen/ngosen-fork-archive/pull/40)).
- Lark (Messenger web) trên Firefox gõ đúng dấu trở lại ("về" thay cho "vêf", "nấu" thay cho "nâú").
  Ô soạn tin này báo con trỏ đã tiến qua chữ vừa gõ trước khi báo chữ đó, và bộ gõ tưởng là một cú
  bấm chuột nên bỏ dở từ đang gõ ([#45](https://github.com/ngosen/ngosen-fork-archive/pull/45)).
- Trên X11, gõ trong terminal (gnome-terminal, xfce4-terminal…) không còn ra chữ rác như "ngayDDày".
  Cách bôi đen chữ cũ rồi gõ đè dành cho thanh địa chỉ Chrome ([#38](https://github.com/ngosen/ngosen-fork-archive/pull/38)) nay chỉ áp dụng cho trình duyệt họ
  Chromium; terminal in Shift+Mũi tên trái ra thành ký tự thay vì bôi đen ([#46](https://github.com/ngosen/ngosen-fork-archive/pull/46)).
- Ubuntu 26.04 (GNOME 50.0 tới 50.3): gói kèm extension `forward-keys@ngosen.github.io` sửa lỗi GNOME làm
  rơi phím xoá bộ gõ gửi qua GNOME Shell, khiến Chrome, Edge và app Electron chạy Wayland gõ ra
  `tieêngếng`. Bật bằng `gnome-extensions enable forward-keys@ngosen.github.io` ([#41](https://github.com/ngosen/ngosen-fork-archive/pull/41)).
- Trên KDE Plasma (Wayland), bộ gõ tự gửi phím xoá cho ứng dụng thay vì nhờ máy chủ uinput, rồi chờ ứng
  dụng báo đã xoá xong mới gõ chữ mới. Ứng dụng X11 chạy qua XWayland không còn bị kẹt, nuốt hết phím
  sau một lần thay chữ. Ô soạn tin Facebook trên KDE xoá rồi gõ lại như ô thường, không bôi đen rồi gõ
  đè nữa ([#33](https://github.com/ngosen/ngosen-fork-archive/pull/33)).
- Trên GNOME, gõ nhanh trong Firefox không còn mất chữ ("viet" ra "v"), ô soạn tin Facebook trên Edge
  không còn ra "i" thay cho "đi", và chuyển từ app khác sang terminal không còn làm rơi chữ. Luật theo
  app nhận cả tên GNOME báo kèm đuôi `.desktop` (như `firefox.desktop`) ([#33](https://github.com/ngosen/ngosen-fork-archive/pull/33)).
- App nối với bộ gõ qua IBus hoặc qua mô-đun fcitx5 (`GTK_IM_MODULE=ibus`/`fcitx`, ví dụ Zalo và các
  app Electron chạy X11), và mọi app trên GNOME, gõ được mà không cần máy chủ uinput. App GTK4 xoá chữ
  bằng surrounding text vì mô-đun GTK4 bỏ qua phím gửi hộ. Game dùng SDL vẫn đi máy chủ vì SDL chỉ
  nhận chữ ghi và chữ gạch chân ([#34](https://github.com/ngosen/ngosen-fork-archive/pull/34)).
- Trên X11, thanh địa chỉ Chrome, Edge và Chromium không còn giữ dấu cũ khi gõ một địa chỉ đã từng vào
  ("tiêng" thay cho "tiếng"). Trình duyệt tự gợi ý phần đuôi và bôi đen nó, nên phím xoá đầu chỉ xoá phần
  gợi ý. Ngó Sen nay bôi đen cả chữ cũ bằng Shift+Mũi tên trái qua XTEST rồi gõ đè. Các app khác trên X11
  cũng xoá qua XTEST khi không gửi phím hộ được, không cần máy chủ uinput. Gói nay kéo theo thư viện
  `libxcb-xtest`, vì Linux Mint không cài sẵn nó ([#38](https://github.com/ngosen/ngosen-fork-archive/pull/38)).
- Trên X11, bấm chuột sang chỗ khác giữa lúc gõ không còn làm từ mới dính vào từ cũ khi máy chủ uinput
  không chạy: Ngó Sen nghe cú bấm chuột qua XInput2 của X server. Gói kéo theo thư viện `libxcb-xinput` ([#39](https://github.com/ngosen/ngosen-fork-archive/pull/39)).

### Tài liệu

- README viết lại cho người dùng: lộ trình Ngó Sen 1.0 ở đầu, rồi cách chọn chế độ, cài và gỡ. Chi tiết
  từng vá không còn trong kho; hướng dẫn tự dựng từ mã chuyển sang `TU-DUNG.md` ([#42](https://github.com/ngosen/ngosen-fork-archive/pull/42)).
- README: mục lộ trình đổi tên thành "Roadmap", ghi rõ lõi ghép dấu là bamboo-core; phần nguồn gốc
  (vibecode, tách từ fcitx5-lotus) chuyển xuống cuối; ghi thêm các máy ảo dùng để thử ([#44](https://github.com/ngosen/ngosen-fork-archive/pull/44)).
- Tên mới trong mã (tệp, hàm, lớp, hằng, biến môi trường) không còn chữ "lotus"; tên cũ giữ nguyên
  tới khi cần sửa chỗ đó. CI kiểm tra mỗi lần đẩy mã ([#37](https://github.com/ngosen/ngosen-fork-archive/pull/37)).
- Nhật ký thay đổi có mục riêng cho bản `3.5.10-4`; những gì gộp sau bản đó nằm dưới "Chưa phát hành"
  ([#28](https://github.com/ngosen/ngosen-fork-archive/pull/28)).

### Kiểm thử

- Bốn bài kiểm thử khoảng chờ của Messenger không còn hỏng khi máy chạy thử bị khựng vài chục phần
  nghìn giây: khoảng chờ trong bài nới từ 40 lên 500 ms, và vòng lặp sự kiện của bài chạy nốt việc
  đã tới hạn trước khi kiểm. Không đổi gì ở bộ gõ ([#32](https://github.com/ngosen/ngosen-fork-archive/pull/32)).

## [3.5.10-4] — 04/10/2026

Bản phát hành đầu tiên có gói dựng sẵn:
[ngosen-3.5.10-4](https://github.com/ngosen/ngosen/releases/tag/ngosen-3.5.10-4).

### Thêm

- Kịch bản cài một dòng `install.sh`: nhận ra bản phân phối, tải gói dựng sẵn từ bản phát hành mới
  nhất, đối chiếu mã băm, hỏi lại rồi cài và bật máy chủ nền. Thay được gói `fcitx5-lotus` đang có,
  kể cả bản gốc số cao hơn, và ghi đè được bản cài từ mã trên Arch. README hướng dẫn cài theo cách
  này; hướng dẫn cập nhật bản tự dựng được bổ sung các lệnh nạp lại luật quyền ([#26](https://github.com/ngosen/ngosen-fork-archive/pull/26)).

### Thay đổi

- Trang Giới thiệu trong cửa sổ cài đặt trỏ về kho `ngosen/ngosen` và ghi "Dựa trên fcitx5-lotus của"
  kèm tên các tác giả gốc. Bỏ hai nút "Báo cáo lỗi" và "Đề xuất tính năng", vì chúng mở trang báo lỗi
  của bản gốc ([#20](https://github.com/ngosen/ngosen-fork-archive/pull/20)).
- Bộ gõ đổi tên hiển thị thành **Ngó Sen**: tên trong danh sách bộ gõ và addon của fcitx5, nhãn "Ngó
  Sen - Tắt", cửa sổ và mục menu "Cài đặt Ngó Sen", trang Giới thiệu. Tệp cấu hình và tên bên trong
  giữ nguyên, nên cấu hình cũ dùng tiếp được ([#15](https://github.com/ngosen/ngosen-fork-archive/pull/15)).
- Gói Fedora đổi tên thành `fcitx5-ngosen`. Cài gói này thì bản `fcitx5-lotus` cũ của fork được thay
  tự động; gói này và `fcitx5-lotus` của bản gốc không cài chung được ([#16](https://github.com/ngosen/ngosen-fork-archive/pull/16)).
- Gói Debian/Ubuntu đổi tên thành `fcitx5-ngosen` và thay gói `fcitx5-lotus` khi cài. Khi cài hoặc
  cập nhật, gói tự gỡ tài khoản `uinput_proxy` khỏi nhóm `input` và áp luật quyền cho chuột, bàn chạm
  đang cắm, giống gói Fedora ([#23](https://github.com/ngosen/ngosen-fork-archive/pull/23)).
- Gói openSUSE đổi tên thành `fcitx5-ngosen`, có cùng các bước sau cài như gói Fedora. Thêm công thức
  gói Arch (`packaging/arch/PKGBUILD`) cho ra gói pacman `fcitx5-ngosen`; quy trình dựng thử trên
  GitHub nay dựng gói này thay cho tệp nén thô ([#24](https://github.com/ngosen/ngosen-fork-archive/pull/24)).
- Quy trình phát hành viết lại: gắn nhãn `ngosen-<phiên bản>` thì GitHub kiểm số phiên bản trong các
  tệp đóng gói, dựng gói cho Fedora, Debian, Ubuntu, openSUSE và Arch, rồi tạo bản phát hành nháp kèm
  `SHA256SUMS`. Bỏ kho gói có chữ ký và chìa khoá công khai của bản gốc. Các gói lên `3.5.10-4` ([#25](https://github.com/ngosen/ngosen-fork-archive/pull/25)).
- Kho chuyển về địa chỉ `github.com/ngosen/ngosen` (link cũ tự chuyển). Trang giới thiệu viết lại theo
  tên Ngó Sen và nằm ở `README.md`; README của bản gốc không còn trong kho, xem ở kho fcitx5-lotus
  ([#17](https://github.com/ngosen/ngosen-fork-archive/pull/17)).
- Cập nhật bamboo-core (lõi bộ gõ Telex/VNI) theo bản gốc ([#11](https://github.com/ngosen/ngosen-fork-archive/pull/11)).

### Bỏ

- Gói Nix (`flake.nix`, thư mục `nix/`) và hai quy trình kiểm Nix. Công thức này tải mã của bản gốc
  về dựng chứ không dựng mã của Ngó Sen, nên chưa bao giờ cho ra đúng bản này ([#21](https://github.com/ngosen/ngosen-fork-archive/pull/21)).
- Biến môi trường `LOTUS_SERVER_PATH`: mô-đun không còn kiểm đường dẫn của máy chủ nền ([#9](https://github.com/ngosen/ngosen-fork-archive/pull/9)).
- Giao diện cài đặt: bỏ dòng và lời giải thích còn sót của tuỳ chọn `FixUinputWithAck` đã gỡ. Giao
  diện vốn không hiện tuỳ chọn này, vì addon không còn khai báo nó ([#14](https://github.com/ngosen/ngosen-fork-archive/pull/14)).

### Sửa lỗi

- Gói Fedora: khi cập nhật, tài khoản `uinput_proxy` tự được gỡ khỏi nhóm `input`, và luật quyền cho
  chuột, bàn chạm được áp ngay cho thiết bị đang cắm, không phải khởi động lại máy hay cắm lại ([#19](https://github.com/ngosen/ngosen-fork-archive/pull/19)).
- Máy chủ nền giữ tối đa 1024 phím xoá trong hàng chờ và bỏ hàng chờ khi bộ gõ ngắt kết nối. Trước đây
  một chương trình gửi dồn dập có thể khiến máy chủ tiếp tục xoá chữ rất lâu sau khi nó đã dừng ([#18](https://github.com/ngosen/ngosen-fork-archive/pull/18)).
- Khi bộ gõ khởi động lại và nối lại với máy chủ nền đúng lúc kết nối cũ vừa đứt, máy chủ không còn
  ngắt nhầm kết nối mới, và không gõ tiếp các phím xoá còn dở của kết nối cũ ([#22](https://github.com/ngosen/ngosen-fork-archive/pull/22)).
- Luật udev không còn cấp `/dev/uinput` và mọi thiết bị nhập cho cả nhóm `input`, khớp bản gốc
  (LotusInputMethod/fcitx5-lotus#525) ([#10](https://github.com/ngosen/ngosen-fork-archive/pull/10)).
- Máy chủ nền và mô-đun bộ gõ nhận nhau theo tài khoản (uid) của tiến trình bên kia, thay cho đường
  dẫn chương trình. Máy chủ không còn giữ quyền `CAP_SYS_PTRACE`. Mô-đun nay kiểm cả socket phím
  xoá, nên chương trình chiếm tên socket trước không đọc được độ dài từng từ ([#9](https://github.com/ngosen/ngosen-fork-archive/pull/9)).
- Máy chủ nền chỉ mở chuột, bàn chạm và núm trỏ, ở chế độ chỉ đọc; không mở bàn phím nữa, và tài
  khoản `uinput_proxy` ra khỏi nhóm `input`. Chuột kiêm bàn phím mất tính năng bấm chuột để ngắt từ.
  Máy đã cài từ trước cần chạy một lần `sudo gpasswd -d uinput_proxy input` ([#8](https://github.com/ngosen/ngosen-fork-archive/pull/8)).
- Máy chủ nền chỉ nhận số phím từ 1 tới 1024 (backspace) hoặc từ -1 tới -1024 (bôi đen); số khác bị
  bỏ qua và ghi log. Trước đây một số âm rất lớn làm máy chủ chết, bàn phím chết theo ([#7](https://github.com/ngosen/ngosen-fork-archive/pull/7)).
- Chế độ Uinput, app nhận chữ qua dbus (fcitx5-gtk): chữ thay thế được commit ngay sau khi phím xoá
  xử lý xong thay vì trong lúc xử lý, để Ghostty và foot không làm rơi chữ. Lấy từ bản gốc
  (LotusInputMethod/fcitx5-lotus#510), chưa tái hiện được lỗi trên máy thử ([#12](https://github.com/ngosen/ngosen-fork-archive/pull/12)).
- Chế độ Preedit: bấm phím mở menu chế độ khi đang gõ dở một chữ thì chữ đó được commit và bộ gõ về
  trạng thái đầu. Lấy từ bản gốc ([#11](https://github.com/ngosen/ngosen-fork-archive/pull/11)).
- Chế độ Preedit: xử lý phím theo độ dài chữ đang soạn thay vì độ dài phím vừa bấm, và chặn lỗi ở
  macro Tab. Lấy từ bản gốc ([#11](https://github.com/ngosen/ngosen-fork-archive/pull/11)).

## Mốc khởi đầu của fork — 26/09/2026

`ban-dung` ở commit `b121f3c`, dựa trên nhánh `dev` của bản gốc tại `79d5706` (15/09/2026), cộng
các commit lấy thêm từ bản gốc ngày 24/09/2026. Mọi mục dưới đây là chỗ fork khác bản gốc đó.

### Thay đổi

- Gộp ba chế độ uinput (Slow, Smooth, Super Smooth) thành một chế độ **Uinput**. Cấu hình cũ, luật
  theo app (chế độ 1–3) và thứ tự chế độ tự chuyển sang; menu chế độ chỉ hiện nó một lần ([#6](https://github.com/ngosen/ngosen-fork-archive/pull/6)).
- Ở chế độ Uinput, sau khi gửi backspace, bộ gõ chờ app báo surrounding text đã đổi (bằng timer)
  thay vì ngủ một khoảng cố định (`WaitSurroundingEvent`, bật sẵn).
- App không báo surrounding text (terminal, Chromium) bỏ qua bước chờ thử lại.
- Server gửi backspace liền nhau; `LOTUS_BACKSPACE_GAP_MS` (0–50) để chèn khoảng nghỉ.
- Các bản sửa cho Messenger và ô soạn bài Facebook bật sẵn ([#5](https://github.com/ngosen/ngosen-fork-archive/pull/5)).
- Tên biến, comment và log trong mã riêng của fork viết tiếng Anh, theo [AGENTS.md](AGENTS.md) ([#4](https://github.com/ngosen/ngosen-fork-archive/pull/4)).

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

- Tệp `KHAC-GI-SO-VOI-BAN-GOC.md`: từng miếng vá, vì sao có, đã gửi lên bản gốc chưa (không còn trong kho từ
  [#42](https://github.com/ngosen/ngosen-fork-archive/pull/42)).
- [AGENTS.md](AGENTS.md): quy định khi sửa fork và khi gửi vá lên bản gốc ([#3](https://github.com/ngosen/ngosen-fork-archive/pull/3)).
- README: cài sang máy khác, chế độ Uinput duy nhất, khởi động lại server sau khi cập nhật.
