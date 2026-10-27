# Barrier Synchronization — Trọng Tín

Phần 2 của bài tập lớn CO5133. Repo này chạy độc lập, gồm sense-reversing barrier, phiên bản có exponential backoff và `std::barrier` để so sánh. Source C++ và các kịch bản kiểm thử được tách từ bài chung, giữ nguyên cách hoạt động và cách đo trong báo cáo.

Khôi và Trí có thể tải ZIP hoặc clone repo, build trên máy mình rồi chạy script bên dưới để lấy số liệu. Không cần repo locks, counter, MPI hay thư viện benchmark bên ngoài.

## 1. Chuẩn bị và build

Cần CMake từ 3.20, compiler **và thư viện chuẩn** hỗ trợ C++20 (`std::barrier`, `std::latch`), cùng Python 3.9 trở lên để chạy script thu số liệu. Phần C++ không phụ thuộc Python. CMake sẽ kiểm tra khả năng hỗ trợ barrier ngay khi cấu hình.

Mở terminal tại thư mục chứa `CMakeLists.txt`:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

Trên Windows, dùng Developer PowerShell của Visual Studio hoặc terminal đã có compiler MinGW và CMake trong `PATH`. Với MinGW, thêm `-G "MinGW Makefiles"` ở lệnh cấu hình đầu tiên. Không dùng lại cùng thư mục build khi đổi compiler hoặc generator.

Trên macOS/Linux, dùng compiler C++20 và thư viện chuẩn tương ứng. Nếu CMake báo thiếu `std::barrier`/`std::latch`, cần đổi hoặc cập nhật toolchain rồi cấu hình trong thư mục build mới. Chỉ bật `-std=c++20` chưa đủ khi thư viện chuẩn chưa hỗ trợ.

Bộ kiểm tra có **8 test CTest**: một test chạy 25 trường hợp kiểm tra barrier, cùng bảy test từ chối tham số benchmark sai. Kiểm tra dữ liệu theo pha, luồng đến muộn, hai đối tượng barrier, tái sử dụng nhiều lần và số người tham gia bằng 0 vẫn hoạt động trong Release.

## 2. Lấy số liệu trên máy của mỗi bạn

Cắm nguồn nếu dùng laptop, chọn và ghi lại chế độ nguồn, đóng chương trình nặng; chạy lần lượt từng benchmark. Xem số **lõi vật lý** trong Task Manager (Windows), System Information (macOS) hoặc `lscpu` (Linux). Không lấy số luồng logic làm số lõi vật lý.

Ví dụ máy có **8 lõi vật lý**; thay `8`, tên máy và mô tả nguồn bằng thông tin thực tế:

**Windows (PowerShell):**

```powershell
python scripts/run_benchmarks.py --machine tri-windows --physical-cores 8 --power-mode "AC, Balanced"
```

**macOS/Linux:**

```sh
python3 scripts/run_benchmarks.py --machine khoi-mac --physical-cores 8 --power-mode "AC, Low Power Mode off"
```

Nếu Windows chỉ có lệnh `py`, thay `python` bằng `py -3`. CMake và CTest cần nằm trong `PATH` của terminal chạy Python.

Script tự chạy lại CTest trước khi đo. Mỗi cấu hình chạy đủ ba phiên bản, dùng cùng số pha và lặp **5 lần**. Các mốc mặc định là `1, 2, 4, 8, 16`, bổ sung số lõi vật lý và số luồng logic của máy, bỏ mốc trùng. Có thể vượt số luồng phần cứng ở máy nhỏ; đây là trường hợp oversubscription cần ghi nhận khi phân tích.

Số pha mặc định theo cách chọn trong báo cáo:

| Số luồng N | Số pha E mỗi lần đo |
| --- | ---: |
| 1 | 20 000 000 |
| 2 | 2 000 000 |
| 3–15 | 200 000 |
| Từ 16 | 20 000 |

Mỗi lần có 1000 pha warm-up nằm ngoài phần đo. Thông lượng là **R(N) = E/t**, đơn vị pha/giây; một pha tính một lần cho toàn bộ N luồng. Khoảng đo bắt đầu ở completion của barrier xuất phát và dừng khi luồng chính nhận latch hoàn tất. Tạo luồng, warm-up và `join()` nằm ngoài khoảng này. Các worker không làm công việc hữu ích giữa các barrier.

