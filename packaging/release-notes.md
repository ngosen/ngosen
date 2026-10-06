Thay đổi của bản này: xem [CHANGELOG.md](https://github.com/ngosen/ngosen/blob/main/CHANGELOG.md).

## Chọn tệp theo bản phân phối

| Bản phân phối | Tệp | Mức đã thử |
| --- | --- | --- |
| Fedora 44 | `fcitx5-ngosen-*.fc44.x86_64.rpm` | dùng hằng ngày (KDE Plasma Wayland) |
| Fedora 43 | `fcitx5-ngosen-*.fc43.x86_64.rpm` | chỉ dựng và cài thử trong container |
| Arch, CachyOS | `fcitx5-ngosen-*-x86_64.pkg.tar.zst` | CachyOS dùng hằng ngày với bản tự dựng từ mã; gói này mới cài thử trong container |
| Ubuntu 24.04 | `fcitx5-ngosen_*_noble_amd64.deb` | dùng sơ (GNOME X11) với bản tự dựng; gói này mới cài thử trong container |
| Ubuntu 22.04, 26.04 | `…_jammy_amd64.deb`, `…_resolute_amd64.deb` | chỉ dựng |
| Debian 12, 13 | `…_bookworm_amd64.deb`, `…_trixie_amd64.deb` | chỉ dựng |
| openSUSE Tumbleweed | `fcitx5-ngosen-*.opensuse-tumbleweed.x86_64.rpm` | chỉ dựng và cài thử trong container |

"Chỉ dựng" nghĩa là gói dựng được và qua bộ kiểm lúc dựng, chưa ai gõ thử trên bản phân phối đó.

Gói này thay cho `fcitx5-lotus`; hai gói không cài chung được. Cài xong, bật máy chủ nền cho tài khoản
của mình rồi khởi động lại fcitx5:

```
sudo systemctl enable --now fcitx5-lotus-server@$(whoami).service
sudo systemctl restart fcitx5-lotus-server@$(whoami).service
fcitx5 -rd
```

Đối chiếu tệp tải về với `SHA256SUMS`: `sha256sum -c SHA256SUMS --ignore-missing`.
