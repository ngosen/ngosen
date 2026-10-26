[English](CONTRIBUTING.en.md) | [Tiếng Việt](CONTRIBUTING.md)

# Đóng góp cho Ngó Sen

Cảm ơn bạn quan tâm đến việc đóng góp cho Ngó Sen! Tài liệu này hướng dẫn bạn cách tham gia phát triển dự án.

## 📋 Mục lục

- [Bắt đầu](#bắt-đầu)
- [Quy trình đóng góp](#quy-trình-đóng-góp)
- [Quy tắc ứng xử](#quy-tắc-ứng-xử)
- [Quy tắc code style](#quy-tắc-code-style)
- [Quy trình Pull Request](#quy-trình-pull-request)
- [Lưu ý quan trọng về nhánh](#lưu-ý-quan-trọng-về-nhánh)
- [Báo cáo lỗi](#báo-cáo-lỗi)
- [Đề xuất tính năng](#đề-xuất-tính-năng)

## Bắt đầu

### Yêu cầu hệ thống

- GCC hoặc Clang với hỗ trợ C++17
- CMake >= 3.16
- Rust 1.88+ và cargo (cho lõi gõ)
- Fcitx5 development headers
- Git

### Cài đặt và build

```bash
# Clone repository
git clone https://github.com/ngosen/ngosen.git
cd ngosen

# Build
mkdir build && cd build
cmake ..
make -j$(nproc)
```

## Quy trình đóng góp

### 1. Fork repository

Fork repository này trên GitHub và clone fork của bạn về máy.

```bash
git clone https://github.com/yourusername/ngosen.git
cd ngosen
git remote add upstream https://github.com/ngosen/ngosen.git
```

### 2. Tạo nhánh mới

Luôn tạo nhánh mới cho mỗi tính năng hoặc sửa lỗi:

```bash
git checkout next
git pull upstream next
git checkout -b feature/ten-tinh-nang
# hoặc
git checkout -b fix/ten-loi
```

### 3. Thực hiện thay đổi

- Viết code sạch, dễ đọc
- Tuân thủ quy tắc code style (xem bên dưới)
- Cập nhật tài liệu nếu cần

### 4. Commit changes

Sử dụng commit messages rõ ràng và mô tả:

```
feat: add emoji support

- Add emoji_data.h
- Update generate_emoji_header.py
```

## Quy tắc ứng xử

Mọi thành viên tham gia đóng góp cho dự án này đều phải tuân thủ [Quy tắc Ứng xử cho Cộng tác viên](CODE_OF_CONDUCT.md) để xây dựng một cộng đồng cởi mở, thân thiện và lành mạnh.

## Quy tắc code style

- **C/C++**: Tuân thủ file [`.clang-format`](.clang-format) trong repository.
- **Python (`settings-gui`)**: Tuân thủ lint và format của `ruff` (`ruff check` và `ruff format`).
- Khuyến khích tạo git hook `pre-commit` để tự động format code trước khi commit bằng cách tạo file `.git/hooks/pre-commit` với nội dung:

```bash
#!/bin/bash
# Format C/C++
CPP_FILES=$(git diff --cached --name-only --diff-filter=ACMR \vert{} grep -E '\.(cpp\vert{}h)$')
if [ -n "$CPP_FILES" ]; then
    for file in $CPP_FILES; do
        clang-format -i "$file"
        git add "$file"
    done
fi

# Lint & Format Python
PY_FILES=$(git diff --cached --name-only --diff-filter=ACMR \vert{} grep -E '^settings-gui/.*\.py$')
if [ -n "$PY_FILES" ]; then
    ruff check --fix settings-gui
    ruff format settings-gui
    for file in $PY_FILES; do
        git add "$file"
    done
fi
```

Sau đó chạy lệnh: `chmod +x .git/hooks/pre-commit`

## Quy trình Pull Request

### 1. Đảm bảo code sạch bằng `clang-format` và `ruff`

```bash
# C/C++
clang-format -i src/*.cpp src/*.h

# Python
ruff check --fix settings-gui
ruff format settings-gui
```

### 2. Chạy test

```bash
# Build C++ code
cd build
cmake .. && make
```

### 3. Rebase với nhánh next

```bash
git checkout next
git pull upstream next
git checkout feature/ten-tinh-nang
git rebase next
```

### 4. Push và tạo PR

```bash
git push origin feature/ten-tinh-nang
```

Tạo Pull Request trên GitHub với:

- Tiêu đề mô tả rõ ràng
- Mô tả chi tiết thay đổi
- Liên kết đến issue liên quan (nếu có)
- Screenshot cho UI changes

## Lưu ý quan trọng về nhánh

### PR vào `next` hoặc `patch`, không bao giờ vào `main`

- Nhánh `main` chỉ chứa release và chỉ được merge bởi maintainer. Lệnh cài tải `install.sh` từ `main`,
  nên thứ gì vào `main` là chạy ngay trên máy người dùng.
- Tính năng mới và mọi thay đổi khác: PR vào `next`.
- Sửa lỗi của version đang phát hành: PR vào `patch`. Maintainer ra version sửa lỗi (vd. 1.4.2 → 1.4.3) từ `patch`,
  rồi merge `patch` sang `next`.
- Không chắc thì gửi vào `next`; maintainer sẽ chuyển nếu cần.

### Cấu trúc nhánh

```
main    ← release (chỉ maintainer được merge)
  ↑            ↑
next          patch
(tính năng)   (sửa lỗi version đang phát hành)
  ↑            ↑
feature/*, fix/*  ← Nhánh cá nhân cho mỗi PR
```

### Quy trình merge

1. Developer tạo PR vào `next` (tính năng) hoặc `patch` (sửa lỗi)
2. Code review bởi maintainer. Muốn người báo lỗi test bản sửa trước khi merge, maintainer gắn nhãn
   `test-build`: CI đăng các gói thành version thử `pr-<số PR>-build` và để link trong PR. Đóng PR là
   version thử bị xoá
3. Merge, rồi test trên version thử `next-build`
4. Version sửa lỗi: `patch` → `main`, rồi `patch` → `next`. Version tính năng: `next` → `main`, rồi đưa `patch`
   về bằng `main`

## Báo cáo lỗi

Khi báo cáo lỗi, vui lòng cung cấp:

- Version Ngó Sen
- Hệ điều hành và version
- Các bước để tái hiện lỗi
- Log hoặc screenshot (nếu có) (Log bằng lệnh fcitx5-diagnose)
- Môi trường (desktop environment, terminal, v.v.)

## Đề xuất tính năng

Trước khi đề xuất tính năng mới:

- Kiểm tra xem tính năng đã tồn tại chưa
- Tìm kiếm issue tương tự
- Mô tả rõ ràng tính năng và use case
- Giải thích tại sao tính năng này cần thiết

## Giấy phép

Bằng cách đóng góp, bạn đồng ý rằng code của bạn sẽ được cấp phép theo cùng giấy phép với dự án (GPL-3.0-or-later).

---

Cảm ơn bạn đã đóng góp!
