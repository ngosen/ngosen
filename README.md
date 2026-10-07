# Ngó Sen — bộ gõ tiếng Việt cho fcitx5

Ngó Sen được dùng hằng ngày trên Fedora 44 và CachyOS (KDE Plasma Wayland), và được thử thêm trên máy ảo
Ubuntu 26.04 (GNOME, Wayland), MX Linux (Xfce, X11) và Linux Mint (Cinnamon, X11). Kho để công khai cho
ai cần thì lấy dùng, **không hứa hỗ trợ**. Gặp lỗi thì báo ở mục
[Issues](https://github.com/ngosen/ngosen/issues).

## Roadmap: Ngó Sen 1.0

Bản 1.0 nhắm bốn thay đổi lớn. Chưa có ngày phát hành.

- **Bỏ máy chủ nền uinput.** Hiện bộ gõ cần một chương trình chạy ngầm có quyền đặc biệt để xoá chữ cũ.
  Bản 1.0 không cần nó nữa: cài xong là gõ, không phải bật dịch vụ, không cần quyền thiết bị.
- **Chỉ còn hai chế độ gõ: Gõ Sen và Preedit.** Gõ Sen là chế độ dùng cho mọi chỗ: gõ nhanh không mất
  chữ, gõ đúng trong trình duyệt, thanh địa chỉ, Facebook, app Electron như Zalo, và terminal.
  Preedit là chế độ hiện chữ gạch chân trong lúc gõ, dành cho app không hợp với Gõ Sen. Người dùng không
  còn phải chọn giữa nhiều chế độ khó hiểu.
- **Lõi ghép dấu (bamboo-core) chuyển sang Rust.** Phần biến `tieengs` thành `tiếng` đang viết bằng
  Go, sẽ đổi sang bản viết bằng Rust. Chỉ đổi khi bản mới gõ ra y hệt bản cũ.
- **Tách lõi Ngó Sen để dùng được ở nhiều nơi.** Ngoài fcitx5, Ngó Sen sẽ có bản cho IBus (bộ gõ mặc
  định của GNOME và Ubuntu) và cho các môi trường dùng wlroots như Sway.

## Nên dùng chế độ nào

Dùng **`Uinput`** làm chế độ gõ chính: chọn trong cửa sổ cài đặt Ngó Sen, mục chế độ mặc định. Bản 1.0
sẽ đổi tên chế độ này thành Gõ Sen.

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

**Cài xong, hoặc mỗi lần cập nhật,** khởi động lại máy chủ nền rồi khởi động lại fcitx5:

```
sudo systemctl restart fcitx5-lotus-server@$(whoami).service
fcitx5 -rd
```

### Ubuntu 26.04: bật extension sửa lỗi gõ

Có từ bản phát hành sau `3.5.10-4`. Cài xong, đăng xuất rồi đăng nhập lại, sau đó chạy một lần:

```
gnome-extensions enable forward-keys@ngosen.github.io
```

Khi Ubuntu tự sửa lỗi này ([báo lỗi trên Launchpad](https://bugs.launchpad.net/ubuntu/+source/mutter/+bug/2169784))
thì tắt extension bằng `gnome-extensions disable forward-keys@ngosen.github.io`.

## Gỡ

1. Bỏ Ngó Sen khỏi danh sách bộ gõ trong "Fcitx5 Configuration".
2. Tắt máy chủ nền:

   ```
   sudo systemctl disable --now fcitx5-lotus-server@$(whoami).service
   ```

3. Gỡ gói rồi khởi động lại fcitx5:

   ```
   sudo dnf remove fcitx5-ngosen       # Fedora
   sudo apt remove fcitx5-ngosen       # Ubuntu, Debian
   sudo pacman -R fcitx5-ngosen        # Arch, CachyOS
   sudo zypper remove fcitx5-ngosen    # openSUSE
   fcitx5 -rd
   ```

Cấu hình (các tệp `lotus*.conf` trong `~/.config/fcitx5/conf/`) vẫn ở lại máy, để lần cài sau dùng
tiếp. Muốn xoá sạch thì xoá các tệp đó và chạy `sudo userdel uinput_proxy`.

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

Planned for 1.0: no background uinput server; two typing modes, Gõ Sen (one mode that works for fast
typing, browsers and Electron apps) and Preedit; the Bamboo composition core ported from Go to Rust; and
a shared core that can plug into fcitx5, IBus and wlroots compositors. The rest of this page is in
Vietnamese.
