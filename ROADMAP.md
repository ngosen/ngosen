# Lộ trình

Những gì Ngó Sen định làm tiếp, theo thứ tự. Mỗi version mang một tính năng chính, để mỗi lần cập nhật
bạn chỉ phải làm quen một thứ mới. Tính năng làm xong trước lượt thì để dành cho version sau.

Ở đây không ghi ngày phát hành. Chỉ những việc đã gắn [milestone](https://github.com/ngosen/ngosen/milestones)
mới có số version, và thứ tự có thể đổi. Muốn đề nghị hay góp ý thì mở
[issue](https://github.com/ngosen/ngosen/issues).

## Sau đó

Chưa có số version, xếp theo thứ tự định làm:

1. Bớt đổi nhầm chữ tiếng Anh thành tiếng Việt, như `nest` thành `nét`, `too` thành `tô`.
2. Đoán dấu: gõ cả câu không dấu rồi bấm một phím để thêm dấu.
3. Bôi đen một đoạn không dấu rồi bấm phím để thêm dấu cả đoạn. Chuyển mã cũ (TCVN3, VNI) sang Unicode
   và bỏ dấu đoạn bôi đen đã xong ở #655.
4. Sửa lỗi gõ vội, như `tiếgn` thành `tiếng`.
5. Bảng các app gõ được, lấy từ kết quả test người dùng gửi về. Tự mở trang Kiểm tra máy đã xong ở 1.12.
6. Đưa Ngó Sen vào repo của các distro: từ 1.5.3 đã có repo riêng có chữ ký cho Fedora (COPR),
   Debian và Ubuntu (APT), Arch (pacman) và openSUSE Tumbleweed. nixpkgs đang chờ review
   ([NixOS/nixpkgs#575620](https://github.com/NixOS/nixpkgs/pull/575620)). Từ 1.13 có gói AUR;
   kho Debian đã nhận bản 1.9.2 vào testing.
