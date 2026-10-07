# Tự dựng Ngó Sen từ mã

Cách nhanh hơn là cài gói dựng sẵn, xem mục [Cài](README.md#cài) trong README. Tệp này dành cho ai muốn
tự dựng, hoặc dùng bản phân phối chưa có gói.

Các bước dưới đây cho Arch và CachyOS. Bản phân phối khác thì cài các gói tương ứng, các bước còn lại
giống hệt.

**1. Gỡ bản Lotus đóng gói sẵn, nếu máy đã có.** Không gỡ thì tệp của hai bản đè lên nhau, và lần cập
nhật hệ thống sau sẽ báo lỗi tệp xung đột.

```
pacman -Qs fcitx5-lotus
sudo pacman -R fcitx5-lotus
```

**2. Cài công cụ dựng.**

```
sudo pacman -S --needed cmake extra-cmake-modules gcc go git python make pkgconf acl fcitx5 libinput hicolor-icon-theme python-qtpy python-dbus librsvg libxcb
```

**3. Tải mã và dựng.** Phải cài vào `/usr`, vì dịch vụ nền tìm chương trình ở đó. Nhớ
`--recurse-submodules`, vì lõi ghép dấu nằm ở kho con `bamboo-core`.

```
git clone --recurse-submodules -b main https://github.com/ngosen/ngosen.git
cd ngosen
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=/usr/lib
cmake --build build -j8
sudo cmake --install build
```

Trên Fedora và openSUSE dùng `-DCMAKE_INSTALL_LIBDIR=lib64` thay cho `/usr/lib`.

**4. Bật máy chủ nền cho tài khoản của mình.**

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

Máy từng cài Lotus thì chạy thêm lệnh này một lần, trước lệnh `restart`:

```
sudo gpasswd -d uinput_proxy input
```

**5. Thêm bộ gõ.** Khởi động lại fcitx5 (hoặc đăng xuất rồi đăng nhập lại), mở "Fcitx5 Configuration"
và thêm Ngó Sen. Trên KDE Wayland: System Settings → Virtual Keyboard → chọn "Fcitx 5".

**Cập nhật bản mới về sau:** trong thư mục `ngosen`, chạy `git pull --recurse-submodules`, lặp lại
bước 3, rồi chạy:

```
sudo udevadm control --reload-rules
sudo udevadm trigger --subsystem-match=misc --subsystem-match=input
sudo systemctl daemon-reload
sudo systemctl restart fcitx5-lotus-server@$(whoami).service
fcitx5 -rd
```

## Chạy bộ kiểm

Bộ kiểm mặc định tắt, bật bằng `-DBUILD_TESTING=ON`:

```
cmake -B build-test -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build-test -j8
unshare -Urn ctest --test-dir build-test
```
