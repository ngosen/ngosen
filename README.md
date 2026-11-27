<div align="center">

<img src="data/icons/hicolor/scalable/apps/io.github.ngosen.NgoSen.Settings.svg" alt="Logo Ngó Sen" width="128">

# Ngó Sen

**Bộ gõ tiếng Việt cho Linux, gõ tự nhiên và mượt mà như UniKey trên Windows.**

[![Release](https://img.shields.io/github/v/release/ngosen/ngosen?label=version&color=1f6e5c)](https://github.com/ngosen/ngosen/releases/latest)
[![License](https://img.shields.io/badge/license-GPL--3.0--or--later-1f6e5c)](LICENSE)
[![fcitx5](https://img.shields.io/badge/fcitx5-GNOME%20%C2%B7%20KDE%20%C2%B7%20Xfce%20%C2%B7%20Sway%20%C2%B7%20Hyprland%20%C2%B7%20niri-c9a96e)](#cai-dat)

[Cài đặt](#cai-dat) · [Tính năng](#tinh-nang) · [Khắc phục sự cố](#khac-phuc-su-co) · [CHANGELOG](CHANGELOG.md) · [Báo lỗi](https://github.com/ngosen/ngosen/issues)

</div>

---

Ngó Sen có hai bản, dùng chung lõi gõ nên gần như mọi tính năng có ở cả hai; hai điểm nhỏ chỉ bản fcitx5 có
được ghi ở mục cài bản IBus. Chọn bản nào là tuỳ thích:

- **fcitx5**: chạy trên GNOME, KDE Plasma, Xfce, Sway, Hyprland và niri, nhẹ RAM. Tính năng mới có ở bản này
  trước.
- **IBus** ([ibus-ngosen](https://github.com/ngosen/ibus-ngosen)): chạy trên IBus, bộ gõ có sẵn của GNOME, và
  trên Cinnamon, Xfce, KDE Plasma khi dùng IBus. Tính năng mới đến bản IBus chậm hơn một chút.

Cửa sổ cài đặt có hai giao diện: **GTK** (tự mở trên GNOME, Linux Mint Cinnamon và Xfce) và **Qt** (mở
trên KDE Plasma và các desktop khác). Cửa sổ theo giao diện sáng/tối của máy, lưu thay đổi ngay và có nút
Hoàn tác.

<table>
  <tr>
    <th>Bản GTK trên GNOME</th>
    <th>Bản Qt trên KDE Plasma</th>
  </tr>
  <tr>
    <td>
      <picture>
        <source media="(prefers-color-scheme: dark)" srcset="docs/screenshots/settings-gtk-dark.png">
        <img alt="Cửa sổ cài đặt Ngó Sen bản GTK, trang Gõ chữ, trên GNOME" src="docs/screenshots/settings-gtk-light.png">
      </picture>
    </td>
    <td>
      <picture>
        <source media="(prefers-color-scheme: dark)" srcset="docs/screenshots/settings-dark.png">
        <img alt="Cửa sổ cài đặt Ngó Sen bản Qt, trang Gõ chữ, trên KDE Plasma" src="docs/screenshots/settings-light.png">
      </picture>
    </td>
  </tr>
</table>

## Mục lục

- [✨ Tính năng nổi bật](#tinh-nang)
- [💡 Lựa chọn chế độ gõ](#che-do-go)
- [📦 Cài đặt](#cai-dat)
  - [Cài đặt tự động](#cai-tu-dong)
  - [Chọn bản fcitx5 hoặc IBus](#chon-ban)
  - [Cài qua trình quản lý gói](#trinh-quan-ly-goi)
  - [Cài trên NixOS](#nixos)
  - [Cài bằng tay, tự build, bản thử](#cai-bang-tay)
- [🛠️ Thiết lập sau khi cài đặt](#thiet-lap)
- [🔍 Khắc phục sự cố thường gặp](#khac-phuc-su-co)
- [🗑️ Gỡ cài đặt](#go-cai-dat)
- [🏛️ Nguồn gốc & ghi công](#nguon-goc)
- [English](#english)

<a id="tinh-nang"></a>

## ✨ Tính năng nổi bật

- **Cứu chữ:** quên bật tiếng Việt và gõ nhầm `tieengs vieetj`? Chỉ cần bấm phím chuyển sang tiếng Việt,
  chữ sẽ đổi thành `tiếng việt`. Nhấn `Tab` hoặc `Esc` để lấy lại chữ đã gõ. Cứu chữ không ghi nhớ gì
  trong ô mật khẩu.
- **Cứu dấu:** bôi đen chữ rồi bấm phím dấu thanh để thêm, đổi hoặc bỏ dấu (`nhanh` + `s` → `nhánh`). Ở
  chế độ Gõ Sen, chỉ cần bấm chuột ngay sau từ (`nhanh|` + `s`) là sửa được dấu mà không cần bôi đen.
- **Chuyển mã:** bôi đen đoạn chữ gõ bằng bảng mã cũ (TCVN3, VNI…) rồi bấm phím chuyển mã, chữ được đổi
  sang bảng mã đã chọn (mặc định Unicode); có thể đổi chữ hoa, chữ thường hoặc bỏ dấu cùng lúc. Đặt phím và bảng mã ở trang
  **Chuyển mã** trong cửa sổ cài đặt; trang này còn có ô để dán chữ vào và chuyển ngay. Đã thử trên GNOME
  với Firefox, Chromium, LibreOffice Writer và GNOME Text Editor; không dùng được trong terminal.
- **Menu chế độ riêng cho từng app:** nhấn phím `` ` `` để mở menu cạnh con trỏ, chọn **Gõ Sen (1)**,
  **Gạch chân (q)** hoặc **Tắt (e)**, bật **Gợi ý emoji (w)** cho app muốn dùng, và bật **Vim mode (v)** cho
  app dùng phím Vim như nvim hay Claude Code. Ngó Sen nhớ lựa chọn riêng cho từng app.
- **Gõ thẳng không gạch chân (Gõ Sen):** chữ vào app ngay khi gõ; gợi ý ở thanh địa chỉ hay ô tìm kiếm
  hiện theo từng phím.
- **Nhận ra tiếng Anh:** chữ không thể thành tiếng Việt thì hiện lại đúng phím đã gõ. Đo bằng Telex, cài
  đặt mặc định, trên 9.884 từ tiếng Anh thông dụng: 92,9% đúng ngay khi gõ, 96,9% đúng sau khi bấm Space.
  Muốn giữ từ tiếng Anh trùng cách gõ tiếng Việt thì gõ đúp phím dấu (`thiss` → `this`).
- **Lệnh và đường dẫn:** từ bắt đầu bằng `/` giữ nguyên như gõ (`/cost`, `cd /usr/share`).
- **Gõ thừa phím vẫn giữ dấu:** `dduowjcd` + Space ra `được`, `coongty` + Space ra `công ty`, `nhuwg` + Space
  ra `nhưng`; bấm Backspace ngay sau đó thì trả lại đúng chữ đã gõ.
- **Gợi ý emoji khi gõ:** tắt sẵn; bật cho từng app bằng **Gợi ý emoji (w)** trong menu `` ` ``. Khi bật, gõ xong
  `cười`, `con mèo` hay `haha` là hiện `Tab: 😂 …` cạnh con trỏ; Tab để đánh số rồi chọn bằng số, Esc giữ
  nguyên chữ. Để yên 4 giây thì danh sách tự ẩn.
- **Phím chuyển Anh/Việt riêng:** cài mới dùng `Ctrl + Shift` như UniKey và bật sẵn tiếng Việt;
  người dùng cũ giữ phím chuyển và trạng thái đang dùng. Mặc định, phím chuyển đổi mọi app cùng lúc.
  Muốn mỗi app nhớ riêng như trước, chọn “Từng app” ở “Phím chuyển áp dụng cho” trên trang Phím tắt.
  App đặt Tắt trong menu `` ` `` vẫn gõ tiếng Anh.
- **Phím chốt chữ (Ctrl):** bấm rồi thả `Ctrl` để giữ nguyên từ đang gõ, không tự khôi phục hay bung gõ tắt.
  Chữ đang hiện đúng phím đã gõ như `tunx` thì bấm `Ctrl` để thêm dấu thành `tũn`.
  Ở chế độ Gõ Sen, bấm tiếp thì từ đổi qua lại giữa chữ có dấu và đúng phím đã gõ: `tieengs` + `Ctrl` `Ctrl`
  ra `tieengs`. Từ tiếng Anh lỡ bị thêm dấu cũng lấy lại được như vậy.
- **Đoán dấu:** gõ không dấu `toi di hoc` thì ra `tôi đi học`. Tắt sẵn; bật ở trang **Đoán chữ** trong
  cửa sổ cài đặt, nơi có thêm lựa chọn sửa cả âm tiết gõ sai (`mọi mguoifw` ra `mọi người`) và mức mạnh
  tay khi sửa. Từ gõ trượt mà chưa đủ chắc thì được xét lại khi bạn gõ xong từ kế tiếp; Backspace ngay
  sau đó trả lại đúng chữ đã gõ. Trong terminal và ứng dụng X11 cũ qua XIM, Đoán dấu nghỉ trừ khi bật **Đoán cả trong
  terminal**: ở đó nó không sửa lại được chữ trước nên đoán sai nhiều hơn. Đoán dấu học từ chữ bạn gõ để
  đoán sát cách bạn viết; xem [Đoán chữ học gì](#doan-chu-hoc-gi).
- **Gõ tắt (macro):** tự theo chữ hoa: `ko` → `không` thì gõ `Ko` sẽ ra `Không`.
- **Chữ viết tắt khi chat:** gõ `ko`, `dc`, `cviec` rồi dấu cách hay dấu câu là ra `không`, `được`,
  `công việc`, không cần tự khai gõ tắt. Bật sẵn, và lần đầu bung Ngó Sen báo một lần bằng thông báo của
  desktop. Bung nhầm thì bấm Backspace ngay sau dấu cách để lấy lại chữ đã gõ; chữ nào bạn lấy lại hai lần
  liền thì từ đó Ngó Sen để nguyên. Trang **Gõ tắt** trong cửa sổ cài đặt có danh sách các chữ viết tắt,
  nút **Bung lại** cho chữ đang để nguyên, và công tắc **Bung chữ viết tắt khi chat** để tắt hẳn. Không bung
  trong terminal, ô mật khẩu và ô địa chỉ.
- **Trang "Kiểm tra máy":** tìm lỗi thiết lập của desktop, cho biết còn mấy việc cần sửa và sửa từng việc
  bằng một nút bấm.

Chi tiết từng version ở [CHANGELOG.md](CHANGELOG.md), những gì sắp làm ở [ROADMAP.md](ROADMAP.md).

<a id="doan-chu-hoc-gi"></a>

### Đoán chữ học gì

Khi Đoán dấu bật, nó học từ chữ bạn gõ. Việc học bật sẵn; lần đầu học, Ngó Sen báo một lần bằng thông
báo của desktop.

- **Học gì:** chữ có dấu mà Ngó Sen đã đưa vào ô, gồm chữ bạn tự gõ có dấu và chữ bạn bấm Backspace để giữ
  nguyên như đã gõ sau khi đoán sai. Không học chữ máy tự đoán, không học chữ bung ra từ gõ tắt.
- **Lưu gì:** các cụm 1, 2 và 3 chữ liền nhau kèm số lần gặp, cả chỗ bắt đầu và kết thúc một đoạn gõ.
  Ghép các cụm lại có thể đọc ra những câu ngắn bạn đã gõ.
- **Khi nào:** khi một đoạn gõ kết thúc: rời ô, bấm chuột, nhấn Enter, hoặc gõ đủ 200 âm tiết. Chữ gõ rồi
  xoá bằng Backspace vẫn có thể đã được học.
- **Không học:** trong ô mật khẩu hay ô báo là riêng tư. Trong terminal chỉ học khi bật **Đoán cả trong
  terminal**, và khi đó vẫn không học lúc Ngó Sen nhận ra terminal đang giấu chữ (đoán theo dấu hiệu nên
  có thể sót). Không học ở chế độ Off, hay khi tắt Đoán dấu hoặc
  tắt học.
- **Lưu ở đâu:** `~/.local/share/ngosen/learned-diacritics`, dạng văn bản thường, chỉ bạn đọc được. Ghi 5
  phút một lần khi có gì mới, và khi fcitx5 thoát. Không gửi đi đâu.
- **Xem, quên, tắt:** trang **Đoán chữ** có nút **Xem**, nút **Quên hết** và công tắc **Học từ chữ bạn
  gõ**. "Khôi phục mặc định" không xoá những gì đã học và giữ nguyên công tắc học.
- **Chữ viết tắt khi chat:** chữ nào bạn lấy lại bằng Backspace hai lần liền thì được để nguyên như gõ.
  Ngó Sen chỉ ghi tên các chữ ấy kèm số lần, ở `~/.local/share/ngosen/kept-chat-shorthand`, không ghi chữ
  nào khác bạn gõ. Việc đếm theo công tắc **Học từ chữ bạn gõ**, dù Đoán dấu bật hay tắt; **Quên hết** xoá
  cả phần này, và trang **Gõ tắt** cho bung lại từng chữ.

<a id="che-do-go"></a>

## 💡 Lựa chọn chế độ gõ

1. **Gõ Sen (khuyên dùng):** đưa chữ thẳng vào app, không gạch chân. Ngó Sen tự nhận ra cách trình duyệt,
   terminal, Zalo, LibreOffice nhận chữ để xử lý cho đúng, không cần đổi chế độ.
2. **Preedit (Gạch chân):** giữ từ đang gõ trong vùng gạch chân rồi mới đưa vào app.

> [!TIP]
> Mở cài đặt Ngó Sen → trang **Gõ chữ** → chọn **Gõ Sen** ở hàng Chế độ gõ. Kiểu gõ **Telex** được dùng và
> test hằng ngày; VNI và các kiểu khác dùng được nhưng chưa test kỹ.

<a id="cai-dat"></a>
<a id="cài"></a>

## 📦 Cài đặt

<a id="cai-tu-dong"></a>

### 1. Cài đặt tự động (khuyên dùng)

Hỗ trợ Fedora, Ubuntu (từ 24.04), Debian (từ 12), Kali, deepin, LMDE, Devuan, Parrot, Arch, Artix, CachyOS
và openSUSE Tumbleweed trên `x86_64`. Fedora, Ubuntu, Debian và Kali có thêm gói `aarch64`.

```bash
curl -fsSL https://raw.githubusercontent.com/ngosen/ngosen/main/install.sh | bash
```

Lệnh tải gói đúng distro từ [release mới nhất](https://github.com/ngosen/ngosen/releases/latest), kiểm tra
SHA-256, hỏi lại trước khi cài và tự thiết lập theo desktop đang dùng. Chạy lại lệnh này để cập nhật version
mới; cấu hình cũ giữ nguyên. Thiết lập chỉ áp dụng cho tài khoản chạy lệnh.

Cài mới bật sẵn tiếng Việt. Nhấn rồi thả `Ctrl + Shift` để chuyển giữa tiếng Anh và tiếng Việt; biểu
tượng khay hiện hai mầm ngó sen hình chữ V khi đang gõ tiếng Việt. Bạn cũng có thể bấm “✔ Tiếng Việt” / “✖ Tiếng Việt”,
mục luôn hiện ở đầu menu khay fcitx5, để chuyển ngôn ngữ như phím chuyển và giữ lựa chọn sau khi
khởi động lại. Mặc định, cả hai cách chuyển mọi app cùng lúc; chọn “Từng app” ở “Phím chuyển áp dụng
cho” trên trang Phím tắt để mỗi app nhớ riêng. App đặt Tắt trong menu `` ` `` vẫn gõ tiếng Anh; bấm
phím chuyển trong app đó mở menu để chọn Gõ Sen hoặc Gạch chân và gõ tiếng Việt trở lại.

<details>
<summary>Lệnh cài làm gì trên từng desktop</summary>

- **GNOME:** đặt fcitx5 thay IBus, bật extension sửa lỗi gõ khi cần, thêm extension
  [Input Method Panel](https://extensions.gnome.org/extension/261/) để bảng emoji nằm dưới dòng chữ
  (không có nó, app Wayland như Firefox thấy bảng che lên chữ đang gõ) và icon, menu bộ gõ hiện trên
  thanh trên cùng.
- **KDE Plasma (Wayland):** chọn fcitx5 ở System Settings → Virtual Keyboard để app Qt như Kate, Dolphin
  cũng gõ được. Máy có WPS Office thì đặt `QT_IM_MODULE=fcitx` riêng cho WPS.
- **Xfce, Cinnamon, MATE** (X11, Debian và Ubuntu): dùng im-config đặt fcitx5 làm bộ gõ.
- **Sway, Hyprland, niri:** chạy lệnh từ terminal trong phiên desktop để thêm Ngó Sen và bật fcitx5, không
  cần đăng xuất. Lệnh không sửa file config của desktop; để fcitx5 tự chạy ở lần đăng nhập sau, dán dòng lệnh được in ra.

Desktop khác: cài xong thì đăng xuất rồi đăng nhập lại.

Lần cài đầu, lệnh mở trang **Kiểm tra máy** trong cửa sổ cài đặt để xem còn bước nào chưa xong: mở ngay nếu
fcitx5 đang chạy, không thì mở một lần ở lần đăng nhập sau.

</details>

<a id="chon-ban"></a>

### 2. Chọn bản fcitx5 hoặc IBus

Khi cài lần đầu trên GNOME, hoặc Xfce, Cinnamon, MATE dùng X11 ở Debian, Ubuntu, Linux Mint, lệnh cài hỏi
chọn bản nào. Nhấn Enter là chọn bản fcitx5. Muốn chọn thẳng:

```bash
# Bản fcitx5
curl -fsSL https://raw.githubusercontent.com/ngosen/ngosen/main/install.sh | bash -s -- --fcitx5

# Bản IBus
curl -fsSL https://raw.githubusercontent.com/ngosen/ngosen/main/install.sh | bash -s -- --ibus
```

<details>
<summary>Bản IBus khác gì, và khi chuyển bản thì sao</summary>

Bản IBus có đủ tính năng của bản fcitx5: Gõ Sen, gợi ý emoji, Teencode, Vim mode, Cứu chữ, Chuyển mã, gõ tắt,
từ điển riêng, bảng phím tùy chỉnh và chế độ gõ riêng cho từng app (phím `` ` ``). Tính năng mới đến bản IBus
chậm hơn một chút. Hai điểm nhỏ chỉ bản fcitx5 có: Cứu dấu trên chữ đang bôi đen ở mọi app (bản IBus chỉ ở app
X11) và Chuyển mã trong LibreOffice.

Chạy lại lệnh cài để cập nhật thì giữ bản đang có, không hỏi lại. Muốn chuyển sang bản kia, chạy lệnh với
`--ibus` hoặc `--fcitx5`. Dùng `--yes` thì lệnh không hỏi và cài bản fcitx5.

Hai package không cài cùng nhau được; chuyển bản sẽ gỡ package của bản kia cho mọi tài khoản trên máy. Trên
Debian, Ubuntu và Fedora, package cũ được gỡ trong cùng lần cài, nên cài lỗi thì bản cũ vẫn còn. Trên openSUSE
và Arch, lệnh dừng và in lệnh gỡ package kia; gỡ xong thì chạy lại lệnh cài.

Từ fcitx5 sang IBus: bản IBus chép kiểu gõ, gõ tắt, bảng phím và từ điển tùy chỉnh ở lần chạy đầu, lệnh cài
thêm “Tiếng Việt (Ngó Sen)” vào nguồn nhập của GNOME. Từ IBus sang fcitx5: cài đặt không tự chép sang, hãy sao
lưu trong cửa sổ cài đặt của bản IBus rồi khôi phục trong bản fcitx5. Sau khi chuyển, đăng xuất rồi đăng nhập
lại.

Xem thêm ở [README của bản IBus](https://github.com/ngosen/ibus-ngosen#readme).

</details>

<a id="trinh-quan-ly-goi"></a>
<a id="cập-nhật-bằng-trình-quản-lý-gói"></a>

### 3. Cài qua trình quản lý gói

Mỗi release đều có trên repo có chữ ký, nên version mới đến qua update của hệ thống (`dnf upgrade`,
`apt upgrade`, `pacman -Syu`, `zypper up`). Repo APT, pacman và zypper dùng chung key có fingerprint
`6D4A 2075 F985 3BE8 F6B6 FE7E 7550 1858 158B 36CD`. COPR trên Fedora dùng key của COPR.

Trên Debian, Ubuntu và các distro dựa trên chúng, lệnh cài một dòng nay tự thêm repo APT sau khi cài.
Nếu đã cài bằng lệnh này trước đây, chạy lại để thêm repo. Fedora, Arch và openSUSE vẫn thêm repo theo
hướng dẫn dưới đây.

Đã cài bằng lệnh một dòng thì chỉ cần thêm repo. Máy chưa có Ngó Sen thì cài gói xong làm theo mục
[Thiết lập sau khi cài đặt](#thiet-lap).

**Fedora (COPR, x86_64 và aarch64):**

```bash
sudo dnf copr enable nguyenphi/ngosen
sudo dnf install fcitx5-ngosen
```

**Debian, Ubuntu, Linux Mint, MX (APT, x86_64 và arm64):**

```bash
sudo curl -fsSL -o /usr/share/keyrings/ngosen.gpg https://ngosen.github.io/ngosen/apt/ngosen.gpg
echo "deb [signed-by=/usr/share/keyrings/ngosen.gpg] https://ngosen.github.io/ngosen/apt $(. /etc/os-release && echo ${UBUNTU_CODENAME:-$VERSION_CODENAME}) main" | sudo tee /etc/apt/sources.list.d/ngosen.list
sudo apt update
sudo apt install fcitx5-ngosen
```

> [!NOTE]
> Kali: thay phần `$(. /etc/os-release && echo ${UBUNTU_CODENAME:-$VERSION_CODENAME})` bằng `forky`. deepin 23
> và 25: thay bằng `bookworm`. LMDE, Devuan, Parrot: thay bằng bản Debian gốc, `bookworm` hoặc `trixie` (xem
> `/etc/debian_version`: 12 là `bookworm`, 13 là `trixie`).

**Arch, CachyOS, EndeavourOS (pacman, x86_64):**

```bash
curl -fsSL https://ngosen.github.io/ngosen/arch/ngosen.asc | sudo pacman-key --add -
sudo pacman-key --lsign-key 6D4A2075F9853BE8F6B6FE7E75501858158B36CD
printf '\n[ngosen]\nServer = https://ngosen.github.io/ngosen/arch/$arch\n' | sudo tee -a /etc/pacman.conf
sudo pacman -Syu fcitx5-ngosen
```

Hoặc cài từ AUR, tự build trên máy: `yay -S fcitx5-ngosen` (bản IBus: `yay -S ibus-ngosen`).

**openSUSE Tumbleweed (zypper, x86_64):**

```bash
sudo zypper addrepo https://ngosen.github.io/ngosen/opensuse/ngosen.repo
sudo zypper refresh
sudo zypper install fcitx5-ngosen
```

zypper hỏi một lần có tin cậy key không; đối chiếu fingerprint rồi chọn tin cậy.

Bản IBus và bản Wayland (cho niri, Sway, Hyprland) cũng có trong repo APT, pacman và zypper: thay `fcitx5-ngosen` bằng
`ibus-ngosen` hoặc [`wl-ngosen`](https://github.com/ngosen/wl-ngosen) trong lệnh cài.

<a id="goi-cua-distro"></a>

**Gói của distro:** Debian có gói `fcitx5-ngosen` riêng, do Debian build và đưa version mới tới qua
`apt upgrade`; các distro lấy gói từ Debian cũng vậy. apt luôn coi gói của Ngó Sen là mới hơn gói của
distro, kể cả khi số version nhỏ hơn, nên mỗi máy chỉ nên theo một bên:

- Muốn dùng gói của distro: `sudo apt install fcitx5-ngosen`, không thêm repo Ngó Sen và không chạy lệnh
  cài một dòng. Lệnh cài một dòng thay gói của distro bằng gói Ngó Sen (lệnh báo trước rồi mới hỏi).
- Đang dùng gói Ngó Sen, muốn quay về gói của distro: báo apt ưu tiên gói `fcitx5-ngosen` của distro
  (version có `+ds`) rồi cài lại. Từ đó apt theo version của distro. Repo vẫn giữ, nên `ibus-ngosen`
  và `wl-ngosen` vẫn nhận version mới. apt gọi việc này là hạ version vì cách đánh số, dù version của
  distro có thể mới hơn.

```bash
printf '%s\n' 'Package: fcitx5-ngosen' 'Pin: version /\+ds/' 'Pin-Priority: 1001' | sudo tee /etc/apt/preferences.d/ngosen-fcitx5
sudo apt update
sudo apt install --allow-downgrades fcitx5-ngosen
```

Muốn dùng lại gói Ngó Sen thì xoá `/etc/apt/preferences.d/ngosen-fcitx5` rồi chạy `sudo apt upgrade`.

<a id="nixos"></a>

### 4. Cài trên NixOS

Thêm flake vào cấu hình; Nix build từ source trong repo:

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

Muốn build từ bản clone trên máy, xem mục NixOS trong [TU-DUNG.md](TU-DUNG.md#nixos).

<a id="cai-bang-tay"></a>

### 5. Cài bằng tay, tự build, bản thử

- Tự thiết lập desktop: thêm `--no-setup` vào lệnh cài
  (`... | bash -s -- --no-setup`).
- Không muốn chạy lệnh tải từ mạng: tải gói ở [Releases](https://github.com/ngosen/ngosen/releases) và cài
  bằng tay.
- Tự build từ source: xem [TU-DUNG.md](TU-DUNG.md).
- Test tính năng chưa có trong release: cài [version thử](https://github.com/ngosen/ngosen/releases/tag/next-build),
  build tự động khi nhánh `next` thay đổi:

  ```bash
  curl -fsSL https://raw.githubusercontent.com/ngosen/ngosen/next/install.sh | NGOSEN_RELEASE_URL=https://github.com/ngosen/ngosen/releases/download/next-build bash
  ```

<a id="thiet-lap"></a>
<a id="thiết-lập-sau-khi-cài-package"></a>

## 🛠️ Thiết lập sau khi cài đặt

Cài từ repo, từ package tải về hoặc dùng `--no-setup` thì chưa thêm Ngó Sen vào danh sách bộ gõ. Làm các
bước sau (hoặc chạy lệnh cài một dòng ở trên để nó làm giúp):

- **Thêm Ngó Sen vào danh sách bộ gõ:** mở “Cấu hình Fcitx 5” (`fcitx5-configtool`), tab bộ gõ, thêm
  “Ngó Sen”. Máy để ngôn ngữ tiếng Việt thì fcitx5 tự thêm ở lần chạy đầu.
- **Khởi động lại bộ gõ** sau mỗi lần cài hoặc cập nhật: chạy `fcitx5 -rd`. Trên KDE Wayland, đăng xuất rồi
  đăng nhập lại thay cho lệnh này.
- **GNOME:** đặt fcitx5 tự chạy khi đăng nhập và nạp biến môi trường, rồi đăng xuất và đăng nhập lại:

  ```sh
  mkdir -p ~/.config/autostart ~/.config/environment.d
  [ -e /etc/xdg/autostart/org.fcitx.Fcitx5.desktop ] || cp /usr/share/applications/org.fcitx.Fcitx5.desktop ~/.config/autostart/
  printf 'GTK_IM_MODULE=fcitx\nQT_IM_MODULE=fcitx\nXMODIFIERS=@im=fcitx\nSDL_IM_MODULE=fcitx\n' > ~/.config/environment.d/90-ngosen.conf
  ```

  Cài thêm extension [Input Method Panel](https://extensions.gnome.org/extension/261/) rồi đăng xuất,
  đăng nhập lại: không có nó, bảng emoji che lên chữ đang gõ trong app Wayland như Firefox, và thanh
  trên cùng có thể không có icon bộ gõ. Xem thêm
  [extension sửa lỗi gõ](#gnome-extension).
- **KDE Plasma (Wayland):** chọn **Fcitx 5** ở *System Settings → Virtual Keyboard*, rồi đăng xuất và đăng
  nhập lại. App X11 chạy qua XWayland (như Minecraft bản cũ) cần thêm
  `echo 'XMODIFIERS=@im=fcitx' > ~/.config/environment.d/90-ngosen.conf` trước khi đăng xuất. WPS Office
  (cả bản Flatpak) thì Ngó Sen tự cho chạy với `QT_IM_MODULE=fcitx` mỗi lần đăng nhập; cài WPS sau Ngó Sen
  thì đăng xuất, đăng nhập lại là gõ được.
- **Xfce, Cinnamon, MATE** (Debian, Ubuntu, Mint): chạy `im-config -n fcitx5`, rồi đăng xuất và đăng nhập lại.
- **Sway, Hyprland, niri:** chạy lệnh cài một dòng từ terminal trong phiên desktop để được thiết lập sẵn.
- **Kiểm tra lại:** sau khi đăng nhập lại, mở cửa sổ cài đặt Ngó Sen (`fcitx5-ngosen-settings`). Lần mở đầu
  tiên nó vào trang **Kiểm tra máy**, nơi liệt kê bước nào còn thiếu và có nút sửa cho nhiều bước.

<a id="khac-phuc-su-co"></a>
<a id="lỗi-thường-gặp"></a>

## 🔍 Khắc phục sự cố thường gặp

<a id="gnome-extension"></a>
<a id="gnome-extension-sửa-lỗi-gõ"></a>
<a id="ubuntu-2604-extension-sửa-lỗi-gõ"></a>

### GNOME mất phím hoặc không nhớ chế độ gõ theo app

Gói có kèm extension `forward-keys@ngosen.github.io` cho GNOME 46 đến 50. Chỉ cần bật nó trong ba trường hợp:
GNOME 50.0–50.3 (GNOME làm mất phím bộ gõ gửi cho app,
[báo lỗi trên Launchpad](https://bugs.launchpad.net/ubuntu/+source/mutter/+bug/2169784)), máy không có gói
`xdg-desktop-portal-gnome`, hoặc muốn nhớ chế độ gõ cho app không có tệp `.desktop`. Lệnh cài tự bật ở hai
trường hợp đầu. Bật bằng tay: đăng xuất, đăng nhập lại rồi chạy một lần

```bash
gnome-extensions enable forward-keys@ngosen.github.io
```

### Preedit mất chữ khi bấm chuột

Ở chế độ Preedit, chữ đang gõ dở (còn gạch chân) mất khi bấm chuột, ở những app nối với fcitx5 qua IBus:
Chrome, Chromium và app Electron trên GNOME Wayland, và app GTK dùng module ibus trên X11 (ví dụ xed và GNOME
Text Editor trên Linux Mint 22). Lỗi ở fcitx5 5.1.19 trở về trước, đã sửa ở 5.1.20
([báo lỗi trên Launchpad](https://bugs.launchpad.net/ubuntu/+source/fcitx5/+bug/2172006)). Trong lúc distro
chưa có bản mới, dùng chế độ **Gõ Sen**, hoặc gõ xong chữ bằng phím cách rồi mới bấm chuột.

### LibreOffice trên KDE Wayland lặp chữ khi bấm chuột

Ở chế độ Preedit trên KDE Plasma Wayland, đang có chữ gạch chân mà bấm chuột trong LibreOffice Writer hay Calc
thì chữ đó hiện hai lần (gõ "tiếng", bấm chuột, gõ `a` ra "tiếngtiếnga"). LibreOffice tự đưa chữ gạch chân vào
văn bản mà không báo cho bộ gõ; bộ gõ tiếng Trung cũng bị. Dùng chế độ **Gõ Sen**, gõ xong chữ bằng phím cách
rồi mới bấm chuột, hoặc cho LibreOffice nối với fcitx5 qua module Qt (cần gói fcitx5 cho Qt 6, như
`fcitx5-frontend-qt6` trên Debian/Ubuntu hay `fcitx5-qt6` trên Fedora):

```bash
for app in writer calc; do
  cp /usr/share/applications/libreoffice-$app.desktop ~/.local/share/applications/
  sed -i 's|^Exec=libreoffice|Exec=env QT_IM_MODULE=fcitx libreoffice|' \
    ~/.local/share/applications/libreoffice-$app.desktop
done
```

Tắt hẳn LibreOffice rồi mở lại từ menu. Các cửa sổ LibreOffice dùng chung một tiến trình, nên app mở đầu tiên
quyết định cho cả phiên; Impress chưa được thử. Muốn bỏ cách này thì xoá hai file
`~/.local/share/applications/libreoffice-writer.desktop` và `libreoffice-calc.desktop` cùng thư mục.

### VS Code trên Ubuntu

Nên dùng gói `.deb` từ [trang của Microsoft](https://code.visualstudio.com/download) hoặc
[Flatpak](https://flathub.org/apps/com.visualstudio.code), gõ được ngay. Bản snap gõ ra Telex thô (`tieengs`);
nếu buộc phải dùng snap, cho nó đi qua XIM:

```bash
cp /var/lib/snapd/desktop/applications/code_code.desktop ~/.local/share/applications/
sed -i 's|^Exec=/snap/bin/code|Exec=env GTK_IM_MODULE=xim XMODIFIERS=@im=fcitx /snap/bin/code|' \
  ~/.local/share/applications/code_code.desktop
```

Tắt hẳn VS Code rồi mở lại từ menu. Qua XIM, khoảng một câu trong mười có một chữ sai dấu. Muốn bỏ cách này
thì xoá file `~/.local/share/applications/code_code.desktop`.

### Gặp lỗi khác

Báo ở [Issues](https://github.com/ngosen/ngosen/issues). Không có tài khoản GitHub thì nhắn trang
[Facebook](https://www.facebook.com/profile.php?id=61595753582618): trong cửa sổ cài đặt, vào trang
“Kiểm tra máy”, bấm “Nhắn qua Facebook…” để chép sẵn thông tin máy rồi dán vào tin nhắn.

Nhờ AI tìm lỗi thì gửi nó câu này kèm mô tả lỗi: “Đọc https://github.com/ngosen/ngosen/blob/main/docs/ai-help.md
rồi giúp tôi sửa lỗi gõ Ngó Sen”. Trang đó dặn AI chỉ chẩn đoán, để bạn tự bấm nút sửa, và nhắc bạn đọc kỹ thông
tin trước khi dán cho AI. Muốn hỏi AI cách dùng Ngó Sen, chẳng hạn menu phím `` ` ``, hãy đưa nó link
https://github.com/ngosen/ngosen/blob/main/docs/ai-guide.md.

<a id="go-cai-dat"></a>

## 🗑️ Gỡ cài đặt

Lệnh hỏi lại rồi mới gỡ, và trả lại thiết lập desktop mà lệnh cài đã đổi:

```bash
curl -fsSL https://raw.githubusercontent.com/ngosen/ngosen/main/install.sh | bash -s -- --uninstall
```

Hoặc gỡ bằng trình quản lý gói rồi chạy `fcitx5 -rd`:

```bash
sudo dnf remove fcitx5-ngosen       # Fedora
sudo apt remove fcitx5-ngosen       # Ubuntu, Debian
sudo pacman -R fcitx5-ngosen        # Arch, CachyOS
sudo zypper remove fcitx5-ngosen    # openSUSE
```

Cấu hình được giữ lại để lần cài sau dùng tiếp: các file `ngosen*.conf` trong `~/.config/fcitx5/conf/` (và
`lotus*.conf` nếu từng dùng version trước 1.1.0). Muốn dọn sạch thì xoá các file đó.

<details>
<summary>Gỡ trên từng desktop</summary>

- **GNOME:** fcitx5 thôi tự chạy khi đăng nhập, app dùng lại IBus, tắt extension sửa lỗi gõ và gỡ Input Method
  Panel nếu lệnh cài đã thêm. Đăng xuất rồi đăng nhập lại. fcitx5 vẫn còn; muốn gỡ luôn thì chạy
  `sudo apt autoremove` (Ubuntu, Debian).
- **Xfce, Cinnamon, MATE:** trả bộ gõ về mặc định của máy; gỡ hẳn fcitx5 bằng `sudo apt remove fcitx5`.
- **KDE:** giữ fcitx5 làm bàn phím ảo vì có thể đang dùng cho bộ gõ khác; muốn bỏ thì chọn None ở System
  Settings → Virtual Keyboard.
- **Sway, Hyprland, niri:** bỏ dòng đã dán vào config, rồi đăng xuất và đăng nhập lại.

</details>

<a id="nguon-goc"></a>

## 🏛️ Nguồn gốc & ghi công

- Dựa trên mã của [fcitx5-lotus](https://github.com/LotusInputMethod/fcitx5-lotus) và
  [VMK](https://github.com/thanhpy2009/VMK), giấy phép **GPL-3.0-or-later**.
- Lõi biến `tieengs` thành `tiếng` là bản Rust [bamboo-core](https://github.com/bamboo-vi/bamboo-core) của
  [@nguyen10t2](https://github.com/nguyen10t2), port từ bản Go
  [bamboo-core](https://github.com/BambooEngine/bamboo-core) của [@t1ld3x](https://github.com/t1ld3x)
  (giấy phép **MIT**). Mọi gói Ngó Sen, kể cả Debian 12, đều dùng bản Rust.
- Danh sách từ tiếng Anh trong lõi lấy từ [SCOWL](https://wordlist.aspell.net) của Kevin Atkinson.
- Bảng của Đoán dấu là số đếm cụm chữ (không chứa câu nào) do [Tim Sen](https://github.com/ngosen/timsen)
  đếm trên Wikipedia tiếng Việt (CC BY-SA 4.0), FineWeb-2 (ODC-By 1.0) và phụ đề [OpenSubtitles](http://www.opensubtitles.org/) qua
  [OPUS](https://opus.nlpl.eu); bảng theo giấy phép **CC BY-SA 4.0**.
- Issue và pull request trước ngày 10/10/2026 nằm ở repo lưu trữ
  [ngosen/ngosen-fork-archive](https://github.com/ngosen/ngosen-fork-archive).

### Lấy mã hay ý tưởng từ Ngó Sen

- Chép hay sửa mã của Ngó Sen thì giữ giấy phép **GPL-3.0-or-later** và ghi nguồn, kể cả khi nhờ AI đọc mã
  rồi viết lại.
- Đưa cách làm của Ngó Sen sang bộ gõ khác (cách xoá chữ trong từng loại app, cách nhận ra ô địa chỉ trình
  duyệt…), kể cả khi chỉ lấy ý tưởng, thì ghi nguồn kèm link tới repo này trong README hoặc ghi chú phát hành.

<a id="english"></a>

## English

Ngó Sen is a Vietnamese input method for fcitx5. The maintainer only **vibecodes** this project: an AI coding
agent writes the code, then the maintainer measures and uses it daily. It is licensed under GPL-3.0-or-later.

Ngó Sen has two typing modes, Gõ Sen (for fast typing, browsers and Electron apps) and Preedit, and uses the
Rust [bamboo-core](https://github.com/bamboo-vi/bamboo-core) by [@nguyen10t2](https://github.com/nguyen10t2),
ported from the original Go [bamboo-core](https://github.com/BambooEngine/bamboo-core) by
[@t1ld3x](https://github.com/t1ld3x). Both projects are MIT licensed. The core's English word list comes from
[SCOWL](https://wordlist.aspell.net) by Kevin Atkinson. The table behind Đoán dấu holds word-sequence counts
made by [Tim Sen](https://github.com/ngosen/timsen) from Vietnamese Wikipedia, FineWeb-2 and [OpenSubtitles](http://www.opensubtitles.org/)
(via [OPUS](https://opus.nlpl.eu)), licensed CC BY-SA 4.0.

It includes Cứu chữ to retype what was typed before switching to Vietnamese, and Cứu dấu to change the tone
mark of a selected word. A convert hotkey turns selected TCVN3 or VNI text into the chosen charset, Unicode by default.
[The IBus version](https://github.com/ngosen/ibus-ngosen) shares the same typing core and has almost
all the same features; new features reach it a little later. If you reuse Ngó Sen's code, keep the GPL and its notices, including when
an AI tool rewrites it. If you port its approaches to another input method, credit this repository with a link.
The rest of this page is in Vietnamese.
