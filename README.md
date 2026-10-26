# Ngó Sen

Ngó Sen là bộ gõ tiếng Việt được **vibecode** cho fcitx5 trên Linux, cho trải nghiệm gõ như UniKey trên Windows.
fcitx5 chạy được trên GNOME, KDE, Xfce, Sway và Hyprland, có nhiều tính năng và nhẹ RAM. Chỉ làm cho fcitx5 cũng
đỡ công giữ nhiều bản. Mã IBus cũ ở nhánh `feat/ibus-engine`, không còn được bảo trì.

Cửa sổ cài đặt dùng giao diện sáng, tối của máy trên KDE và GNOME. Đổi gì lưu ngay, có nút Hoàn tác.

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="docs/screenshots/settings-dark.png">
  <img alt="Cửa sổ cài đặt Ngó Sen, trang Gõ chữ, trên KDE Plasma" src="docs/screenshots/settings-light.png" width="750">
</picture>

## Nên dùng chế độ nào

Ngó Sen có hai chế độ: Gõ Sen và Preedit. Nên dùng Gõ Sen làm chế độ chính: mở cài đặt Ngó Sen, trang
Gõ chữ, chọn Gõ Sen ở hàng Chế độ gõ.

Gõ Sen đưa chữ thẳng vào app, không gạch chân. Gợi ý ở thanh địa chỉ hay ô tìm kiếm hiện theo từng phím.
Trình duyệt, terminal, Zalo, LibreOffice đều dùng Gõ Sen; Ngó Sen tự nhận ra cách app nhận chữ, không cần
đổi chế độ. Chế độ này trước đây tên là `Uinput`; cấu hình cũ và chế độ Surrounding Text cũ tự chuyển
thành Gõ Sen.

Telex được dùng và test hằng ngày. VNI và các kiểu khác dùng được nhưng chưa test kỹ.

## Cài

Lệnh dưới đây cài cho Fedora, Ubuntu, Debian, Kali, Arch, CachyOS và openSUSE Tumbleweed trên x86_64.
Fedora, Ubuntu, Debian và Kali còn có gói cho ARM 64-bit (aarch64):

```
curl -fsSL https://raw.githubusercontent.com/ngosen/ngosen/main/install.sh | bash
```

