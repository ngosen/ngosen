# Tự dựng Ngó Sen từ mã

Cách nhanh hơn là cài gói dựng sẵn, xem mục [Cài](README.md#cài) trong README. Tệp này dành cho ai muốn
tự dựng, hoặc dùng bản phân phối chưa có gói.

Các bước dưới đây cho Arch và CachyOS. Bản phân phối khác thì cài các gói tương ứng, các bước còn lại
giống hệt.

**1. Máy từng tự dựng Ngó Sen 1.0.0 trở về trước:** xoá các tệp mang tên `lotus` mà bản đó cài, vì
`cmake --install` chỉ thêm tệp chứ không xoá. Nếu máy có cài gói `fcitx5-lotus` thì bỏ qua bước này, vì
các tệp đó thuộc về gói đó.

```
sudo rm -f /usr/lib/fcitx5/liblotus.so /usr/share/fcitx5/addon/lotus.conf /usr/share/fcitx5/inputmethod/lotus.conf \
    /usr/bin/fcitx5-lotus-settings
sudo rm -rf /usr/share/fcitx5/lotus /usr/share/fcitx5-lotus
```

Cấu hình cũ trong `~/.config/fcitx5/conf/lotus*.conf` được tự chép sang `ngosen*.conf` ở lần chạy đầu.

**2. Cài công cụ dựng.**

```
sudo pacman -S --needed cmake extra-cmake-modules gcc go git python make pkgconf fcitx5 hicolor-icon-theme python-qtpy python-dbus librsvg libxcb
```

**3. Tải mã và dựng.** Phải cài vào `/usr`, vì fcitx5 tìm bộ gõ ở đó. Nhớ
`--recurse-submodules`, vì lõi ghép dấu nằm ở kho con `bamboo-core`.

```
git clone --recurse-submodules -b main https://github.com/ngosen/ngosen.git
cd ngosen
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=/usr/lib
cmake --build build -j8
sudo cmake --install build
```

Trên Fedora và openSUSE dùng `-DCMAKE_INSTALL_LIBDIR=lib64` thay cho `/usr/lib`.

**4. Máy từng chạy máy chủ nền của Lotus hoặc Ngó Sen bản cũ:** tắt nó và xoá các tệp nó để lại, vì
`cmake --install` chỉ thêm tệp chứ không xoá. Máy chưa từng cài thì bỏ qua bước này.

```
sudo systemctl disable --now fcitx5-lotus-server@$(whoami).service
sudo rm -f /usr/bin/fcitx5-lotus-server /usr/lib/systemd/system/fcitx5-lotus-server@.service \
    /usr/lib/udev/rules.d/99-lotus.rules /usr/lib/sysusers.d/lotus.conf /usr/lib/modules-load.d/fcitx5-lotus.conf
sudo userdel uinput_proxy
sudo systemctl daemon-reload
```

**5. Thêm bộ gõ.** Khởi động lại fcitx5 (hoặc đăng xuất rồi đăng nhập lại), mở "Fcitx5 Configuration"
và thêm Ngó Sen. Trên KDE Wayland: System Settings → Virtual Keyboard → chọn "Fcitx 5".

**Cập nhật bản mới về sau:** trong thư mục `ngosen`, chạy `git pull --recurse-submodules`, lặp lại
bước 3, rồi khởi động lại fcitx5:

```
fcitx5 -rd
```

## NixOS

Trên NixOS không cài vào `/usr` được, nên các bước ở trên không dùng được. Hãy dựng bằng flake trong
kho:

```
git clone -b main https://github.com/ngosen/ngosen.git
cd ngosen
nix build              # dựng gói và chạy test, kết quả ở ./result
nix flake check        # thêm bài thử nạp bộ gõ vào fcitx5 trên màn hình X ảo
```

Muốn dùng bản vừa dựng thì trỏ input của cấu hình vào thư mục đó, phần còn lại giống mục Cài trong
README, rồi `sudo nixos-rebuild switch`:

```nix
inputs.ngosen.url = "git+file:///home/ten-ban/ngosen";
```

Flake chỉ thấy các tệp git đang theo dõi, nên tệp mới phải `git add` trước khi dựng. Đang sửa mã mà
muốn dựng lại nhanh thì chạy `nix develop` để mở shell có đủ công cụ, rồi dựng và chạy test như mục
Chạy bộ kiểm bên dưới, thêm `-DNGOSEN_RUST_CORE=ON` vì gói Nix dùng lõi Rust.

## Chạy bộ kiểm

Bộ kiểm mặc định tắt, bật bằng `-DBUILD_TESTING=ON`:

```
cmake -B build-test -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build-test -j8
unshare -Urn ctest --test-dir build-test
```
