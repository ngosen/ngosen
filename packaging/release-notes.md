Thay đổi của bản này: xem [CHANGELOG.md](https://github.com/ngosen/ngosen/blob/main/CHANGELOG.md).

Từ bản `0.5.0` Ngó Sen đánh số riêng, không theo số `3.5.10` của bản gốc nữa. Trình quản lý gói vẫn coi
bản này mới hơn `3.5.10-4`, nên cập nhật như thường.

## Chọn tệp theo bản phân phối

| Bản phân phối           | Tệp                                              | Mức đã thử                                                               |
| ----------------------- | ------------------------------------------------ | ------------------------------------------------------------------------ |
| Fedora 44               | `fcitx5-ngosen-*.fc44.x86_64.rpm`                | dùng hằng ngày (KDE Plasma Wayland)                                      |
| Fedora 43               | `fcitx5-ngosen-*.fc43.x86_64.rpm`                | chỉ dựng                                                                 |
| Arch, CachyOS           | `fcitx5-ngosen-*-x86_64.pkg.tar.zst`             | gõ thử 5 app ở chế độ Gõ Sen trên máy ảo CachyOS (Hyprland)              |
| Ubuntu 26.04            | `…_resolute_amd64.deb`                           | gõ thử 9 app trên máy ảo (GNOME Wayland), với bản tự dựng                |
| Ubuntu 24.04            | `fcitx5-ngosen_*_noble_amd64.deb`                | gõ thử 7 app trên máy ảo Linux Mint 22 (cùng nền 24.04), với bản tự dựng |
| Debian 13               | `…_trixie_amd64.deb`                             | gõ thử 6 app trên máy ảo MX 25 (cùng nền Debian 13)                      |
| Ubuntu 22.04, Debian 12 | `…_jammy_amd64.deb`, `…_bookworm_amd64.deb`      | chỉ dựng                                                                 |
| openSUSE Tumbleweed     | `fcitx5-ngosen-*.opensuse-tumbleweed.x86_64.rpm` | chỉ dựng                                                                 |

"Chỉ dựng" nghĩa là gói dựng được và qua bộ kiểm lúc dựng, chưa ai gõ thử trên bản phân phối đó.

Cột "Mức đã thử" là lần gõ thử bản 0.5. Các bản sửa bảng tính mới trong 1.0 được gõ thử trên máy ảo CachyOS
(KDE Wayland): LibreOffice Calc, WPS Office và Google Sheets trong Firefox.

Lõi ghép dấu viết bằng Rust có trong gói Fedora, Arch, openSUSE và Ubuntu 24.04 trở lên. Debian 12, 13 và
Ubuntu 22.04 vẫn dùng lõi Go, vì Rust có sẵn ở đó quá cũ. Hai lõi gõ ra chữ như nhau.

Gói này thay cho `fcitx5-lotus`; hai gói không cài chung được. Cài xong, hoặc mỗi lần cập nhật, khởi động
lại fcitx5:

```
fcitx5 -rd
```

Ubuntu 26.04: xem mục "bật extension sửa lỗi gõ" trong [README](https://github.com/ngosen/ngosen#cài).

Đối chiếu tệp tải về với `SHA256SUMS`: `sha256sum -c SHA256SUMS --ignore-missing`.
