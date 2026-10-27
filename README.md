# Barrier Synchronization

Hiện thực sense-reversing barrier, bản có backoff và `std::barrier`. Cần CMake từ 3.20 và compiler hỗ trợ C++20. Các lệnh dưới đây dùng trong WSL/Linux.

## 1. Build

```bash
cmake -B build -S .
cmake --build build -j
```

## 2. Kiểm thử

```bash
./build/bin/test_barriers
```

## 3. Chạy benchmark

```bash
./build/bin/bench_barriers
```

Chương trình chạy cả 3 phiên bản với 4 luồng, 200000 pha và in thời gian, thông lượng (pha/giây) ngay trên terminal.

Để lấy số liệu báo cáo, chạy 5 lần cho mỗi số luồng:

```bash
for run in {1..5}; do
    echo "=== Run $run ==="
    ./build/bin/bench_barriers --threads 1  --episodes 20000000
    ./build/bin/bench_barriers --threads 2  --episodes 2000000
    ./build/bin/bench_barriers --threads 4  --episodes 200000
    ./build/bin/bench_barriers --threads 8  --episodes 200000
    ./build/bin/bench_barriers --threads 16 --episodes 20000
done
```

Bổ sung mốc số lõi vật lý và luồng logic của máy nếu khác các mốc trên. Gửi lại output kèm CPU, RAM, hệ điều hành, compiler và chế độ nguồn. Cắm nguồn và đóng chương trình nặng khi đo; nếu mẫu quá ngắn thì tăng `--episodes` cho cả 3 phiên bản.

Nếu thư mục `build` đã tạo trên Windows, dùng thư mục build mới cho WSL.
