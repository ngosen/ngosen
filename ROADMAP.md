# Lộ trình

Những gì Ngó Sen định làm tiếp, theo thứ tự. Mỗi version mang một tính năng chính, để mỗi lần cập nhật
bạn chỉ phải làm quen một thứ mới. Tính năng làm xong trước lượt thì để dành cho version sau.

Ở đây không ghi ngày phát hành. Chỉ những việc đã gắn [milestone](https://github.com/ngosen/ngosen/milestones)
mới có số version, và từ 1.6.0 trở đi thứ tự có thể đổi. Muốn đề nghị hay góp ý thì mở
[issue](https://github.com/ngosen/ngosen/issues).

## 1.6.0

- Cứu chữ: quên bật tiếng Việt mà đã gõ `tieengs vieetj`, bấm phím chuyển sang Ngó Sen là chữ thành
  `tiếng việt` (#71, #80, #82, #83). Đã xong, có thể thử ngay bằng
  [test build](README.md#test-tính-năng-chưa-có-trong-release).
- Cứu dấu: bôi đen chữ rồi gõ dấu (`nhanh` + `s` → `nhánh`) ở app hỗ trợ surrounding text.
  Đã xong, có thể thử ngay bằng
  [test build](README.md#test-tính-năng-chưa-có-trong-release).
  Click vào chữ rồi gõ dấu, không cần bôi đen: chưa làm, để sau (#118, #272).
- Có gì mới: sau khi cập nhật lên version có tính năng mới, một thông báo nhỏ ở góc màn hình hiện một lần,
  bấm vào thì mở trang "Có gì mới" trong cửa sổ cài đặt, mỗi tính năng có cách dùng và nút tắt. Bản chỉ sửa
  lỗi thì không báo.

## 1.7.0

- Làm lại cách chèn emoji (#110).
- Gộp "Kiểm tra chính tả" vào "Tự khôi phục từ gõ sai", vì nó chỉ có tác dụng khi Tự khôi phục đang bật
  (#112).

## Sau đó

Chưa có số version, xếp theo thứ tự định làm:

1. Chữ tiếng Anh có chữ cái đôi không còn mất phím khi trả về: `affect` không thành `afect`, `coffee`
   không thành `cofee`.
2. Bớt đổi nhầm chữ tiếng Anh thành tiếng Việt, như `nest` thành `nét`, `too` thành `tô`.
3. Đoán dấu: gõ cả câu không dấu rồi bấm một phím để thêm dấu.
4. Bôi đen một đoạn rồi thêm dấu, bỏ dấu hoặc chuyển chữ mã cũ (TCVN3, VNI) sang Unicode.
5. Sửa lỗi gõ vội, như `tiếgn` thành `tiếng`.
6. Tự chạy "Kiểm tra máy tôi" lần đầu mở cửa sổ cài đặt. Có thêm bảng các app gõ được, lấy từ kết quả test
   người dùng gửi về.
7. Mẹo dùng Ngó Sen trong cửa sổ cài đặt, mỗi mẹo dẫn sang trang hướng dẫn trên website. Mẹo có sẵn trong
   gói cài, không tải gì từ mạng.
8. Đưa Ngó Sen vào repo của các distro: từ 1.5.3 đã có repo riêng có chữ ký cho Fedora (COPR),
   Debian và Ubuntu (APT), Arch (pacman) và openSUSE Tumbleweed. nixpkgs đang chờ review
   ([NixOS/nixpkgs#575620](https://github.com/NixOS/nixpkgs/pull/575620)). Còn AUR, rồi Debian.
