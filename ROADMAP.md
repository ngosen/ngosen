# Lộ trình

Những gì Ngó Sen định làm tiếp, theo thứ tự. Mỗi version mang một tính năng chính, để mỗi lần cập nhật
bạn chỉ phải làm quen một thứ mới. Tính năng làm xong trước lượt thì để dành cho version sau.

Ở đây không ghi ngày phát hành. Chỉ những việc đã gắn [milestone](https://github.com/ngosen/ngosen/milestones)
mới có số version, và từ 1.6.0 trở đi thứ tự có thể đổi. Muốn đề nghị hay góp ý thì mở
[issue](https://github.com/ngosen/ngosen/issues).

## 1.5.0

- Nút "Kiểm tra máy tôi…" trong cửa sổ cài đặt: xem fcitx5 và Ngó Sen đã cài đúng chưa, lỗi nào cũng kèm
  cách sửa, và test được một app gõ đúng hay sai (#88, #90).
- Gộp "Kiểm tra chính tả" vào "Tự khôi phục từ gõ sai", vì nó chỉ có tác dụng khi Tự khôi phục đang bật
  (#112).

## 1.6.0

- Cứu chữ: quên bật tiếng Việt mà đã gõ `tieengs vieetj`, bấm phím chuyển sang Ngó Sen là chữ thành
  `tiếng việt` (#71, #80, #82, #83).
- Cứu dấu: sửa dấu chữ đã gõ xong. Bôi đen chữ, gõ dấu rồi bấm Tab (`nhanh` + `s` → `nhánh`). Ở app hỗ trợ
  surrounding text, chỉ cần click vào chữ rồi gõ dấu (#118).

## 1.7.0

- Làm lại cách chèn emoji (#110).

## Sau đó

Chưa có số version, xếp theo thứ tự định làm:

1. Chữ tiếng Anh có chữ cái đôi không còn mất phím khi trả về: `affect` không thành `afect`, `coffee`
   không thành `cofee`.
2. Bớt đổi nhầm chữ tiếng Anh thành tiếng Việt, như `nest` thành `nét`, `too` thành `tô`.
3. Đoán dấu: gõ cả câu không dấu rồi bấm một phím để thêm dấu.
4. Bôi đen một đoạn rồi thêm dấu, bỏ dấu, hoặc chuyển chữ mã cũ (TCVN3, VNI) sang Unicode.
5. Sửa lỗi gõ vội, như `tiếgn` thành `tiếng`.
6. Tự chạy "Kiểm tra máy tôi" lần đầu mở cửa sổ cài đặt. Có thêm bảng các app gõ được, lấy từ kết quả test
   người dùng gửi về.
7. Đưa Ngó Sen vào kho phần mềm của các distro: nixpkgs và AUR trước, rồi COPR, PPA, OBS, sau cùng là
   Debian.
