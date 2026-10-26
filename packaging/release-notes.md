Version 1.6.1 phát hành ngày 26/10/2026, sửa gợi ý Cứu chữ / Cứu dấu để theo ngôn ngữ giao diện của Ngó Sen, mặc định tiếng Việt (#322); Ngó Sen tự có trong danh sách bộ gõ khi fcitx5 chạy lần đầu sau khi cài bằng package trên desktop tiếng Việt (#300, #316); chữ trong ô mật khẩu không còn bị ghi vào log gỡ lỗi (#321). Chi tiết ở [CHANGELOG.md](https://github.com/ngosen/ngosen/blob/main/CHANGELOG.md).

## Cài và cập nhật

Một lệnh cho mọi distro. Lệnh tự chọn đúng gói và hỏi lại trước khi cài:

```
curl -fsSL https://raw.githubusercontent.com/ngosen/ngosen/main/install.sh | bash
```

Cài lần đầu thì đăng xuất rồi đăng nhập lại, sau đó nhấn rồi thả Ctrl+Shift để chuyển giữa tiếng Anh và tiếng Việt. Đã cài từ trước thì vẫn dùng phím chuyển cũ, thường là Ctrl+Space.

Cập nhật thì chạy lại đúng lệnh đó: fcitx5 tự nạp version mới, gõ được ngay. Trên KDE Wayland thì đăng xuất rồi đăng nhập lại.

## Tải gói bằng tay

Chọn file theo distro:

- Fedora 44, 43: `fcitx5-ngosen-*.fc44.x86_64.rpm`, `fcitx5-ngosen-*.fc43.x86_64.rpm`
- Arch, CachyOS: `fcitx5-ngosen-*-x86_64.pkg.tar.zst`
- openSUSE Tumbleweed: `fcitx5-ngosen-*.opensuse-tumbleweed.x86_64.rpm`
- Ubuntu 26.10, 26.04, 24.04, 22.04: `…_stonking_amd64.deb`, `…_resolute_amd64.deb`, `…_noble_amd64.deb`, `…_jammy_amd64.deb`
- Debian 13, 12: `…_trixie_amd64.deb`, `…_bookworm_amd64.deb`
- Kali Rolling: `…_forky_amd64.deb`
- Máy ARM 64-bit: file cùng tên với đuôi `_arm64.deb`, hoặc `.aarch64.rpm` cho Fedora

Cài gói bằng tay trên Ubuntu 26.04 thì bật thêm [extension sửa lỗi gõ](https://github.com/ngosen/ngosen#ubuntu-2604-extension-sửa-lỗi-gõ).

Đối chiếu file tải về với `SHA256SUMS`: `sha256sum -c SHA256SUMS --ignore-missing`.

## Đã test những gì

Gói 1.6.1 từ PR phát hành #323 được test trên VM ngày 26/10/2026, dùng Gõ Sen, Telex. Ubuntu 26.04 (GNOME Wayland, desktop tiếng Anh, gói `resolute`): cập nhật từ 1.6.0-1 bằng install.sh, fcitx5 tự nạp lại; trong một ô chữ GTK 3, câu `dduwowngf tieengs Vieetj Nam cos dduwowjc khoong Haf Nooij` ra đúng chữ có dấu, Cứu chữ đổi `tieengs vieetj` thành `tiếng việt`, Esc trả lại chữ cũ, Tab giữ nguyên `tieengs Nam`; dòng gợi ý của Cứu chữ hiện tiếng Việt. Linux Mint 22 (Cinnamon X11, gói `noble`): cài gói bằng apt, không qua install.sh, rồi chạy fcitx5 lần đầu với `LANG=vi_VN.UTF-8`: Ngó Sen có sẵn trong danh sách bộ gõ. Mint chọn tiếng Việt bằng `LANGUAGE` mà giữ `LANG=en_US.UTF-8` thì danh sách vẫn chỉ có bàn phím. Sau đó cài bằng install.sh rồi khởi động lại: bốn ca trên đúng trong ô chữ GTK 3, câu thử đúng trong Chrome. Fedora, Arch, Debian, openSUSE, Kali và KDE chưa được test với 1.6.1.

Gói 1.6.0 từ PR phát hành #314 được test trên VM ngày 26/10/2026 trên Ubuntu 26.04 (GNOME Wayland, gói `resolute`) và Linux Mint 22 (Cinnamon X11, gói `noble`), dùng Gõ Sen, Telex. Nâng cấp: mỗi máy cài 1.5.8-1, lưu một cài đặt và một gõ tắt, rồi chạy install.sh với gói mới; fcitx5 tự nạp lại, cài đặt, gõ tắt và phím Ctrl+Space còn nguyên, Cứu chữ đã bật. Sau đó, trong một ô chữ GTK 3 trên cả hai máy: gõ câu `dduwowngf tieengs Vieetj Nam cos dduwowjc khoong Haf Nooij` ra đúng chữ có dấu; gõ `tieengs vieetj` ở tiếng Anh rồi bật tiếng Việt, Cứu chữ đổi thành `tiếng việt`, nhấn Esc trả lại chữ cũ; gõ `tieengs Nam` rồi bật tiếng Việt thì chỉ hiện gợi ý, nhấn Tab giữ nguyên chữ đã gõ. Trên Ubuntu, bốn ca đó cũng đúng trong Chrome, và gõ câu trong terminal GNOME Console đúng; trên Mint, gõ câu trong Chrome đúng. Cài mới (gói build ngay trước bản sửa nâng cấp #315, bản sửa không đụng đường cài mới): mở ở tiếng Anh, Ctrl+Shift chuyển sang tiếng Việt, Cứu chữ bật sẵn. Fedora, Arch, Debian, openSUSE, Kali và KDE chưa được test với 1.6.0.

Gói 1.5.8 từ PR phát hành #306 được test trên VM ngày 26/10/2026 trên Ubuntu 26.04 (GNOME Wayland, gói `resolute`) và CachyOS (KDE Plasma Wayland, gói Arch). Trên mỗi máy, cửa sổ cài đặt mở được và vẫn chạy sau 10 giây. Phần lưu của cửa sổ cài đặt được gọi thẳng với fcitx5 đang chạy, mỗi máy một lượt: đổi một ô cài đặt chính và lưu một gõ tắt, rồi đọc lại; cả hai đều lưu được. Với gói 1.5.7-1 đã phát hành trên cùng hai máy, cả hai lần lưu đều bị fcitx5 từ chối và cài đặt không đổi. Chưa bấm lưu bằng chuột trong cửa sổ; Fedora, Debian, Mint, openSUSE và Kali chưa được test với 1.5.8.

Gói 1.5.7 từ PR phát hành #293 được gõ thử trên VM ngày 24/10/2026, dùng Gõ Sen, Telex, trừ lượt Preedit ghi riêng bên dưới.

Linux Mint 22.1 (Cinnamon X11, gói `noble`), Firefox, ô chữ thường, ô contenteditable, TinyMCE và Lexical: câu `tieengs vieetj laf ngoon nguwx cuar nguwowif vieetj nam` gõ 120 ms và 30 ms giữa các phím → `tiếng việt là ngôn ngữ của người việt nam`; `toan`, dấu cách, Backspace, `s` → `toán`. Đúng 11/12 lượt; lượt còn lại trang Lexical báo ô trống chứ không ra chữ sai, gõ lại 3/3 lượt đúng. Terminal xterm.js: `toan`, dấu cách, Backspace, `s` → `toán`, đúng 4/4 lượt. gemini.google.com không đăng nhập, chế độ Preedit: gõ `xin chaof` rồi Enter một lần là gửi, đúng 3/3 lượt.

CachyOS (KDE Plasma Wayland, gói Arch): ô chữ thường trong Firefox và Chromium, câu trên gõ 120 ms và 30 ms, `toan`, dấu cách, Backspace, `s` → `toán`, đúng 12/12 lượt; trước đó lượt đầu ngay sau khi Firefox mới mở sai 2 ca do máy ảo khựng. Máy có cả PyQt5 và PyQt6; cửa sổ cài đặt mở được và vẫn chạy sau 10 giây.

Ubuntu 26.04 (GNOME Wayland, gói `resolute`): cửa sổ cài đặt mở được; Firefox snap, ô chữ thường: câu trên đúng ở cả hai tốc độ. `toan`, dấu cách, Backspace, `s` ra `toans` (3/3 lượt, 1.5.6 trên cùng máy cũng vậy): Firefox bản snap vẫn chưa được sửa lỗi xoá qua dấu cách.

Lưu cài đặt khi fcitx5 khởi động lại lúc cửa sổ đang mở (#279) và dung lượng cài trên Ubuntu sạch (#280) không được test lại trên gói 1.5.7; phụ thuộc của gói .deb đã được đọc lại, chỉ còn `python3-pyqt6`. Fedora, Debian, openSUSE và Kali chưa được gõ thử với 1.5.7.

Gói 1.5.6 từ PR phát hành #268 được gõ thử trên VM ngày 23/10/2026, dùng Gõ Sen, Telex.

Linux Mint 22.1 (Cinnamon X11, gói `noble`), Chromium, Google Sheets: gõ thẳng vào ô rồi Enter, mỗi trường hợp đúng 3/3 lượt: `dduowcj` → `được`; câu `tieengs vieetj laf ngoon nguwx cuar nguwowif vieetj nam` gõ nhanh (30 ms giữa các phím) → `tiếng việt là ngôn ngữ của người việt nam`; `Haf Nooij ddeens 2026 thangs 10` → `Hà Nội đến 2026 tháng 10`; `toan`, Backspace, `s` → `toán`; `duyeet`, 5 Backspace, `yeejt` → `duyệt`. Câu dài trên cũng đúng 3/3 lượt khi gõ nhanh sau khi bấm F2 mở ô.

Ubuntu 26.04 (GNOME Wayland, gói `resolute`), app GLFW chạy qua XWayland (hộp chữ dựng bằng LWJGL 3.4.1, GLFW 3.5.0, giống Minecraft Java): `tieengs`, Backspace, ` vieetj` → `tiến việt`; câu `tieengs vieetj laf ngoon nguwx cuar nguwowif vieetj nam` → `tiếng việt là ngôn ngữ của người việt nam`, đúng 3/3 lượt.

Ubuntu 26.04, Chrome: gõ `tieen` rồi Enter trong ô chat đúng 3/3 lượt; gõ rồi Tab sang ô khác đúng 3/3 lượt. GNOME Text Editor: câu 13 từ `tieengs vieetj xin chaof cacs banj hoom nay trowif ddepj quas ddungs rooif` → `tiếng việt xin chào các bạn hôm nay trời đẹp quá đúng rồi`, đúng 3/3 lượt.

Cửa sổ cài đặt (#212, #249, #250, #252, #253) và lệnh cài trên KDE (#251) không được test lại trên gói 1.5.6. CachyOS, Fedora, Debian, openSUSE và Kali chưa được gõ thử với 1.5.6.

Gói 1.5.5 từ PR phát hành #208 được gõ thử trên VM ngày 20–21/10/2026, dùng Gõ Sen, Telex, trừ lượt VNI Windows ghi riêng bên dưới.

Ubuntu 26.04 (GNOME Wayland, gói `resolute`), Firefox cài từ Flatpak, thanh địa chỉ khi Firefox autofill: `gox` → `gõ`, `daaus` → `dấu`, `ee` → `ê`, `es` → `é`, `dduowcj` → `được`, mỗi trường hợp đúng 3/3 lượt. Với 1.5.4-3 trên cùng VM: `ee` → `eê` 3/3 lượt, `gox` vẫn là `gox` 3/3 lượt, `daaus` → `daáu` 3/3 lượt.

Ubuntu 26.04 và Linux Mint 22.1 (Cinnamon X11, gói `noble`), ô chữ GTK3, macro `VN` → `Việt Nam` và `q` → `“quá”`: gõ `Vn VN q`, Space, backspace, `s` ra `Việt Nam VIỆT NAM “quá”s` (1.5.4-3 ra `Vn VN “quá”s`). Câu 13 từ `tieengs vieetj xin chaof cacs banj hoom nay trowif ddepj quas ddungs rooif` ra `tiếng việt xin chào các bạn hôm nay trời đẹp quá đúng rồi` trên cả hai VM.

Ubuntu 26.04, ô chữ GTK3: bấm vào trong văn bản ngay sau khi gõ vẫn được nhận là bấm chuột, đúng 2/2 lượt và thêm một lượt trên gói cuối.

Linux Mint 22.1, xterm: mở lại từ cuối vẫn được, `vieetj`, Space, backspace, `s` → `viết`; với VNI Windows, `ban`, Space, backspace, `f` → `baøn`.

CachyOS, Fedora, Debian, openSUSE và Kali chưa được gõ thử với 1.5.5.

Gói 1.5.4-1, ngày 20/10/2026: Ubuntu 26.04 (GNOME Wayland, gói `resolute`): Chrome đúng 6/6 lượt, không mất commit; Firefox đúng 4/4 lượt; ô chữ GTK3 đúng 2/2 lượt. Với macro `e` → `em`, `ko` → `không`, gõ `e ko a` ra `em không a` trong ô chữ GTK3 và Chrome. Ô bình luận TikTok thật trong Chrome trên Ubuntu đã được gõ thử khi test #176, đúng 23/23 từ; không được thử lại trên gói cuối.

Gói 1.5.4-1, ngày 20/10/2026: Linux Mint 22.1 (Cinnamon X11, gói `noble`): Chromium đúng 4/4 lượt. CachyOS (KDE Plasma Wayland, gói Arch): Chromium đúng 6/6 lượt trên hai trang Draft.js và một ô textarea thường.

Gói 1.5.3 (PR #151, sau các sửa cuối) được test ngày 19/10/2026 bằng cách gửi phím vào VM, dùng Gõ Sen, Telex. Mỗi lượt gõ hai lần cùng một dòng: `duyeet`, space, 5 backspace, `yeejt` → `duyệt`; `toan`, space, 2 backspace, `nf` → `toàn`; `ban`, space, 1 backspace, `f` → `bàn`; rồi `tieengs vieetj xin chaof cacs banj, hoom nay trowif ddepj quas` → `tiếng việt xin chào các bạn, hôm nay trời đẹp quá`. Linux Mint 22.1 (Cinnamon X11, gói `noble`): GTK3 text box, GTK3 text box ép dùng XIM và xed đều đúng; xterm (XIM) đúng 5/10 lượt, các lượt còn lại lỗi vài từ khi phím dồn tới; bản 1.5.2 trên cùng máy lỗi 2/5 lượt nên lỗi này đã có trước 1.5.3. Ubuntu 26.04 (GNOME Wayland, gói `resolute`): GTK3 text box, gnome-text-editor và xterm đều đúng; Wine 11.20 Notepad đúng cả ba trường hợp backspace, nhưng 2/4 dòng có một hoặc hai từ lỗi (`chaà`, `tiêễn`), so với 4/4 dòng lỗi ở bản 1.5.2 trên cùng máy; các lỗi còn lại đến từ Wine.

Gói build từ nhánh `patch` sau #143 và từ PR #149 (PR #149 cùng code với 1.5.3, chỉ khác số version) được gõ thử trên ba máy ảo, kiểu Gõ Sen, Telex: Ubuntu 26.04 (GNOME Wayland, gói `resolute`), Linux Mint 22.1 (Cinnamon X11, gói `noble`) và CachyOS (KDE Plasma Wayland, gói Arch). Gõ `duyeet`, dấu cách, xoá còn `du` rồi `yeejt` ra `duyệt`; gõ `toan`, xoá một chữ, `nf` ra `toàn`; gõ `ban`, dấu cách, xoá một chữ, `f` ra `bàn`. Đã thử trong gnome-text-editor, xed, ô chữ GTK3 và GTK4, Ptyxis, gnome-terminal, Konsole, kitty, VS Code, Chrome và Firefox; giữ phím xoá rồi gõ tiếp vẫn đúng. Kiểu gõ VNI cũng được gõ thử với lỗi xoá qua dấu cách. LibreOffice và Firefox bản snap chưa được sửa lỗi xoá qua dấu cách, nhưng gõ không bị hỏng thêm.

Gói 1.5.2 (PR #130) được test trên máy ảo Ubuntu 26.04 (GNOME Wayland, gói `resolute`): ở trang Phím › Phím tắt, nút phím của cả năm chế độ hiện đủ khi cửa sổ nhỏ, khi phóng to, và khi thu nhỏ lại sau đó.

Gói 1.5.0 (PR #125) được gõ thử trên ba máy ảo: Linux Mint 22.1 (Cinnamon X11, gói `noble`), Ubuntu 26.04 (GNOME Wayland, gói `resolute`) và MX Linux 25 (Xfce X11, gói `trixie`, lần đầu dùng lõi Rust). Trên cả ba, gõ `tieengs vieetj` trong một ô chữ GTK3 ra `tiếng việt`; trên MX gõ thêm `nguwowif vieetj nam` ra `người việt nam`. Trên Ubuntu 26.04, chọn kiểu gõ Custom thì cửa sổ cài đặt hiện dòng nhắc, và "Kiểm tra máy tôi…" báo không có lỗi cài đặt. Gói 1.5.1 (PR #127) được gõ thử lại trên Mint và Ubuntu 26.04: bật "Tự viết hoa đầu câu", gõ `ban. tooi`, Enter, `xin` ra `Xin` ở dòng mới (gói 1.5.0 ra `xin`); gõ thường vẫn ra `tiếng việt`.

Người giữ dự án dùng Ngó Sen hằng ngày trên Fedora 44 (KDE Plasma Wayland). Kali đã được gõ thử ở các version trước. Fedora 43, Ubuntu 22.04, Debian 12 và openSUSE chỉ được build và qua test lúc build, chưa ai gõ thử trên máy thật.

Mọi gói, kể cả Debian 12, nay dùng lõi gõ Rust; lõi Go đã bỏ.