Lệnh tải gói đúng distro từ [release mới nhất](https://github.com/ngosen/ngosen/releases/latest),
so hash SHA-256 và hỏi lại trước khi cài. Chạy lại lệnh để cập nhật version mới; cấu hình cũ giữ nguyên.

Lệnh cài thêm Ngó Sen vào danh sách bộ gõ và thiết lập theo desktop:

- GNOME (desktop mặc định của Ubuntu): đặt fcitx5 thay IBus, bật extension sửa lỗi gõ khi cần. Nếu không
  có khay hệ thống (như Fedora), thêm extension [Input Method Panel](https://extensions.gnome.org/extension/261/)
  để icon và menu bộ gõ hiện trên thanh trên cùng.
- KDE Plasma (Wayland): chọn fcitx5 ở System Settings → Virtual Keyboard để app Qt như Kate, Dolphin
  cũng gõ được.
- Xfce, Cinnamon, MATE (X11, Debian và Ubuntu): dùng im-config đặt fcitx5 làm bộ gõ.

Cài xong, đăng xuất rồi đăng nhập lại; nhấn rồi thả Ctrl+Shift như UniKey để chuyển giữa tiếng Anh
và tiếng Việt, biểu tượng khay hiện chữ V khi đang gõ tiếng Việt. Nếu đã cài Ngó Sen từ trước, bạn vẫn
dùng phím chuyển cũ (thường là Ctrl+Space); muốn đổi phím, mở cửa sổ cài đặt, mục Phím tắt.
Các bước thiết lập chỉ áp dụng cho tài khoản chạy lệnh. Tài khoản khác trên cùng máy cần đăng nhập vào
đó rồi chạy lại lệnh cài.

### Cập nhật bằng trình quản lý gói

Từ version 1.5.3, mỗi release đều có trên repo có chữ ký để nhận version mới qua update của hệ thống:
`dnf upgrade`, `apt upgrade`, `pacman -Syu` hoặc `zypper up`.

Chỉ cần thêm repo một lần. Nếu đã cài Ngó Sen bằng lệnh một dòng, chỉ cần thêm repo; lệnh cài gói trong
mỗi khối dưới đây dành cho máy chưa có Ngó Sen, cài xong làm theo mục “Thiết lập sau khi cài package”.

Các repo dùng chung một key có fingerprint `6D4A 2075 F985 3BE8 F6B6 FE7E 7550 1858 158B 36CD`.
Đối chiếu fingerprint này với thông tin trình quản lý gói hiển thị.

Fedora dùng COPR (x86_64 và aarch64):

```
sudo dnf copr enable nguyenphi/ngosen
sudo dnf install fcitx5-ngosen
```

Debian, Ubuntu và các distro dựa trên chúng (Linux Mint, MX…) dùng repo APT (x86_64 và arm64):

```
sudo curl -fsSL -o /usr/share/keyrings/ngosen.gpg https://ngosen.github.io/ngosen/apt/ngosen.gpg
echo "deb [signed-by=/usr/share/keyrings/ngosen.gpg] https://ngosen.github.io/ngosen/apt $(. /etc/os-release && echo ${UBUNTU_CODENAME:-$VERSION_CODENAME}) main" | sudo tee /etc/apt/sources.list.d/ngosen.list
sudo apt update
sudo apt install fcitx5-ngosen
```

Kali: thay phần `$(. /etc/os-release && echo ${UBUNTU_CODENAME:-$VERSION_CODENAME})` bằng `forky`.

Arch, CachyOS và EndeavourOS dùng repo pacman (x86_64):

```
curl -fsSL https://ngosen.github.io/ngosen/arch/ngosen.asc | sudo pacman-key --add -
sudo pacman-key --lsign-key 6D4A2075F9853BE8F6B6FE7E75501858158B36CD
printf '\n[ngosen]\nServer = https://ngosen.github.io/ngosen/arch/$arch\n' | sudo tee -a /etc/pacman.conf
sudo pacman -Syu fcitx5-ngosen
```

openSUSE Tumbleweed dùng repo zypper (x86_64):

```
sudo zypper addrepo https://ngosen.github.io/ngosen/opensuse/ngosen.repo
sudo zypper refresh
sudo zypper install fcitx5-ngosen
```

zypper hỏi một lần có tin cậy key không; đối chiếu fingerprint rồi chọn tin cậy.

Sau khi update, khởi động lại fcitx5 theo mục “Khởi động lại sau khi cài hoặc cập nhật”.

### Thiết lập sau khi cài package

Nếu máy chưa từng thiết lập Ngó Sen, cài từ repo, package tải về hoặc dùng `--no-setup` chưa thêm Ngó Sen
vào danh sách bộ gõ. Ctrl+Space sẽ chưa gõ được tiếng Việt.

- Mở “Cấu hình Fcitx 5” bằng `fcitx5-configtool`, vào tab bộ gõ và thêm “Ngó Sen” vào danh sách. Máy để
  ngôn ngữ hiển thị tiếng Việt thì fcitx5 tự thêm Ngó Sen ở lần chạy đầu, không cần bước này.
- Với Xfce, Cinnamon, MATE trên Debian/Ubuntu/Mint, chạy `im-config -n fcitx5`, rồi đăng xuất và đăng nhập
  lại. Nếu có IBus, bước này giúp fcitx5 khởi động khi đăng nhập và gõ được trong app dùng XIM như PlanMaker.

Có thể chạy lệnh cài một dòng ở trên để làm cả hai bước.
Nếu đã dùng lệnh cài một dòng và chỉ thêm repo để update, không cần làm thêm.

### Cài bằng tay hoặc tự build

Muốn tự thiết lập desktop thì cài bằng
`curl -fsSL https://raw.githubusercontent.com/ngosen/ngosen/main/install.sh | bash -s -- --no-setup`.

Không muốn chạy lệnh tải từ mạng thì tải gói ở [Releases](https://github.com/ngosen/ngosen/releases) và cài
bằng tay. Muốn tự build từ source thì xem [TU-DUNG.md](TU-DUNG.md).

### Khởi động lại sau khi cài hoặc cập nhật

Mỗi lần cập nhật, hoặc khi cài bằng `--no-setup` hay cài gói bằng tay, khởi động lại fcitx5:

```
fcitx5 -rd
```

KDE Wayland: đăng xuất rồi đăng nhập lại thay cho lệnh trên. fcitx5 bật bằng `fcitx5 -rd` không gõ được
trong app Qt như Kate, Dolphin.

### Test tính năng chưa có trong release

Cài [version thử](https://github.com/ngosen/ngosen/releases/tag/next-build), build tự động khi nhánh `next`
thay đổi:

```
curl -fsSL https://raw.githubusercontent.com/ngosen/ngosen/next/install.sh | NGOSEN_RELEASE_URL=https://github.com/ngosen/ngosen/releases/download/next-build bash
```

### NixOS

Thêm flake của repo vào cấu hình rồi đưa gói vào danh sách addon của fcitx5. Nix build từ source trong repo:

```nix
# flake.nix
inputs.ngosen.url = "github:ngosen/ngosen";

# configuration.nix, với inputs truyền qua specialArgs
i18n.inputMethod = {
  enable = true;
  type = "fcitx5";
  fcitx5.addons = [ inputs.ngosen.packages.${pkgs.stdenv.hostPlatform.system}.default ];
};
```

CI build gói, chạy test và nạp vào fcitx5 trên màn hình X ảo; chưa ai test trên máy NixOS thật.
Gặp lỗi thì mở issue. Muốn build từ bản clone trên máy, xem mục NixOS trong [TU-DUNG.md](TU-DUNG.md#nixos).

## Lỗi thường gặp

### Ubuntu 26.04: extension sửa lỗi gõ

Có từ version `0.5.0-1`. Lệnh cài tự bật extension trên GNOME 50.0 đến 50.3. Nếu cài gói bằng tay,
đăng xuất rồi đăng nhập lại, sau đó chạy một lần:

```
gnome-extensions enable forward-keys@ngosen.github.io
```

Khi Ubuntu sửa lỗi này ([báo lỗi trên Launchpad](https://bugs.launchpad.net/ubuntu/+source/mutter/+bug/2169784)),
tắt extension bằng `gnome-extensions disable forward-keys@ngosen.github.io`.

### GNOME Wayland: bấm chuột mất chữ đang gõ

Ở chế độ Preedit, Chrome, Chromium và các app dựa trên Chromium (Edge, Brave, app Electron) chạy thẳng trên
Wayland mất chữ đang gõ dở khi bấm chuột, dù cùng ô hay ô khác. App GTK và mọi app trên X11 không bị.

Lỗi ở fcitx5 5.1.19 trở về trước: khi app reset, fcitx5 bảo GNOME Shell bỏ chữ đang gõ dở. Version 5.1.20 đã
sửa. Ubuntu 24.04, Ubuntu 26.04, Debian 13 và openSUSE Tumbleweed còn fcitx5 cũ; Arch, Fedora 44 và Debian sid
đã có bản mới. Đã nhờ Ubuntu đưa bản sửa vào ([báo lỗi trên Launchpad](https://bugs.launchpad.net/ubuntu/+source/fcitx5/+bug/2172006)).

Trong lúc chờ, nên dùng chế độ Gõ Sen, hoặc gõ xong chữ bằng phím cách rồi mới bấm chuột.

### Ubuntu: VS Code

Nên dùng gói `.deb` từ [trang của Microsoft](https://code.visualstudio.com/download) hoặc Flatpak từ
[Flathub](https://flathub.org/apps/com.visualstudio.code). Cả hai gõ được tiếng Việt ngay, không cần chỉnh.

Bản snap từ App Center thiếu phần nối với fcitx5 nên gõ ra Telex thô (`tieengs`). Nếu buộc phải dùng snap,
cho nó đi qua XIM (đường nối kiểu cũ của X11): chép file `.desktop` vào thư mục nhà rồi sửa dòng mở app:

```
cp /var/lib/snapd/desktop/applications/code_code.desktop ~/.local/share/applications/
sed -i 's|^Exec=/snap/bin/code|Exec=env GTK_IM_MODULE=xim XMODIFIERS=@im=fcitx /snap/bin/code|' \
  ~/.local/share/applications/code_code.desktop
```

Tắt hẳn VS Code rồi mở lại từ menu. Qua XIM, khoảng một câu trong mười có một chữ sai dấu.
Muốn bỏ cách này thì xoá file `~/.local/share/applications/code_code.desktop`.

Gặp lỗi khác thì báo ở [Issues](https://github.com/ngosen/ngosen/issues). Tin version mới và bài blog có trên
[Facebook](https://www.facebook.com/profile.php?id=61595753582618); hỏi đáp và báo lỗi vẫn ở GitHub.

## Gỡ

Lệnh hỏi lại rồi mới gỡ:

```
curl -fsSL https://raw.githubusercontent.com/ngosen/ngosen/main/install.sh | bash -s -- --uninstall
```

- GNOME: trả lại thiết lập cho tài khoản chạy lệnh. fcitx5 thôi tự chạy khi đăng nhập, app dùng lại IBus,
  tắt extension sửa lỗi gõ và gỡ Input Method Panel nếu lệnh cài đã thêm. Đăng xuất rồi đăng nhập lại để
  GNOME dùng IBus như cũ. File đã sửa tay sau khi cài được giữ nguyên. fcitx5 vẫn còn; muốn gỡ luôn thì
  chạy `sudo apt autoremove` (Ubuntu, Debian) sau đó.
- Xfce, Cinnamon, MATE: trả bộ gõ về mặc định của máy. fcitx5 còn cài thì có thể vẫn tự chạy;
  gỡ hẳn bằng `sudo apt remove fcitx5`.
- KDE: giữ fcitx5 làm bàn phím ảo vì có thể đang dùng cho bộ gõ khác. Muốn bỏ thì chọn None ở
  System Settings → Virtual Keyboard.

Gỡ bằng tay:

1. Bỏ Ngó Sen khỏi danh sách bộ gõ trong "Fcitx5 Configuration".
2. Gỡ gói rồi khởi động lại fcitx5:

   ```
   sudo dnf remove fcitx5-ngosen       # Fedora
   sudo apt remove fcitx5-ngosen       # Ubuntu, Debian
   sudo pacman -R fcitx5-ngosen        # Arch, CachyOS
   sudo zypper remove fcitx5-ngosen    # openSUSE
   fcitx5 -rd
   ```

Cấu hình vẫn ở lại để lần cài sau dùng tiếp: các file `ngosen*.conf` trong `~/.config/fcitx5/conf/`,
và `lotus*.conf` nếu từng dùng version trước 1.1.0. Muốn xoá sạch thì xoá các file đó.

## Nguồn gốc

Dựa trên mã của [fcitx5-lotus](https://github.com/LotusInputMethod/fcitx5-lotus) và
[VMK](https://github.com/thanhpy2009/VMK). Giấy phép GPL-3.0-or-later.

Lõi biến `tieengs` thành `tiếng` là bản Rust [bamboo-core](https://github.com/bamboo-vi/bamboo-core) của
[@nguyen10t2](https://github.com/nguyen10t2), port từ bản Go
[bamboo-core](https://github.com/BambooEngine/bamboo-core) của
[@t1ld3x](https://github.com/t1ld3x). Cả hai dự án dùng giấy phép MIT.
Mọi gói Ngó Sen, kể cả Debian 12, đều dùng bản Rust.

Issue và pull request trước ngày 10/10/2026 nằm ở repo lưu trữ
[ngosen/ngosen-fork-archive](https://github.com/ngosen/ngosen-fork-archive).
Những gì sắp làm nằm ở [ROADMAP.md](ROADMAP.md).

### Lấy mã hay ý tưởng từ Ngó Sen

Chép hay sửa mã của Ngó Sen thì giữ giấy phép GPL-3.0-or-later và ghi nguồn, như GPL yêu cầu. Nhờ AI đọc mã
rồi viết lại cũng vậy, vì đó vẫn là mã sửa từ mã của Ngó Sen.

Đưa cách làm của Ngó Sen sang bộ gõ khác (cách xoá chữ trong từng loại app, cách nhận ra ô địa chỉ trình duyệt…),
kể cả khi chỉ lấy ý tưởng, thì ghi nguồn kèm link tới repo này, trong README hay ghi chú phát hành.

## English

Ngó Sen is a Vietnamese input method for fcitx5. The maintainer only **vibecodes** this project: an AI coding
agent writes the code, then the maintainer measures and uses it daily. It is licensed under GPL-3.0-or-later
and published as is, with no promise of support.

Since 1.0, Ngó Sen has two typing modes, Gõ Sen (for fast typing, browsers and Electron apps) and Preedit,
and uses the Rust [bamboo-core](https://github.com/bamboo-vi/bamboo-core) by
[@nguyen10t2](https://github.com/nguyen10t2), ported from the original Go
[bamboo-core](https://github.com/BambooEngine/bamboo-core) by [@t1ld3x](https://github.com/t1ld3x).
Both projects are MIT licensed. All Ngó Sen packages, including Debian 12, use the Rust core.

Ngó Sen targets fcitx5 only. The IBus engine was dropped and remains unmaintained on `feat/ibus-engine`.
If you reuse Ngó Sen's code, keep the GPL and its notices, including when an AI tool rewrites it.
If you port its approaches to another input method, credit this repository with a link.
The rest of this page is in Vietnamese.