Nếu script cảnh báo mẫu dưới 0.1 giây, tăng E cho cấu hình đó và chạy lại. Số pha phải giống nhau giữa ba phiên bản tại cùng N; không chọn riêng lần nhanh nhất. Có thể đặt cả dãy N và E theo thứ tự tương ứng, chẳng hạn:

```sh
python3 scripts/run_benchmarks.py --machine khoi-mac --physical-cores 8 --power-mode "AC, Low Power Mode off" --threads 1 2 4 8 16 --episodes 40000000 4000000 400000 400000 40000
```

Windows dùng `python` thay `python3`. Thêm `--build-dir build-other` nếu dùng thư mục build khác. Mỗi lệnh benchmark được giới hạn 300 giây; nếu máy chậm, giảm E hoặc tăng `--timeout`. Script dừng và giữ log nếu test, benchmark hoặc việc đọc kết quả lỗi.

## 3. Gửi lại những file nào?

Sau khi chạy xong, script in đường dẫn dạng `results/<tên-máy>-<thời-gian>/`. **Nén và gửi nguyên thư mục đó cho Tín**, gồm:

| File | Nội dung |
| --- | --- |
| `correctness-release.log` | Kết quả kiểm thử trước khi đo |
| `benchmark-runs.txt` | Lệnh chạy và output gốc của từng lần |
| `benchmark-runs.csv` | Từng số đo, gồm N, E, thời gian và pha/giây |
| `benchmark-summary.csv` | Median, min, max và số mẫu theo từng cấu hình |
| `environment.json` | OS, số lõi/luồng, compiler, cờ Release, nguồn điện, kế hoạch chạy, SHA-256 source và executable |
| `hardware.txt` | Thông tin phần cứng lấy từ hệ điều hành |

Kiểm tra `status` trong `environment.json` là **`complete`**. Lần chạy thất bại có trạng thái `failed`, giữ lại để xem lỗi; không dùng số liệu dở dang trong bảng kết quả. Với 6 mốc N và 5 lần lặp, số đo đầy đủ là `6 × 5 × 3 = 90` dòng dữ liệu.

Xem lại thông tin CPU, RAM và chế độ nguồn đã đúng chưa; bổ sung thủ công nếu hệ điều hành không trả đủ. Không sửa source trong lúc thu số liệu. Mỗi lần chạy tạo thư mục mới nên không ghi đè kết quả trước. `results/` không được đưa vào Git mặc định.

## 4. Chạy riêng và kiểm tra nhanh

Trên macOS/Linux hoặc Windows MinGW:

```sh
./build/bin/bench_barriers --help
./build/bin/bench_barriers --threads 4 --episodes 200000 --barrier all
```

Windows dùng đuôi `.exe`. Với Visual Studio, executable ở `build/bin/Release/`; script tự tìm cả hai kiểu đường dẫn.

Có thể chạy thử đường đi từ test đến xuất CSV bằng lệnh dưới; **đây chỉ là kiểm tra nhanh**, không dùng thay bộ đo 5 lần:

```sh
python3 scripts/run_benchmarks.py --machine smoke --physical-cores 8 --power-mode "AC, smoke test" --threads 1 2 --episodes 100000 1000 --repeats 1
```

Kiểm tra riêng phần xử lý số liệu Python:

```sh
python3 -m unittest discover -s scripts -p "test_*.py"
```

## 5. Cấu trúc

```text
include/parcomp/barriers/    Interface, sense-reversing và std::barrier wrapper
include/parcomp/common/      Exponential backoff dùng cho barrier
tests/test_barriers.cpp      Kiểm tra tính đúng
benchmarks/bench_barriers.cpp  Đo pha/giây
scripts/run_benchmarks.py    Chạy test, thu số liệu và tổng hợp
```

Barrier yêu cầu một nhóm cố định N > 0, mỗi luồng gọi đúng một lần trong mỗi pha. Không thay đổi số người tham gia giữa chừng. Kết quả kiểm thử là bằng chứng cho các kịch bản đã chạy, không thay cho lập luận tính đúng trên mọi lịch thực thi.
