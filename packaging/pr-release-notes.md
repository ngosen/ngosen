Build tự động từ pull request [#@PR@](https://github.com/ngosen/ngosen/pull/@PR@), commit @SHA@. Đây là version để test bản sửa trước khi gộp, có thể còn lỗi. Những gì thay đổi nằm trong mô tả của pull request.

Cài hoặc cập nhật version thử:

```
curl -fsSL https://raw.githubusercontent.com/ngosen/ngosen/next/install.sh | NGOSEN_RELEASE_URL=https://github.com/ngosen/ngosen/releases/download/@TAG@ bash
```

Số version có thêm ngày giờ và mã commit, ví dụ `1.2.2-1.202610121530.git8cb4b89`. Khi version chính thức tiếp theo ra, chạy lệnh cài thường trong README là máy lên version đó.

Gặp lỗi thì trả lời trong issue đã báo hoặc trong pull request trên, ghi kèm số version.
