# Mô tả

<!-- Mô tả rõ ràng và chi tiết thay đổi của bạn: vấn đề gì, giải quyết thế nào. -->

## Loại thay đổi

- [ ] Tính năng mới
- [ ] Sửa lỗi
- [ ] Cải thiện hiệu năng
- [ ] Cập nhật tài liệu
- [ ] Cải tiến CI/CD

## Liên kết issue

<!-- Fixes #123 (nếu có) -->

## Screenshot (cho UI changes)

<!-- Dán screenshot trước/sau nếu thay đổi giao diện -->

# Checklist

- [ ] Nhánh đích là `main`
- [ ] Đã chạy `clang-format` (tuân thủ `.clang-format`)
- [ ] Đã kiểm tra `ruff check` và `ruff format` (nếu sửa `settings-gui`)
- [ ] Build và test pass (`cmake -B build -DBUILD_TESTING=ON && cmake --build build && ctest --test-dir build`)
- [ ] Sửa lỗi thì có test hỏng khi bỏ bản sửa
- [ ] Đã thêm một dòng vào `CHANGELOG.md` mục `[Chưa phát hành]` (xem `RELEASING.md`)
