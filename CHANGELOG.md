# Nhật ký thay đổi

Mọi thay đổi đáng kể của Ngó Sen được ghi ở đây. Cách ghi theo
[Keep a Changelog](https://keepachangelog.com/vi/1.1.0/). Mỗi dòng nói người dùng thấy gì thay đổi.

Số version gồm hai phần: phần trước dấu gạch là số của Ngó Sen (nằm trong `CMakeLists.txt`), phần sau là
lần đóng gói. Từ `0.5.0` Ngó Sen đánh số riêng; các version trước mang số `3.5.10` của bản gốc lúc tách ra.
Gói có thêm "epoch" 1 để trình quản lý gói vẫn coi `0.5.0` mới hơn `3.5.10`. Mỗi release có tag
`ngosen-<version>`.

## [Chưa phát hành]

### Thêm

- Cứu chữ: quên bật tiếng Việt mà đã gõ `tieengs vieetj`, bấm phím chuyển sang Ngó Sen (vd. Ctrl+Space) thì
  chữ đổi luôn thành `tiếng việt`, cạnh con trỏ hiện "Cứu chữ: Esc để trả lại chữ đã gõ" (#82).
- Cứu chữ không tự đổi khi trong đoạn có chữ gõ lại vẫn y nguyên (như `abc` trong `abc tieengs`), vì
  thường đó là tiếng Anh gõ có chủ ý. Khi đó chỉ hiện gợi ý `abc tieengs → abc tiếng` kèm dòng
  "Cứu chữ: Tab để đổi, Esc để giữ nguyên": Tab gõ lại có dấu, Tab lần nữa trả lại như cũ, Esc hoặc phím
  khác thì bỏ qua (#80).
- Cứu chữ không ghi nhớ gì trong ô mật khẩu, quên đoạn đã gõ khi bấm chuột, Enter, phím mũi tên hay chuyển
  ô, và không hiện ở app không cho bộ gõ xoá chữ (#71).
- Ô "Cứu chữ: gõ lại có dấu khi quên bật tiếng Việt" ở trang Gõ chữ, nhóm Gõ nhanh, bật sẵn. Tắt ô thì Ngó Sen
  không ghi nhớ phím gõ lúc chưa bật tiếng Việt (#122).
- Mỗi version phát hành được tự dựng trên kho COPR `nguyenphi/ngosen`, để người dùng Fedora lên version mới bằng
  `dnf upgrade`: bật kho bằng `sudo dnf copr enable nguyenphi/ngosen` (#133, #134).
- Kho APT có chữ ký cho Debian, Ubuntu và các bản dựa trên chúng, để lên version mới bằng `apt upgrade` (#135).
- Kho pacman có chữ ký cho Arch, CachyOS và EndeavourOS, để lên version mới bằng `pacman -Syu` (#137).
- Kho openSUSE Tumbleweed có chữ ký, để lên version mới bằng `zypper up` (#140).
- Có gói cho Ubuntu 26.10: lệnh cài một dòng và kho APT nhận Ubuntu 26.10 trên máy x86_64 và ARM 64-bit (#150).

### Thay đổi

- Test X11 của bản Nix tắt Xvfb khi xong, không còn treo khi chạy ngoài sandbox; người dùng không thấy
  khác gì (#147).

### Sửa

- Gõ Sen trong WordCraft và các app dùng XIM trên phiên X11 không còn rối chữ (như `dââấ` thay vì `dấu`): Ngó
  Sen xoá chữ bằng phím thật qua XTEST thay vì gửi backspace qua XIM (#156).
- Gõ Sen trong Wine 11 (phần mềm Windows) trên phiên Wayland không còn rối chữ (như `tiêến` thay vì `tiếng`):
  Ngó Sen gửi phím xoá thẳng vào cửa sổ app, không hiện hộp hỏi quyền điều khiển (#158).
- Gõ trên thanh địa chỉ Firefox ESR (bản Firefox mặc định của Debian) khi Firefox đang tự điền địa chỉ không còn
  ra chữ thừa (như `coờ` thay vì `cờ`) (#162).
- Cứu chữ không hiện khi phím chuyển là Ctrl+Space (phím mặc định của fcitx5) hay một phím tắt khác đặt trong
  fcitx5, và không chạy trong kitty (#83).
- Nhật ký gõ ghi đúng app đang gõ khi chuyển qua lại giữa các cửa sổ. Trước đây quay về một ô đã gõ trước đó
  thì phím gõ ở đó bị ghi dưới tên app vừa rời (#143).
- Nút "Mặc định" trong cửa sổ cài đặt sáng ngay khi đã có gõ tắt, keymap riêng hay từ thêm vào từ điển, không
  cần mở trang đó trước (#132).
- Gõ Sen: xoá lùi từ sau dấu cách vào lại từ vừa gõ rồi gõ tiếp thì Ngó Sen sửa tiếp chính từ đó. Trước đây
  gõ `duyeet`, dấu cách, xoá còn `du` rồi gõ `yeejt` ra `duyeejt` thay vì `duyệt`, ở mọi app kể cả kitty (#142).
- Gõ Sen trong gnome-text-editor: xoá lùi giữa từ rồi gõ tiếp không còn mất chữ. Trước đây gõ `toan`, xoá một
  chữ rồi gõ `nf` ra `toòn` thay vì `toàn` (#149).

## [1.5.2-1] — 18/10/2026

Sửa lỗi cửa sổ cài đặt: nút phím ở trang Phím › Phím tắt bị cắt mép hoặc mất khi phóng to cửa sổ.
Tải ở [ngosen-1.5.2-1](https://github.com/ngosen/ngosen/releases/tag/ngosen-1.5.2-1).

### Sửa

- Ở trang Phím › Phím tắt, nút phím ở cuối mỗi hàng chế độ không còn bị cắt mép, và không biến mất khi phóng to
  cửa sổ (#128).

### Tài liệu

- README ghi đúng gói nào dùng lõi Rust (mọi gói trừ Debian 12); lộ trình bỏ mục 1.5.0 vì đã có trong 1.5.1
  (#129).

## [1.5.1-1] — 18/10/2026

Version mới: nút "Kiểm tra máy tôi…" trong cửa sổ cài đặt, và lõi gõ Rust cho cả Debian 13 và Ubuntu 22.04.
1.5.0 không được phát hành; 1.5.1 là 1.5.0 thêm bản sửa #117.
Tải ở [ngosen-1.5.1-1](https://github.com/ngosen/ngosen/releases/tag/ngosen-1.5.1-1).

### Thêm

- Nút "Kiểm tra máy tôi…" ở trang Giới thiệu của cửa sổ cài đặt: xem fcitx5 có chạy không, Ngó Sen có trong
  danh sách bộ gõ không, các biến `GTK_IM_MODULE`, `QT_IM_MODULE`, `XMODIFIERS`, IBus có chạy song song không, và
  trên KDE Wayland thì fcitx5 có được chọn làm bàn phím ảo không. Mỗi lỗi kèm cách sửa; nút "Báo cáo lỗi…" mở
  trang báo lỗi trên GitHub đã điền sẵn kết quả, không tự gửi gì (#88).
- "Kiểm tra máy tôi" thêm mục test một app: gõ `Tieengs Vieetj cos daaus` trong app, chọn "Lưu nhật ký gõ" ở
  menu Ngó Sen, rồi bấm "Đọc nhật ký gõ". Nếu app có báo chữ cho bộ gõ, "Kiểm tra máy tôi" tự biết gõ đúng hay sai;
  app không báo thì bạn tự chọn. Kết quả, cùng cách app nhận chữ và cách bộ gõ xoá chữ, được đưa vào báo lỗi (#90).

### Thay đổi

- Gói cho Debian 13 và Ubuntu 22.04 chuyển sang lõi gõ Rust như các gói khác; chỉ gói Debian 12 còn dùng lõi Go (#91).
- README ghi công tác giả hai lõi gõ: @nguyen10t2 (lõi Rust) và @t1ld3x (lõi Go) (#89).
- Dữ liệu test của cửa sổ cài đặt dùng đường dẫn addon `ngosen` thay cho tên cũ `lotus`; người dùng không
  thấy khác gì (#79).
- Test tự động của lệnh cài chạy thêm trên Fedora 44 với KDE; người dùng không thấy khác gì (#70).
- Sửa một test của lõi gõ thỉnh thoảng báo sai khi máy bận; người dùng không thấy khác gì (#116).
- Đổi tên vài hàm và test còn mang tên chế độ cũ (Uinput, Super Smooth) theo tên Gõ Sen; người dùng không
  thấy khác gì (#69).

### Sửa

- Chọn kiểu gõ Custom khi chưa bật "Keymap tuỳ chỉnh" hay chưa có phím nào thì gõ không ra dấu mà cửa sổ cài đặt
  không nói gì. Giờ dưới hàng Kiểu gõ có dòng nhắc bật Keymap tuỳ chỉnh và thêm phím ở Phím › Keymap (#123).
- Ở trang Phím › Phím tắt, khi hai chế độ trùng phím, dòng cảnh báo không còn đè lên tên chế độ và nút phím không
  còn bị co lại; hàng giãn ra cho đủ chỗ (#124).
- "Tự viết hoa đầu câu" viết hoa cả chữ đầu dòng sau Enter trong app GTK như xed, đúng như tên tiếng Anh của mục
  ghi; trước đây app báo reset lúc xuống dòng làm bộ gõ quên mất (#117).

### Tài liệu

- Trang của version thử không còn ngắt dòng giữa câu trên điện thoại (#109).
- README nói rõ vì sao Ngó Sen chỉ làm cho fcitx5, và gọi thống nhất file, repo, build, release (#73).
- CHANGELOG và README viết "version 1.2" thay cho "bản 1.2"; CHANGELOG dùng file, repo, build, uinput server, forward key như README (#103).
- Hướng dẫn đóng góp có thêm nhánh `patch`: PR sửa lỗi của version đang phát hành gửi vào đó, tính năng vẫn gửi
  vào `next` (#101).
- Trang [lộ trình](ROADMAP.md): những gì Ngó Sen định làm ở các version tới (#120, #121).

## [1.4.3-1] — 17/10/2026

Version sửa lỗi: bỏ chọn "Từ điển tuỳ chỉnh" thì quay về từ điển gốc trên Ubuntu 22.04, 24.04 và Debian 12, 13.
Tải ở [ngosen-1.4.3-1](https://github.com/ngosen/ngosen/releases/tag/ngosen-1.4.3-1).

### Sửa

- Bỏ chọn "Từ điển tuỳ chỉnh" thì Ngó Sen quay về từ điển gốc trên distro có fcitx5 cũ hơn 5.1.13: Ubuntu 22.04, 24.04 (cả Mint 22), Debian 12, 13 (cả MX 25). Trước đây từ thêm vào từ điển tuỳ chỉnh vẫn được giữ dấu sau khi bỏ chọn (#111).

### Tài liệu

- Trang release viết lại cho dễ đọc trên điện thoại: version này sửa gì, lệnh cài, chọn file theo distro, và
  chỉ ghi những gì đã test trên version này (#108).
- README trên trang chính khớp version 1.4.2: nói rõ vì sao Ngó Sen chỉ làm cho fcitx5, ghi công tác giả hai lõi gõ,
  và dùng version, file, repo, build, release như CHANGELOG (#104).
- Trang tải mỗi version không còn đoạn giải thích cách đánh số từ `0.5.0` (vẫn có ở đầu nhật ký thay đổi), và viết "version", file, distro, build như README (#102).

## [1.4.2-1] — 16/10/2026

Version sửa lỗi: Gõ Sen hết mất chữ trên Sway và ở ô trống trên Wayland, hết chữ lặp trong Ptyxis và GNOME Console;
thêm gói cho máy ARM và lệnh cài cho Kali.
Tải ở [ngosen-1.4.2-1](https://github.com/ngosen/ngosen/releases/tag/ngosen-1.4.2-1).

### Thêm

- Gói cài cho máy ARM 64-bit (aarch64): Debian, Ubuntu, Kali và Fedora 43/44; lệnh cài nhanh tự chọn gói theo máy (#86).
- Lệnh cài nhanh chạy được trên Kali Linux: nó lấy gói build cho Debian testing, distro mà Kali dựa trên (#85).

### Thay đổi

- Lệnh cài không đổi phím bật/tắt tiếng Việt nữa: dùng phím mặc định của fcitx5 là Ctrl+Space, muốn
  đổi thì vào cài đặt của fcitx5. Máy đã được đặt Ctrl+Shift vẫn giữ nguyên (#76).

### Sửa

- Gõ Sen làm mất chữ khi gõ `dd`, `ow`… làm chữ đầu tiên trong ô trống trên Wayland, như ô hỏi của Gemini
  trên Chromium hay Zed: ô bị xoá trống thì mất focus một lúc, chữ gửi lúc đó bị bỏ. Giờ Ngó Sen chờ ô có focus lại rồi mới gửi (#94).
- Bật kiểm chính tả bằng từ điển thì vài chữ đúng bị trả về phím đã gõ khi kiểu đặt dấu khác cách từ điển ghi:
  `chói loas` thay cho `chói lóa` (kiểu cũ), `nhoes` thay cho `nhoé` (kiểu mới). Từ điển giờ khớp cả hai kiểu.
  Sửa ở lõi Rust; gói Debian 12, Debian 13 và Ubuntu 22.04 còn dùng lõi Go nên chưa có (#92).
- Trên Sway, chữ có dấu trong app GTK và Firefox bị mất khi gõ Gõ Sen (gõ `đường` ra `ường`) (#87).
- Lệnh cài nhanh dừng với "im-config không đặt được fcitx5" trên máy X11 dùng im-config 1.1 (Debian testing, Kali),
  và lệnh gỡ không trả lại bộ gõ cũ ở đó (#85).
- Lệnh cài nhanh báo không tìm thấy fcitx5 trên máy chưa từng chạy `apt update`, như máy ảo Kali build sẵn (#85).
- Gõ Sen trong GNOME Console trên GNOME ra chữ lặp (`ddwowngf` thành `ddduuwwoowwnnggff`) rồi đứng, giống
  lỗi Ptyxis ở #74. Ptyxis trên phiên X11 (như KDE Plasma X11) khi máy có cài IBus cũng sai dấu như vậy. Giờ
  cả hai gõ đúng dấu (#81).
- Lõi gõ Rust lên bamboo-core 0.4.1: gõ từ dài hơn 16 phím khi đang bật tiếng Việt (như tên biến
  `getUserNameByIdentifier`) không còn bị cắt hay mất phím; `tuowf` ra `tườ` thay vì `từơ`. Gói dùng lõi Go
  không đổi (#78).
- Trên COSMIC, biểu tượng Ngó Sen ở khay hiện đúng chữ V có màu thay vì một ô vuông trắng (#75).
- Gõ Sen trong Ptyxis (terminal mặc định của Ubuntu 26.04 và Fedora) ra chữ lặp (`tieengs` thành
  `tieeenngss`) rồi đứng. Giờ gõ đúng dấu như ở app khác (#74).
- Lời nhắc sau khi cài hoặc cập nhật gói chỉ nói tới KDE Wayland khi máy đang chạy KDE Wayland. Trên KDE
  Wayland, lời nhắc cập nhật bảo đăng xuất rồi đăng nhập lại thay vì chạy `fcitx5 -rd`, vì fcitx5 bật
  bằng lệnh đó không gõ được trong app Qt như Kate, Dolphin (#68).

## [1.4.1-1] — 14/10/2026

Trên KDE Plasma (Wayland), lệnh cài tự chọn fcitx5 làm bàn phím ảo để app Qt như Kate, Dolphin gõ được tiếng
Việt; lệnh cài có test tự động. Bộ gõ không đổi so với 1.4.0.
Tải ở [ngosen-1.4.1-1](https://github.com/ngosen/ngosen/releases/tag/ngosen-1.4.1-1).

### Thay đổi

- Thêm test tự động cho lệnh cài trên Debian, Ubuntu, Fedora; người dùng không thấy khác gì (#65).
- Trên KDE Plasma (Wayland), lệnh cài chọn fcitx5 làm bàn phím ảo của KDE (System Settings → Virtual
  Keyboard), thêm Ngó Sen và đặt phím Ctrl+Shift như trên GNOME. Trước đây fcitx5 vẫn tự chạy nhưng app Qt
  như Kate, Dolphin không gõ được tiếng Việt cho tới khi tự chọn mục đó. Lệnh gỡ để nguyên lựa chọn này,
  vì fcitx5 có thể đang dùng cho bộ gõ khác (#64).

## [1.4.0-1] — 14/10/2026

Lệnh cài một dòng tự đặt fcitx5 làm bộ gõ trên GNOME và các desktop X11 như Xfce, Cinnamon, có thêm lệnh gỡ;
ẩn các tuỳ chọn thử nghiệm và bỏ code không dùng.
Tải ở [ngosen-1.4.0-1](https://github.com/ngosen/ngosen/releases/tag/ngosen-1.4.0-1).

### Thay đổi

- Trên desktop X11 dùng im-config như Xfce, Cinnamon, MATE (MX Linux, Linux Mint), lệnh cài đặt fcitx5
  làm bộ gõ cho tài khoản (app dùng fcitx5, fcitx5 tự chạy khi đăng nhập), thêm Ngó Sen và đặt phím
  Ctrl+Shift như trên GNOME. Trên Debian/Ubuntu, lệnh cài luôn kéo theo `im-config` và các gói giúp app
  GTK, Qt gõ được qua fcitx5, kể cả trên máy tắt cài gói đi kèm như MX. `--uninstall` trả bộ gõ về
  mặc định của máy; fcitx5 vẫn còn cài nên có thể vẫn tự chạy, gỡ hẳn bằng `sudo apt remove fcitx5` (#61).
- Bỏ mục "Đọc chữ quanh con trỏ" (`useSurroundingTextIfPossible`) ở trang Gõ chữ. Mục này mặc định tắt,
  là cách chữa tạm lỗi gõ trên Messenger từ trước khi Ngó Sen có cách sửa riêng. Ai từng bật nó thì giờ
  gõ như mọi người khác; LibreOffice vẫn xoá chữ theo cách riêng của nó như trước (#59).
- Gom các hàm xử lý đường dẫn và chuỗi bị chép ba nơi (đọc giao diện GNOME, KDE và tìm biểu tượng) về
  một chỗ; cửa sổ cài đặt thôi tự đổi tên chế độ cũ vì bộ gõ đã làm việc đó. Người dùng không thấy khác
  gì (#58).
- Bỏ code không còn dùng, người dùng không thấy khác gì: đường "Shift+Space trả lại phím gõ" trong
  hai lõi (không chỗ nào bật), công cụ so lõi `misc/core-compare`, script `translate.sh` (chạy nhầm sẽ
  ghi đè bản dịch), vài hàm và nhánh không ai gọi trong bộ gõ và cửa sổ cài đặt (#57).
- Cửa sổ cấu hình chung của fcitx5 không còn hiện 10 mục "Experiment" (thời gian chờ khi xoá chữ, gõ
  đè trong ô chat Messenger). Giá trị vẫn giữ nguyên và vẫn chỉnh được trong `ngosen.conf` khi cần dò
  lỗi (#56).
- Lệnh cài có thêm `--uninstall`: gỡ gói Ngó Sen và trả lại những gì lệnh cài đã đổi trên GNOME, để
  lần đăng nhập sau GNOME dùng lại IBus. Trên GNOME, lệnh cài đặt phím bật/tắt tiếng Việt là Ctrl+Shift
  thay cho Ctrl+Space, vì Ctrl+Space là phím gợi ý code của VS Code; ai đã tự chọn phím thì giữ nguyên.
  Cài xong, lệnh nhắc tài khoản khác trên máy phải tự chạy lại lệnh cài (#55).
- Trên GNOME không có khay hệ thống (như Fedora), lệnh cài thêm extension Input Method Panel từ
  extensions.gnome.org, để biểu tượng và menu của bộ gõ hiện trên thanh trên cùng. Ubuntu đã có khay nên
  không cài thêm (#54).
- Trên GNOME, lệnh cài một dòng tự đặt fcitx5 làm bộ gõ thay IBus (tự chạy khi đăng nhập, đặt biến
  môi trường cho app), thêm Ngó Sen vào danh sách bộ gõ, ẩn các biểu tượng fcitx5 thừa trong danh sách
  app và bật extension sửa lỗi gõ trên GNOME 50.0 đến 50.3. Cài xong chỉ cần đăng xuất rồi đăng nhập lại.
  Thêm `--no-setup` để chỉ cài gói như trước (#52).

### Sửa

- Gỡ rồi cài lại trên GNOME không còn làm mất Ngó Sen khỏi danh sách bộ gõ. Nếu fcitx5 đang chạy mà
  không có Ngó Sen, nó ghi đè danh sách khi tắt; lệnh cài giờ tắt fcitx5 trước khi đọc danh sách (#60).
- Build gói lại được: repo `LotusInputMethod/bamboo-core` (lõi Go) đã bị xoá nên mọi lần build đều lỗi
  ngay bước tải code. Lõi Go giờ lấy từ bản sao
  [ngosen/bamboo-go-core](https://github.com/ngosen/bamboo-go-core), đúng code như trước. Lõi Rust vẫn
  lấy từ `nguyen10t2/bamboo_core` (#53).

## [1.3.0-1] — 13/10/2026

Thêm tuỳ chọn Backspace chỉ xoá dấu thanh, icon mới hai cọng ngó sen và sửa icon khay trên panel tối.
Tải ở [ngosen-1.3.0-1](https://github.com/ngosen/ngosen/releases/tag/ngosen-1.3.0-1).

### Thêm

- Tuỳ chọn "Backspace xóa dấu thanh" (trang Gõ chữ, nhóm Cách bỏ dấu, mặc định tắt): bấm Backspace lúc đang
  gõ chữ `tiếng` thì ra `tiêng`; chữ không có dấu thanh thì Backspace xoá như thường. Khi tắt, phím `z`
  (Telex) hay `0` (VNI) vẫn xoá dấu thanh như trước. Đề xuất ở #34 (#37).
- [Version thử](https://github.com/ngosen/ngosen/releases/tag/next-build) build tự động mỗi khi nhánh `next` thay
  đổi, cài bằng một lệnh trong README. Số version có thêm ngày giờ và mã commit, nên cài đè lên version chính
  thức được, và version chính thức sau đó vẫn tự nâng lên (#38, #41, #49).

### Thay đổi

- Icon mới: hai cọng ngó sen chụm thành chữ V, thay cho lát củ sen 7 lỗ. Cửa sổ cài đặt dùng bản đầy
  đủ có đổ bóng; icon trên khay (khi bật "Sử dụng biểu tượng trạng thái Ngó Sen") dùng bản giản lược,
  màu xám khi tắt tiếng Việt (#43).
- Khay mặc định hiện icon Ngó Sen có nền màu, rõ trên panel sáng lẫn tối. Máy đã lưu cài đặt Ngó Sen
  thì giữ lựa chọn cũ; muốn đổi thì bật "Sử dụng biểu tượng trạng thái Ngó Sen" trong cài đặt (#45).

### Sửa

- Dòng gợi ý dưới tuỳ chọn trong cửa sổ cài đặt tự xuống dòng khi không đủ chỗ, thay vì bị cắt và bắt cuộn
  ngang (#39).
- Lời nhắc sau khi cài hoặc cập nhật nói thêm: trên KDE Wayland phải đăng xuất rồi đăng nhập lại, vì
  `fcitx5 -rd` không thay được fcitx5 do KDE mở, nên cửa sổ cài đặt chưa hiện tuỳ chọn mới (#42).
- Chữ V trên khay không còn màu đen chìm trên panel tối của Linux Mint, MX Linux và các desktop khác
  có panel tối. Ngó Sen đọc màu panel của Cinnamon như đã làm với KDE và GNOME; ở desktop khác, icon
  mặc định màu trắng. Panel sáng thì chọn "Màu biểu tượng: Sáng" trong cài đặt (#44).
- Icon trên khay nhỏ lại cho cân với các icon bên cạnh, và hết mờ trên panel Xfce. Chữ V trắng (khi tắt
  "Sử dụng biểu tượng trạng thái Ngó Sen") giờ cũng là hai cọng ngó như logo (#46).
- Cập nhật từ version trước 1.1.0 rồi chạy `fcitx5 -rd` như lời nhắc sau khi cài không còn làm mất Ngó Sen
  khỏi danh sách bộ gõ (#47).

## [1.2.2-1] — 11/10/2026

Version vá nhỏ: chữ `vi` thay hình bông sen cạnh con trỏ, và hết lỗi mất dấu khi gõ trên thanh địa chỉ Edge.
Tải ở [ngosen-1.2.2-1](https://github.com/ngosen/ngosen/releases/tag/ngosen-1.2.2-1).

### Thay đổi

- Khi bật Ngó Sen, ô nhỏ cạnh con trỏ hiện `vi` thay cho hình bông sen 🪷, giống bàn phím tiếng Anh hiện
  `us` (#28).
- Gõ trên thanh địa chỉ Edge khi đang có gợi ý tự điền không còn mất dấu: trước đây gõ `ngos` có lúc ra
  `ngos` thay vì `ngó`, vì Ngó Sen tưởng con trỏ nhích theo gợi ý là một cú bấm chuột (#29).

## [1.2.1-1] — 11/10/2026

Version vá nhỏ: cửa sổ cài đặt bỏ trang Ứng dụng, còn 4 trang. Bộ gõ không đổi. Tải ở
[ngosen-1.2.1-1](https://github.com/ngosen/ngosen/releases/tag/ngosen-1.2.1-1).

### Thay đổi

- Cửa sổ cài đặt bỏ trang Ứng dụng, còn 4 trang. Chọn chế độ trong menu chế độ khi đang ở một app thì Ngó
  Sen vẫn nhớ cho app đó; chọn "Chế độ gõ mặc định" trong menu đó để app quay về chế độ chung. Nút Mặc
  định vẫn xoá mọi chế độ đã nhớ (#26).

## [1.2.0-1] — 11/10/2026

Version 1.2: cửa sổ cài đặt mới theo giao diện máy, lưu ngay khi đổi và có Hoàn tác; hết lỗi đảo chữ khi gõ
nhanh trong gnome-terminal. Tải ở [ngosen-1.2.0-1](https://github.com/ngosen/ngosen/releases/tag/ngosen-1.2.0-1).

### Thay đổi

- README và mô tả gói bỏ các câu kể Ngó Sen tách từ đâu; mục Nguồn gốc còn một dòng ghi công. Trang
  Giới thiệu trong phần cài đặt ghi người phát triển, bên dưới là một dòng ghi công thay cho danh sách
  tác giả fcitx5-lotus. Mẫu báo lỗi và hướng dẫn đóng góp trỏ về repo của Ngó Sen (#10).
- Cửa sổ cài đặt nhanh hơn: tìm trong từ điển riêng không còn khựng mỗi phím khi có vài nghìn từ, đổi chế
  độ gõ cho một app không còn đọc lại toàn bộ danh sách app đã cài (#13).
- Cửa sổ cài đặt lưu ngay khi đổi, không còn nút OK, Áp dụng, Huỷ. Mỗi lần lưu hiện dòng "Đã lưu" kèm nút
  Hoàn tác để khôi phục đúng cài đặt trước đó, kể cả luật app, gõ tắt và từ điển riêng. Khi fcitx5 không chạy,
  cửa sổ báo ngay là thay đổi sẽ không được lưu (#14).
- Trang Gõ chữ gộp trang Tổng quan: Chế độ gõ, Kiểu gõ, Bảng mã nằm trên cùng, các tuỳ chọn chia nhóm Cách
  bỏ dấu và Gõ nhanh, tên ngắn hơn, có dòng gợi ý ngay dưới tuỳ chọn thay cho dấu hỏi, và ô Gõ thử ở cuối.
  Cửa sổ hẹp thì tên tuỳ chọn nằm trên ô chỉnh (#15).
- Cửa sổ cài đặt còn 5 trang: Gõ chữ, Ứng dụng, Gõ tắt & Từ điển, Phím, Khác. Sao lưu và Khôi phục thành
  hai nút ở thanh dưới; khôi phục áp dụng ngay và có Hoàn tác (#16).
- Ô tìm cài đặt ở đầu thanh bên: gõ không dấu cũng tìm được ("chinh ta" ra "Kiểm tra chính tả"), chọn kết
  quả thì mở đúng trang, đúng tab và đưa con trỏ tới tuỳ chọn đó (#17).
- Cửa sổ cài đặt mang dấu hiệu Ngó Sen: biểu tượng và chữ "Ngó Sen" đầu thanh bên, chế độ gõ là một hàng
  nút với nút đang chọn màu xanh men, chữ ví dụ (`vieetj → việt`) và ô Gõ thử dùng phông Xanh Mono đi kèm
  gói (giấy phép OFL) (#18).
- Cửa sổ cài đặt dùng giao diện của hệ thống (KDE, GNOME, sáng hay tối) thay cho màu và viền tự vẽ: danh
  sách, bảng, thẻ chế độ của app và trang Giới thiệu theo theme đang dùng. Trang nằm trong tab không còn
  lặp lại tên tab làm tiêu đề (#19).
- Cửa sổ cài đặt dùng bàn phím dễ hơn: Ctrl+F đưa con trỏ vào ô tìm, phím Tab đi qua nội dung trang trước
  rồi mới tới các nút dưới cùng. Trên GNOME, mục Khác ở thanh bên có biểu tượng.
  Dòng gợi ý dưới tuỳ chọn không còn khoảng trống thừa phía trên. Với theme biểu tượng có sẵn biểu tượng
  fcitx (Papirus, Colloid), cửa sổ hiện đúng logo Ngó Sen thay vì logo fcitx (#20).
- README chỉ đúng chỗ chọn chế độ Gõ Sen trong cửa sổ cài đặt mới (#21).

### Sửa

- Gõ nhanh trong gnome-terminal và các app GTK3 khác, bấm chồng hai phím có lúc làm đảo chữ: "rồi" ra
  "riồ", "ngày" ra "ngyà". Phím bấm trong lúc Ngó Sen đang thay chữ giờ được gửi chung với chữ đó (#11).
- Cửa sổ cài đặt: nút Reset đưa mọi trang về mặc định, kể cả trang chưa mở, luật cho từng app và từ điển
  riêng; trước đây nó gần như không đổi gì. Sửa từ điển riêng có tác dụng ngay, không phải khởi động lại
  fcitx5. Đặt phím mở menu chế độ không còn xoá các phím khác đã đặt. Chế độ gõ mặc định chỉ còn chỉnh ở
  trang Chung, không còn hai chỗ ghi đè nhau (#12).
- Cửa sổ cài đặt: mở trang Gõ tắt & Từ điển rồi đổi một cài đặt khác không còn tắt từ điển riêng và ghi lại
  danh sách từ. Đổi sáng/tối khi cửa sổ đang mở thì dòng gợi ý và hàng nút chế độ đổi màu theo, không còn
  chữ tối trên nền tối (#22).
- Cửa sổ cài đặt: chế độ đang chọn cho một app có màu xanh men giống trang Gõ chữ. Khi fcitx5 không nhận
  thay đổi, cửa sổ hiện lại cài đặt fcitx5 đang giữ thay vì giá trị chưa lưu. Khi fcitx5 không chạy chỉ
  còn một dòng báo. Dòng mô tả ở trang Giới thiệu nằm trên một hàng (#23).

## [1.1.0-1] — 10/10/2026

Version 1.1: file cài vào máy mang tên `ngosen`, cấu hình cũ tự chuyển sang; icon khay mới; thêm nhật ký gõ
để gửi kèm khi báo lỗi. Tải ở [ngosen-1.1.0-1](https://github.com/ngosen/ngosen/releases/tag/ngosen-1.1.0-1).

### Thêm

- Nhật ký gõ: vừa gõ ra chữ sai thì chọn "Lưu nhật ký gõ" trong menu fcitx5 ở khay hệ thống. Ngó Sen lưu
  vài trăm phím gần nhất, những gì bộ gõ gửi cho app và những gì app báo lại vào một file trong
  `~/.local/state/ngosen/`, để gửi kèm khi báo lỗi. Nhật ký chỉ nằm trong bộ nhớ cho tới lúc lưu, và
  không ghi gì gõ trong ô mật khẩu ([#93](https://github.com/ngosen/ngosen-fork-archive/pull/93)). Gõ
  lâu trong một app thì file vẫn ghi app đó và cách nó nối với bộ gõ, để chạy lại được các phím (#4).

### Thay đổi

- Mọi file Ngó Sen cài vào máy đều mang tên riêng (`ngosen` thay cho `lotus`), nên gói cài chung được với
  `fcitx5-lotus`. Lần đầu fcitx5 chạy sau khi cập nhật, Ngó Sen tự chép cấu hình `lotus*.conf` sang
  `ngosen*.conf` và đổi tên trong danh sách bộ gõ, nên không phải thêm lại bộ gõ. Log bật bằng
  `fcitx5 --verbose ngosen=4` (#7).
- Icon trên khay hệ thống được vẽ mới: lát cắt ngó sen trên nền xanh, xám đi khi tắt gõ tiếng Việt, mặt cười
  ở chế độ emoji. Kiểu icon chữ (V, E) giữ nguyên (#8).
- Lõi gõ Rust, lõi mà mọi gói cài đặt đang dùng, lấy bamboo_core 0.3.26 thẳng từ tác giả gốc: version đã gộp
  mọi bản sửa của Ngó Sen và nhanh hơn ở Backspace cùng vài chữ có dấu. Chữ gõ ra giống hệt version trước ([#97](https://github.com/ngosen/ngosen-fork-archive/pull/97)).

### Test

- Test gõ ngẫu nhiên kiểu Gõ Sen vào các app giả có tật như app thật: báo chữ chậm, báo chữ cũ, không báo,
  nhận phím chậm. Chữ cuối cùng trong app phải giống hệt chữ Telex của các phím đã gõ. `core_replay` chạy
  lại một file nhật ký gõ trên app giả để tìm lỗi người dùng gửi về ([#94](https://github.com/ngosen/ngosen-fork-archive/pull/94)).
- Test gõ ngẫu nhiên kiểu Gõ Sen xen lẫn click chuột vào giữa chữ cũ, Alt+Tab và click sang app khác, trên
  app X11, Wayland và XIM. Chữ gõ sau mỗi cú click phải giống như gõ thong thả tại đúng chỗ đó ([#95](https://github.com/ngosen/ngosen-fork-archive/pull/95)).
- Phần quyết định giữ hay bỏ chữ đang gõ khi rời ô gõ rồi quay lại được chuyển vào lõi bộ gõ, để test trên
  app giả chạy đúng đoạn code fcitx5 chạy. Gõ không thay đổi gì ([#96](https://github.com/ngosen/ngosen-fork-archive/pull/96)).

### Tài liệu

- Repo được tạo lại ngoài mạng fork của fcitx5-lotus, giữ nguyên lịch sử commit và các release.
  Pull request và issue đến số 98 nằm ở repo lưu trữ
  [ngosen/ngosen-fork-archive](https://github.com/ngosen/ngosen-fork-archive); các link PR cũ trong file
  này trỏ về đó (#1).
- Câu giới thiệu trong README nay là "Bộ gõ tiếng Việt cho fcitx5 trên Linux." thay cho "tối ưu cho
  Linux", và điểm đầu tiên nay là "Không cần quyền đặc biệt" (#3).
- README sửa chữ ở đầu trang: Gõ Sen dùng được cho "hầu hết app" thay cho "mọi app", ghi rõ Sway và
  Hyprland vẫn gõ được qua fcitx5, và dùng "test", "so hash" như trang web (#6).

## [1.0.0-1] — 09/10/2026

Version 1.0: Ngó Sen chỉ làm cho fcitx5. Click sang ô khác rồi gõ giờ đã ra đúng chữ trong LibreOffice Calc, WPS Office
và Google Sheets. Tải ở [ngosen-1.0.0-1](https://github.com/ngosen/ngosen/releases/tag/ngosen-1.0.0-1).

### Thêm

- Gói Nix cho NixOS, build từ code của Ngó Sen (gói Nix cũ tải mã bản gốc nên đã gỡ ở [#21](https://github.com/ngosen/ngosen-fork-archive/pull/21)). Cách cài nằm
  trong README. CI build gói, chạy test và nạp nó vào fcitx5 trên màn hình X ảo ([#82](https://github.com/ngosen/ngosen-fork-archive/pull/82)).
- Hướng dẫn tự build trên NixOS trong `TU-DUNG.md`, và `nix develop` mở shell có đủ công cụ build ([#83](https://github.com/ngosen/ngosen-fork-archive/pull/83)).

### Sửa

- Google Sheets trên Wayland: click chuột sang ô khác rồi gõ, sau vài ô thì chữ ra lẫn chữ của ô trước và
  không thành tiếng Việt (vd. `afoc`, `ieengs`). Nay mỗi ô mới bắt đầu một chữ mới ([#86](https://github.com/ngosen/ngosen-fork-archive/pull/86)).
- LibreOffice Calc trên Wayland: click chuột sang ô khác rồi gõ, sau chữ có dấu thì bộ gõ xoá nhầm và mở
  hộp thoại "Delete Contents", từ đó không gõ được tiếng Việt nữa. Nay phím đầu ở ô mới bắt đầu một chữ
  mới ([#87](https://github.com/ngosen/ngosen-fork-archive/pull/87)).
- LibreOffice Calc mở thẳng trên Wayland (không qua module Qt của fcitx): click sang ô khác rồi gõ thì chữ
  ra lẫn chữ của ô trước hoặc hộp thoại "Delete Contents" mở ra. Nay bộ gõ hỏi Calc đang ở ô nào trước khi
  sửa chữ, nên mỗi ô mới bắt đầu một chữ mới ([#90](https://github.com/ngosen/ngosen-fork-archive/pull/90)).
- WPS Office: gõ hai phím giống nhau liền nhau (`dd`, `ee`, `oo`, `aa`, `ww`) thì phím thứ hai bị mất, nên
  `dd` không ra `đ` và `oo` không ra `ô`. Nay chỉ app gõ qua XIM mới bị coi là "gửi trả phím" ([#90](https://github.com/ngosen/ngosen-fork-archive/pull/90)).
- App X11 chạy trên phiên Wayland (WPS Office qua Xwayland): click chuột sang ô khác rồi gõ thì chữ ra lẫn
  chữ của ô trước. Nay bộ gõ nghe cú click trên cửa sổ Xwayland như trên phiên X11 ([#90](https://github.com/ngosen/ngosen-fork-archive/pull/90)).

### Thay đổi

- Phần xử lý gõ được tách khỏi fcitx5 và sắp xếp lại. Người dùng không thấy gì khác; phần này giờ có test
  riêng, chạy được trên máy không cài fcitx5 ([#77](https://github.com/ngosen/ngosen-fork-archive/pull/77), [#78](https://github.com/ngosen/ngosen-fork-archive/pull/78), [#79](https://github.com/ngosen/ngosen-fork-archive/pull/79)).

### Tài liệu

- Thêm `RELEASING.md`: những chỗ cần cập nhật sau mỗi PR và mỗi lần ra version mới; mẫu PR nay đúng với
  fork (nhánh `main`, `ctest`, dòng CHANGELOG) ([#80](https://github.com/ngosen/ngosen-fork-archive/pull/80)).
- README không còn gọi Hyprland là compositor wlroots: Hyprland đã bỏ wlroots từ version 0.42, nhưng vẫn
  có giao thức input-method-v2 mà Ngó Sen cần ([#81](https://github.com/ngosen/ngosen-fork-archive/pull/81)).
- README bỏ câu "không chạy ngầm với quyền root": uinput server của version cũ chạy bằng tài khoản
  `uinput_proxy`, không phải root. Điểm đầu tiên nay chỉ ghi "Bỏ hẳn uinput server" ([#85](https://github.com/ngosen/ngosen-fork-archive/pull/85)).
- README bỏ điểm "Chuyển từ fcitx5-lotus không mất gì" khỏi danh sách đầu trang; mục Cài vẫn ghi gói tự
  thay version cũ và giữ cấu hình ([#88](https://github.com/ngosen/ngosen-fork-archive/pull/88)).
- README bỏ roadmap bản IBus và bản chạy thẳng trên Sway, Hyprland: Ngó Sen chỉ làm cho fcitx5. Nhánh
  `feat/ibus-engine` giữ lại, không cập nhật nữa ([#89](https://github.com/ngosen/ngosen-fork-archive/pull/89)).

## [0.5.1-1] — 09/10/2026

Sửa lỗi Gõ Sen trên Sway và Hyprland, và cài mới thì dùng Gõ Sen luôn. Tải ở
[ngosen-0.5.1-1](https://github.com/ngosen/ngosen/releases/tag/ngosen-0.5.1-1).

### Thay đổi

- Cài mới thì chế độ mặc định là Gõ Sen thay cho Preedit, khớp với cửa sổ cài đặt. Ai đã chọn chế độ thì
  giữ nguyên ([#74](https://github.com/ngosen/ngosen-fork-archive/pull/74)).

### Sửa

- Chế độ Gõ Sen gõ được trên Sway, Hyprland và các compositor dùng input-method-v2. Trước đây chữ đầu tiên cần
  thêm dấu làm bộ gõ kẹt, mọi phím sau đó không ra chữ ([#73](https://github.com/ngosen/ngosen-fork-archive/pull/73)).

### Tài liệu

- README mở đầu bằng những điểm chính của Ngó Sen và roadmap tới bản IBus và bản cho wlroots không cần
  fcitx5 ([#75](https://github.com/ngosen/ngosen-fork-archive/pull/75)). README gọi server cũ là uinput server, như trên website ([#76](https://github.com/ngosen/ngosen-fork-archive/pull/76)).

## [0.5.0-1] — 08/10/2026

Version đầu tiên đánh số riêng; số `0.5` ứng với khoảng nửa chặng đường tới version 1.0 có bản IBus cho GNOME. Tải ở
[ngosen-0.5.0-1](https://github.com/ngosen/ngosen/releases/tag/ngosen-0.5.0-1).

### Thêm

- README có mục cho VS Code trên Ubuntu: nên cài bằng gói `.deb` hoặc Flatpak; bản snap cần sửa lối tắt
  để gõ được tiếng Việt qua XIM ([#56](https://github.com/ngosen/ngosen-fork-archive/pull/56), [#62](https://github.com/ngosen/ngosen-fork-archive/pull/62)).
- README có mục "Gỡ": tắt uinput server trước, lệnh gỡ gói cho từng distro, lệnh gỡ khi tự build,
  và hai thứ còn lại trên máy sau khi gỡ là file cấu hình và tài khoản `uinput_proxy` ([#27](https://github.com/ngosen/ngosen-fork-archive/pull/27)).
- Trang Giới thiệu trong cửa sổ cài đặt có lại hai nút "Báo cáo lỗi" và "Đề xuất tính năng"; chúng mở
  mẫu tương ứng ở mục Issues của repo `ngosen/ngosen` ([#31](https://github.com/ngosen/ngosen-fork-archive/pull/31)).
- Công cụ `misc/core-compare` gõ cùng một bộ phím vào lõi ghép dấu hiện tại (Go) và bản Rust
  `bamboo-core`, rồi chỉ ra chỗ hai bên ra chữ khác nhau. Chỉ dùng khi phát triển, không đi kèm bộ gõ
  ([#68](https://github.com/ngosen/ngosen-fork-archive/pull/68)).

### Thay đổi

- Phần mã gửi chữ, phím và lệnh xoá ra app đi qua một lớp trung gian riêng, bước đầu để sau này chạy
  được trên IBus. Người dùng không thấy gì khác ([#47](https://github.com/ngosen/ngosen-fork-archive/pull/47)).
- Phần mã đọc chữ quanh con trỏ, loại ô nhập và tên app cũng đi qua lớp trung gian đó; các chỗ nhận
  diện app riêng (LibreOffice, Firefox, Chromium, SDL, GTK4) gom về một file. Người dùng không thấy gì
  khác ([#48](https://github.com/ngosen/ngosen-fork-archive/pull/48)).
- Chữ tạm (chế độ Preedit và Emoji) và việc làm mới bảng gợi ý cũng đi qua lớp trung gian. Người dùng
  không thấy gì khác ([#49](https://github.com/ngosen/ngosen-fork-archive/pull/49)).
- Bảng chọn emoji cũng đi qua lớp trung gian, kèm test cho các phím của bảng chọn (số, Space, Tab,
  mũi tên, lật trang, Enter, Esc). Người dùng không thấy gì khác ([#50](https://github.com/ngosen/ngosen-fork-archive/pull/50)).
- Phần gõ đọc cài đặt từ một bản sao riêng, cập nhật mỗi khi đổi cài đặt (cửa sổ cài đặt, nút bật/tắt
  trên menu, menu bảng mã). Người dùng không thấy gì khác ([#51](https://github.com/ngosen/ngosen-fork-archive/pull/51)).
- Hẹn giờ và việc nghe app báo chữ quanh con trỏ cũng đi qua lớp trung gian. Người dùng không thấy gì
  khác ([#52](https://github.com/ngosen/ngosen-fork-archive/pull/52)).
- Phần gõ có hàm riêng để đọc chữ UTF-8, xem giờ và ghi log, không dùng hàm của fcitx5 nữa. Người dùng
  không thấy gì khác, kể cả dòng log ([#53](https://github.com/ngosen/ngosen-fork-archive/pull/53)).
- Phím bấm tới phần gõ qua lớp trung gian, không còn là sự kiện phím của fcitx5; kèm test cho việc
  đọc và đổi phím (phím bổ trợ, Shift, tên phím, tự viết hoa). Người dùng không thấy gì khác ([#54](https://github.com/ngosen/ngosen-fork-archive/pull/54)).
- Phần gõ lấy từ điển, bảng gõ tắt, bảng phím tự đặt và danh sách emoji qua lớp trung gian thay vì đọc
  thẳng từ engine fcitx5; kèm test cho bảng phím tự đặt. Người dùng không thấy gì khác ([#55](https://github.com/ngosen/ngosen-fork-archive/pull/55)).
- Phần gõ dùng tên phím riêng thay cho tên phím của fcitx5; lúc build, máy tự so từng phím với số của
  fcitx5. Người dùng không thấy gì khác ([#63](https://github.com/ngosen/ngosen-fork-archive/pull/63)).
- Phần gõ của mỗi ô nhập không còn là một kiểu dữ liệu của fcitx5; fcitx5 chỉ giữ nó trong một lớp vỏ
  mỏng. Người dùng không thấy gì khác ([#64](https://github.com/ngosen/ngosen-fork-archive/pull/64)).
- Phần gõ nay build được mà không cần file nào của fcitx5: danh sách chế độ gõ, các cờ dùng chung và các
  hàm xử lý chữ đã dời sang file riêng; dòng "Trang 1/2" của bảng emoji nhờ lớp trung gian dịch. Người
  dùng không thấy gì khác ([#65](https://github.com/ngosen/ngosen-fork-archive/pull/65)).
- Phần gõ dời vào thư mục `src/core/` và được build thành thư viện riêng không nối với fcitx5; lỡ dùng
  file của fcitx5 ở đó thì build báo lỗi ngay. Người dùng không thấy gì khác ([#66](https://github.com/ngosen/ngosen-fork-archive/pull/66)).
- File phần gõ (khoảng 1.800 dòng) được chia thành năm file theo việc: nhận phím, emoji, chờ app, sửa
  chữ cũ, gõ tắt. Thân các hàm giữ nguyên. Người dùng không thấy gì khác ([#67](https://github.com/ngosen/ngosen-fork-archive/pull/67)).
- Thêm lõi ghép dấu viết bằng Rust (thư viện `bamboo-core`), chưa bật: build vẫn dùng lõi Go. Muốn
  thử thì build với `-DNGOSEN_RUST_CORE=ON` (cần Rust 1.88+, không cần mạng vì thư viện phụ thuộc nằm sẵn
  trong `bamboo-rs/vendor`). Người dùng không thấy gì khác
  ([#69](https://github.com/ngosen/ngosen-fork-archive/pull/69)).
- Gói cho Arch, Fedora và openSUSE dùng lõi ghép dấu Rust, không cần Go để build nữa. Gói Debian và
  Ubuntu vẫn dùng lõi Go vì Rust có sẵn ở đó còn cũ. Gõ chữ không khác gì ([#70](https://github.com/ngosen/ngosen-fork-archive/pull/70)).
- Gói `.deb` cho Ubuntu 24.04, 25.10, 26.04 và Debian testing/unstable cũng dùng lõi Rust. Debian 12, 13
  và Ubuntu 22.04 vẫn dùng lõi Go ([#71](https://github.com/ngosen/ngosen-fork-archive/pull/71)).
- Chế độ `Uinput` đổi tên thành **Gõ Sen** (`Mode=Sen` trong `lotus.conf`). Cấu hình cũ, thứ tự chế độ,
  phím tắt và tuỳ chọn ẩn/hiện của chế độ cũ tự chuyển sang tên mới ([#43](https://github.com/ngosen/ngosen-fork-archive/pull/43)).
- Nhánh chính đổi tên từ `ban-dung` thành `main`. Đường dẫn cũ trên GitHub tự chuyển sang tên mới. Ai
  đã tải mã về máy thì chạy `git branch -m ban-dung main && git fetch origin && git branch -u origin/main main` ([#36](https://github.com/ngosen/ngosen-fork-archive/pull/36)).
- Mục Issues của repo được bật để nhận báo lỗi. Mẫu báo lỗi hỏi version và cách cài của Ngó Sen thay
  cho các cách cài của bản gốc; README chỉ tới mục Issues ([#30](https://github.com/ngosen/ngosen-fork-archive/pull/30)).
- Trên phiên X11 thật (Xfce, Cinnamon, MATE…), app nối với bộ gõ qua XIM cũng xoá chữ bằng forward key,
  không cần uinput server; trước đây chỉ làm vậy dưới XWayland ([#35](https://github.com/ngosen/ngosen-fork-archive/pull/35)).
- App cài bằng snap (Firefox, Chromium…) nối với bộ gõ qua module fcitx đời cũ có sẵn trong gói snap;
  giờ chúng cũng xoá chữ bằng forward key, không cần uinput server. Game dùng SDL 2.0.12 trở về trước
  nói cùng giao thức đó vẫn đi server ([#35](https://github.com/ngosen/ngosen-fork-archive/pull/35)).

### Bỏ

- Uinput server `fcitx5-lotus-server`. Bộ gõ xoá chữ cũ bằng forward key qua fcitx5, hoặc bấm phím qua
  XTEST trên phiên X11, nên không còn chương trình chạy ngầm, tài khoản `uinput_proxy` hay quyền thiết
  bị. Cập nhật từ version cũ thì gói tự tắt service cũ và xoá tài khoản đó. Game dùng SDL ngoài X11 không
  nhận forward key nên gõ ra chữ không dấu ([#43](https://github.com/ngosen/ngosen-fork-archive/pull/43)).
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
- Trên Wayland, bấm chuột sang chỗ khác trong ô rồi gõ tiếp giờ bắt đầu từ mới ngay cả khi server
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
- Trên KDE Plasma (Wayland), bộ gõ tự gửi phím xoá cho app thay vì nhờ uinput server, rồi chờ app
  báo đã xoá xong mới gõ chữ mới. App X11 chạy qua XWayland không còn bị kẹt, nuốt hết phím
  sau một lần thay chữ. Ô soạn tin Facebook trên KDE xoá rồi gõ lại như ô thường, không bôi đen rồi gõ
  đè nữa ([#33](https://github.com/ngosen/ngosen-fork-archive/pull/33)).
- Trên GNOME, gõ nhanh trong Firefox không còn mất chữ ("viet" ra "v"), ô soạn tin Facebook trên Edge
  không còn ra "i" thay cho "đi", và chuyển từ app khác sang terminal không còn làm rơi chữ. Luật theo
  app nhận cả tên GNOME báo kèm đuôi `.desktop` (như `firefox.desktop`) ([#33](https://github.com/ngosen/ngosen-fork-archive/pull/33)).
- App nối với bộ gõ qua IBus hoặc qua module fcitx5 (`GTK_IM_MODULE=ibus`/`fcitx`, ví dụ Zalo và các
  app Electron chạy X11), và mọi app trên GNOME, gõ được mà không cần uinput server. App GTK4 xoá chữ
  bằng surrounding text vì module GTK4 bỏ qua forward key. Game dùng SDL vẫn đi server vì SDL chỉ
  nhận chữ ghi và chữ gạch chân ([#34](https://github.com/ngosen/ngosen-fork-archive/pull/34)).
- Trên X11, thanh địa chỉ Chrome, Edge và Chromium không còn giữ dấu cũ khi gõ một địa chỉ đã từng vào
  ("tiêng" thay cho "tiếng"). Trình duyệt tự gợi ý phần đuôi và bôi đen nó, nên phím xoá đầu chỉ xoá phần
  gợi ý. Ngó Sen nay bôi đen cả chữ cũ bằng Shift+Mũi tên trái qua XTEST rồi gõ đè. Các app khác trên X11
  cũng xoá qua XTEST khi không forward key được, không cần uinput server. Gói nay kéo theo thư viện
  `libxcb-xtest`, vì Linux Mint không cài sẵn nó ([#38](https://github.com/ngosen/ngosen-fork-archive/pull/38)).
- Trên X11, bấm chuột sang chỗ khác giữa lúc gõ không còn làm từ mới dính vào từ cũ khi uinput server
  không chạy: Ngó Sen nghe cú bấm chuột qua XInput2 của X server. Gói kéo theo thư viện `libxcb-xinput` ([#39](https://github.com/ngosen/ngosen-fork-archive/pull/39)).

### Tài liệu

- README viết lại cho người dùng: lộ trình Ngó Sen 1.0 ở đầu, rồi cách chọn chế độ, cài và gỡ. Chi tiết
  từng vá không còn trong repo; hướng dẫn tự build từ mã chuyển sang `TU-DUNG.md` ([#42](https://github.com/ngosen/ngosen-fork-archive/pull/42)).
- README: mục lộ trình đổi tên thành "Roadmap", ghi rõ lõi ghép dấu là bamboo-core; phần nguồn gốc
  (vibecode, tách từ fcitx5-lotus) chuyển xuống cuối; ghi thêm các máy ảo dùng để thử ([#44](https://github.com/ngosen/ngosen-fork-archive/pull/44)).
- Tên mới trong mã (file, hàm, lớp, hằng, biến môi trường) không còn chữ "lotus"; tên cũ giữ nguyên
  tới khi cần sửa chỗ đó. CI kiểm tra mỗi lần đẩy mã ([#37](https://github.com/ngosen/ngosen-fork-archive/pull/37)).
- Nhật ký thay đổi có mục riêng cho version `3.5.10-4`; những gì gộp sau version đó nằm dưới "Chưa phát hành"
  ([#28](https://github.com/ngosen/ngosen-fork-archive/pull/28)).

### Test

- Bốn test khoảng chờ của Messenger không còn lỗi khi máy chạy test bị khựng vài chục ms: khoảng chờ
  trong test nới từ 40 lên 500 ms, và vòng lặp sự kiện của test chạy nốt việc đã tới hạn trước khi
  kiểm. Không đổi gì ở bộ gõ ([#32](https://github.com/ngosen/ngosen-fork-archive/pull/32)).

## [3.5.10-4] — 04/10/2026

Release đầu tiên có gói build sẵn:
[ngosen-3.5.10-4](https://github.com/ngosen/ngosen/releases/tag/ngosen-3.5.10-4).

### Thêm

- Script cài một dòng `install.sh`: nhận ra distro, tải gói build sẵn từ release mới
  nhất, đối chiếu hash, hỏi lại rồi cài và bật uinput server. Thay được gói `fcitx5-lotus` đang có,
  kể cả bản gốc số cao hơn, và ghi đè được gói build từ source trên Arch. README hướng dẫn cài theo cách
  này; hướng dẫn cập nhật khi tự build được bổ sung các lệnh nạp lại luật quyền ([#26](https://github.com/ngosen/ngosen-fork-archive/pull/26)).

### Thay đổi

- Trang Giới thiệu trong cửa sổ cài đặt trỏ về repo `ngosen/ngosen` và ghi "Dựa trên fcitx5-lotus của"
  kèm tên các tác giả gốc. Bỏ hai nút "Báo cáo lỗi" và "Đề xuất tính năng", vì chúng mở trang báo lỗi
  của bản gốc ([#20](https://github.com/ngosen/ngosen-fork-archive/pull/20)).
- Bộ gõ đổi tên hiển thị thành **Ngó Sen**: tên trong danh sách bộ gõ và addon của fcitx5, nhãn "Ngó
  Sen - Tắt", cửa sổ và mục menu "Cài đặt Ngó Sen", trang Giới thiệu. File cấu hình và tên bên trong
  giữ nguyên, nên cấu hình cũ dùng tiếp được ([#15](https://github.com/ngosen/ngosen-fork-archive/pull/15)).
- Gói Fedora đổi tên thành `fcitx5-ngosen`. Cài gói này thì gói `fcitx5-lotus` cũ của fork được thay
  tự động; gói này và `fcitx5-lotus` của bản gốc không cài chung được ([#16](https://github.com/ngosen/ngosen-fork-archive/pull/16)).
- Gói Debian/Ubuntu đổi tên thành `fcitx5-ngosen` và thay gói `fcitx5-lotus` khi cài. Khi cài hoặc
  cập nhật, gói tự gỡ tài khoản `uinput_proxy` khỏi nhóm `input` và áp luật quyền cho chuột, bàn chạm
  đang cắm, giống gói Fedora ([#23](https://github.com/ngosen/ngosen-fork-archive/pull/23)).
- Gói openSUSE đổi tên thành `fcitx5-ngosen`, có cùng các bước sau cài như gói Fedora. Thêm công thức
  gói Arch (`packaging/arch/PKGBUILD`) cho ra gói pacman `fcitx5-ngosen`; CI build thử trên
  GitHub nay build gói này thay cho file nén thô ([#24](https://github.com/ngosen/ngosen-fork-archive/pull/24)).
- Quy trình release viết lại: gắn tag `ngosen-<version>` thì GitHub kiểm số version trong các
  file đóng gói, build gói cho Fedora, Debian, Ubuntu, openSUSE và Arch, rồi tạo release nháp kèm
  `SHA256SUMS`. Bỏ repo gói có chữ ký và chìa khoá công khai của bản gốc. Các gói lên `3.5.10-4` ([#25](https://github.com/ngosen/ngosen-fork-archive/pull/25)).
- Repo chuyển về địa chỉ `github.com/ngosen/ngosen` (link cũ tự chuyển). Trang giới thiệu viết lại theo
  tên Ngó Sen và nằm ở `README.md`; README của bản gốc không còn trong repo, xem ở repo fcitx5-lotus
  ([#17](https://github.com/ngosen/ngosen-fork-archive/pull/17)).
- Cập nhật bamboo-core (lõi bộ gõ Telex/VNI) theo bản gốc ([#11](https://github.com/ngosen/ngosen-fork-archive/pull/11)).

### Bỏ

- Gói Nix (`flake.nix`, thư mục `nix/`) và hai quy trình kiểm Nix. Công thức này tải code của bản gốc
  về build chứ không build code của Ngó Sen, nên chưa bao giờ cho ra đúng Ngó Sen ([#21](https://github.com/ngosen/ngosen-fork-archive/pull/21)).
- Biến môi trường `LOTUS_SERVER_PATH`: module không còn kiểm đường dẫn của uinput server ([#9](https://github.com/ngosen/ngosen-fork-archive/pull/9)).
- Giao diện cài đặt: bỏ dòng và lời giải thích còn sót của tuỳ chọn `FixUinputWithAck` đã gỡ. Giao
  diện vốn không hiện tuỳ chọn này, vì addon không còn khai báo nó ([#14](https://github.com/ngosen/ngosen-fork-archive/pull/14)).

### Sửa

- Gói Fedora: khi cập nhật, tài khoản `uinput_proxy` tự được gỡ khỏi nhóm `input`, và luật quyền cho
  chuột, bàn chạm được áp ngay cho thiết bị đang cắm, không phải khởi động lại máy hay cắm lại ([#19](https://github.com/ngosen/ngosen-fork-archive/pull/19)).
- Uinput server giữ tối đa 1024 phím xoá trong hàng chờ và bỏ hàng chờ khi bộ gõ ngắt kết nối. Trước đây
  một chương trình gửi dồn dập có thể khiến server tiếp tục xoá chữ rất lâu sau khi nó đã dừng ([#18](https://github.com/ngosen/ngosen-fork-archive/pull/18)).
- Khi bộ gõ khởi động lại và nối lại với uinput server đúng lúc kết nối cũ vừa đứt, server không còn
  ngắt nhầm kết nối mới, và không gõ tiếp các phím xoá còn dở của kết nối cũ ([#22](https://github.com/ngosen/ngosen-fork-archive/pull/22)).
- Luật udev không còn cấp `/dev/uinput` và mọi thiết bị nhập cho cả nhóm `input`, khớp bản gốc
  (LotusInputMethod/fcitx5-lotus#525) ([#10](https://github.com/ngosen/ngosen-fork-archive/pull/10)).
- Uinput server và module bộ gõ nhận nhau theo tài khoản (uid) của tiến trình bên kia, thay cho đường
  dẫn chương trình. Server không còn giữ quyền `CAP_SYS_PTRACE`. Module nay kiểm cả socket phím
  xoá, nên chương trình chiếm tên socket trước không đọc được độ dài từng từ ([#9](https://github.com/ngosen/ngosen-fork-archive/pull/9)).
- Uinput server chỉ mở chuột, bàn chạm và núm trỏ, ở chế độ chỉ đọc; không mở bàn phím nữa, và tài
  khoản `uinput_proxy` ra khỏi nhóm `input`. Chuột kiêm bàn phím mất tính năng bấm chuột để ngắt từ.
  Máy đã cài từ trước cần chạy một lần `sudo gpasswd -d uinput_proxy input` ([#8](https://github.com/ngosen/ngosen-fork-archive/pull/8)).
- Uinput server chỉ nhận số phím từ 1 tới 1024 (backspace) hoặc từ -1 tới -1024 (bôi đen); số khác bị
  bỏ qua và ghi log. Trước đây một số âm rất lớn làm server chết, bàn phím chết theo ([#7](https://github.com/ngosen/ngosen-fork-archive/pull/7)).
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

### Sửa

- Server xử lý hết hàng sự kiện libinput ở mỗi vòng lặp, không để sự kiện nằm chờ
  (LotusInputMethod/fcitx5-lotus#507).
- Cách chặn lặp chữ do gợi ý tự điền chỉ bật ở thanh địa chỉ trình duyệt.

### Tài liệu

- File `KHAC-GI-SO-VOI-BAN-GOC.md`: từng miếng vá, vì sao có, đã gửi lên bản gốc chưa (không còn trong repo từ
  [#42](https://github.com/ngosen/ngosen-fork-archive/pull/42)).
- [AGENTS.md](AGENTS.md): quy định khi sửa fork và khi gửi vá lên bản gốc ([#3](https://github.com/ngosen/ngosen-fork-archive/pull/3)).
- README: cài sang máy khác, chế độ Uinput duy nhất, khởi động lại server sau khi cập nhật.
