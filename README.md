# Ngó Sen

**Bộ gõ tiếng Việt cho fcitx5 trên Linux.**

- **Không cần quyền đặc biệt.** Ngó Sen không còn uinput server, chỉ dùng đúng quyền của fcitx5. Cài
  xong gõ được ngay, không phải bật service hay cấp quyền thiết bị.
- **Gõ thẳng, không gạch chân.** Chế độ Gõ Sen đưa chữ vào app ngay khi gõ. Ô gợi ý của thanh địa chỉ hay
  ô tìm kiếm chạy theo từng phím.
- **Một chế độ cho mọi app.** Trình duyệt, terminal, Zalo, LibreOffice đều dùng Gõ Sen. Ngó Sen tự nhận ra
  app nhận chữ kiểu gì, không phải đổi chế độ.
- **Lõi Rust.** Phần biến `tieengs` thành `tiếng` được viết lại bằng Rust. Gói cho Fedora, Arch, openSUSE
  và Ubuntu 24.04 trở lên dùng lõi Rust.
- **Cài bằng một dòng lệnh** trên Fedora, Ubuntu, Debian, Arch, CachyOS, openSUSE; script kiểm hash trước
  khi cài.

Gặp lỗi thì báo ở mục [Issues](https://github.com/ngosen/ngosen/issues).

## Chỉ cho fcitx5

Ngó Sen chỉ làm cho fcitx5. Bản IBus và bản chạy thẳng trên Sway, Hyprland đã bỏ. Mã bản IBus vẫn còn ở
nhánh `feat/ibus-engine`.

## Nên dùng chế độ nào

Dùng **Gõ Sen** làm chế độ gõ chính: chọn trong cửa sổ cài đặt Ngó Sen, mục chế độ mặc định. Chế độ
này trước tên là `Uinput`; cấu hình cũ tự chuyển sang tên mới. Chế độ Surrounding Text cũ cũng tự chuyển
thành Gõ Sen.

Kiểu gõ được dùng và kiểm hằng ngày là **Telex**. VNI và các kiểu khác dùng được nhưng chưa kiểm kỹ.

## Cài

Một dòng lệnh, cho Fedora, Ubuntu, Debian, Arch, CachyOS và openSUSE Tumbleweed (máy x86_64):

```
curl -fsSL https://raw.githubusercontent.com/ngosen/ngosen/main/install.sh | bash
```

Lệnh này tải gói cho đúng bản phân phối từ
[bản phát hành mới nhất](https://github.com/ngosen/ngosen/releases/latest), hỏi lại rồi mới cài. Nếu
máy đang có Lotus thì Ngó Sen sẽ thay nó; cấu hình cũ giữ nguyên. Muốn lên bản mới thì chạy lại đúng
lệnh đó.

Không muốn chạy lệnh tải từ mạng thì tải gói ở trang
[Releases](https://github.com/ngosen/ngosen/releases) rồi cài bằng tay. Muốn tự dựng từ mã thì xem
[TU-DUNG.md](TU-DUNG.md).

Trên NixOS, thêm flake của kho này vào cấu hình rồi đưa gói vào addon của fcitx5. Gói được dựng từ mã
trong kho:

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

CI dựng gói này, chạy test và nạp nó vào fcitx5 trên một màn hình X ảo, nhưng chưa ai thử trên máy
NixOS thật. Gặp lỗi xin mở issue. Tự dựng từ bản clone trên máy: xem mục NixOS trong
[TU-DUNG.md](TU-DUNG.md#nixos).

**Cài xong, hoặc mỗi lần cập nhật,** khởi động lại fcitx5:

```
fcitx5 -rd
```

### Ubuntu 26.04: bật extension sửa lỗi gõ

Có từ bản `0.5.0-1`. Cài xong, đăng xuất rồi đăng nhập lại, sau đó chạy một lần:

```
gnome-extensions enable forward-keys@ngosen.github.io
```

Khi Ubuntu tự sửa lỗi này ([báo lỗi trên Launchpad](https://bugs.launchpad.net/ubuntu/+source/mutter/+bug/2169784))
thì tắt extension bằng `gnome-extensions disable forward-keys@ngosen.github.io`.

### Ubuntu: VS Code

Nên cài VS Code bằng gói `.deb` từ [trang của Microsoft](https://code.visualstudio.com/download) hoặc
bằng Flatpak từ [Flathub](https://flathub.org/apps/com.visualstudio.code). Hai bản này gõ được tiếng
Việt ngay, không cần chỉnh gì.

Bản snap (cài từ App Center) không mang theo phần nối với fcitx5, nên gõ ra chữ Telex thô (`tieengs`).
Nếu buộc phải dùng snap, cho nó đi qua XIM (đường nối kiểu cũ của X11) bằng cách chép lối tắt của nó
vào thư mục nhà rồi sửa dòng mở app:

```
cp /var/lib/snapd/desktop/applications/code_code.desktop ~/.local/share/applications/
sed -i 's|^Exec=/snap/bin/code|Exec=env GTK_IM_MODULE=xim XMODIFIERS=@im=fcitx /snap/bin/code|' \
  ~/.local/share/applications/code_code.desktop
```

Tắt hẳn VS Code rồi mở lại từ menu. Qua XIM, khoảng một câu trong mười có một chữ bị sai dấu. Muốn bỏ
cách này thì xoá tệp `~/.local/share/applications/code_code.desktop`.

## Gỡ

1. Bỏ Ngó Sen khỏi danh sách bộ gõ trong "Fcitx5 Configuration".
2. Gỡ gói rồi khởi động lại fcitx5:

   ```
   sudo dnf remove fcitx5-ngosen       # Fedora
   sudo apt remove fcitx5-ngosen       # Ubuntu, Debian
   sudo pacman -R fcitx5-ngosen        # Arch, CachyOS
   sudo zypper remove fcitx5-ngosen    # openSUSE
   fcitx5 -rd
   ```

Cấu hình (các tệp `lotus*.conf` trong `~/.config/fcitx5/conf/`) vẫn ở lại máy, để lần cài sau dùng
tiếp. Muốn xoá sạch thì xoá các tệp đó.

## Nguồn gốc

Người giữ dự án chỉ **vibecode** dự án này: nêu việc cho trợ lý AI viết mã, rồi đo và dùng thử hằng
ngày.

Ngó Sen tách ra từ [fcitx5-lotus](https://github.com/LotusInputMethod/fcitx5-lotus), vốn là bản fork
của [bộ gõ VMK](https://github.com/thanhpy2009/VMK). Ngó sen là mầm mọc ra từ cây sen: cùng gốc với
Lotus nhưng đi hướng riêng. Giấy phép vẫn là GPL-3.0-or-later.

## English

Ngó Sen is a Vietnamese input method for fcitx5. The maintainer only **vibecodes** this project: the
code is written with an AI coding agent, then measured and used daily by the maintainer. It is a fork of
[fcitx5-lotus](https://github.com/LotusInputMethod/fcitx5-lotus), itself a fork of
[VMK](https://github.com/thanhpy2009/VMK), under GPL-3.0-or-later, published as is with no promise of
support.

Ngó Sen 1.0 has no background uinput server; two typing modes, Gõ Sen (one mode that works for fast
typing, browsers and Electron apps) and Preedit; and the Bamboo composition core ported from Go to Rust.
Ngó Sen targets fcitx5 only; the IBus engine was dropped and stays unmaintained on the
`feat/ibus-engine` branch. The rest of this page is in Vietnamese.
