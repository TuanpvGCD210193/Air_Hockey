# 🏑 Unreal Engine 5 - Air Hockey Online Multiplayer

Một tựa game **Air Hockey 2 người chơi Online** được phát triển bằng **Unreal Engine 5 (C++)**, tập trung vào việc tự xây dựng **hệ thống giả lập vật lý tùy chỉnh (Custom Physics)** không phụ thuộc vào hệ thống Collision mặc định của Engine, kết hợp với kỹ thuật xử lý mạng tối ưu **Fast-Paced Multiplayer (Client-Side Prediction & Server Reconciliation)** để đảm bảo trải nghiệm chơi mượt mà trong điều kiện mạng lag và mất gói tin (Packet Loss).

---

## 📌 Tổng Quan Dự Án & Mục Tiêu Học Tập

### 🎯 1. Các Kỹ Thuật Đã Học & Áp Dụng (Key Learnings & Applications)
* **Tự lập trình Vật lý bằng Vector Math (Custom Trace Physics)**:
  * Không sử dụng hệ thống Collision mặc định của Unreal (PhysX/Chaos).
  * Xử lý va chạm hoàn toàn bằng **Sphere Trace / Line Trace** (`GetWorld()->SweepSingleByChannel`).
  * Ứng dụng **Toán học Vector & Vật lý học**:
    * **Phản xạ tường (Reflection Vector)**: Áp dụng công thức phản xạ vector $\vec{V}_{out} = \vec{V}_{in} - 2(\vec{V}_{in} \cdot \vec{N})\vec{N}$ giữ nguyên độ lớn vận tốc.
    * **Va chạm Tay cầm (Paddle Impact)**: Bóng suy giảm 80% động năng ban đầu ($V_{reduced} = V_{initial} \times \sqrt{0.2} \approx 0.447 \times V_{initial}$) và nhận thêm vector lực đẩy từ tay cầm.
    * **Nội suy vận tốc (Trajectory Smoothing)**: Đánh mượt đường đi của tay cầm chuột qua thuật toán nội suy mũ `FMath::VInterpTo` để khử hiện tượng rung/giật con trỏ chuột.

* **Kiến trúc Mạng Fast-Paced Multiplayer (Client-Server Network Architecture)**:
  * Dựa trên tài liệu nghiên cứu chuẩn mực của **Gabriel Gambetta**:
    1. **Client-Side Prediction (CSP)**: Cho phép Local Client phản hồi di chuyển tay cầm ngay tức thì mà không cần chờ Server xác nhận.
    2. **Server Reconciliation**: Lưu lịch sử `UnacknowledgedMoves` trên Client; khi Server gửi vị trí chuẩn (`ServerState`), Client sẽ snap vị trí và **re-simulate** (chạy lại) các input chưa được xác nhận để triệt tiêu độ lệch (drift).
    3. **Snapshot / Entity Interpolation**: Áp dụng nội suy vị trí cho trái bóng Puck trên màn hình Client đối phương để chuyển động lướt mượt mà, không bị hiện tượng khựng/teleport.
    4. **Network Emulation**: Thử nghiệm và tối ưu hóa game dưới điều kiện mạng thực tế (Ping $150\text{ms} - 200\text{ms}$, Packet Loss $3\% - 5\%$).

---

## 🛠️ Chi Tiết Lộ Trình Phát Triển (Detailed Roadmap & Phases)

### 🔹 Phase 1: Core Physics & Custom Sweep Collision
* **Step 1.1: Khóa độ cao mặt bàn & Kẹp biên sân đấu**:
  * Giới hạn chuyển động của Paddle trên mặt phẳng $XY$ cố định độ cao $Z$.
  * Kẹp biên di chuyển: Player 1 thuộc nửa bàn trái ($X \le 0$), Player 2 thuộc nửa bàn phải ($X \ge 0$).
* **Step 1.2: Tối ưu hóa nội suy vận tốc (Velocity Smoothing)**:
  * Khử giật chuột qua thuật toán `FMath::VInterpTo` với `SmoothingSpeed = 25.0f`.
  * Giới hạn vận tốc tối đa `MaxSpeed = 1500.0f`.
* **Step 1.3: Lập trình công thức vector nảy tường cho Puck**:
  * Sử dụng `SweepSingleByChannel` (bán kính $R = 25.0f$).
  * Phản xạ theo góc tới = góc nảy $\vec{V}_{out} = \vec{V}_{in} - 2(\vec{V}_{in} \cdot \vec{N})\vec{N}$, vận tốc tuyệt đối không đổi.
* **Step 1.4: Lập trình va chạm giữa Puck và Paddle**:
  * Mất 80% động năng ban đầu khi chạm tay cầm ($V_{reduced} = V_{initial} \times \sqrt{0.2}$).
  * Cộng thêm vận tốc nảy ra từ tay cầm nếu vận tốc tay cầm đủ lớn ($\vec{V}_{paddle} \times 0.8$).
* **Step 1.5: Phát hiện Puck vượt vạch khung thành**:
  * Kiểm tra tọa độ Puck vượt vạch khung thành 2 bên bàn để gọi sự kiện `OnGoalScored(ScoringPlayerId)`.

---

### 🔹 Phase 2: Game Rules, Scoring & Spawning
* **Step 2.1: Gán quyền sở hữu Paddle khi 2 người chơi join game**:
  * Auto-spawn Paddle 1 ở nửa bàn trái cho Host (Player 1) và Paddle 2 ở nửa bàn phải cho Client (Player 2) trong `PostLogin`.
* **Step 2.2: Reset vị trí Puck và phát bóng sau mỗi bàn thắng**:
  * Đặt lại Puck về giữa sân $(0, 0, Z_{table})$ và phát bóng lại theo hướng ngẫu nhiên/hướng người vừa bị thủng lưới.
* **Step 2.3: Kiểm tra điều kiện 10 điểm thắng & Score Replication**:
  * Người chơi nào đạt 10 điểm trước sẽ thắng cuộc (`MaxScoreToWin = 10`).
  * Replicate `Player1Score`, `Player2Score`, `WinningPlayerId`, `bIsGameOver` về tất cả Client.

---

### 🔹 Phase 3: Fast-Paced Network Mechanics (CSP, Reconciliation & Interpolation)
* **Step 3.1: Client-Side Input Prediction**:
  * Đóng gói input `FPaddleMove` kèm `SequenceNumber` tăng dần.
  * Cập nhật vị trí dự đoán tức thì cho Paddle trên Local Client và lưu move vào mảng `UnacknowledgedMoves`.
* **Step 3.2: Server RPC Validation & State Acknowledgment**:
  * Gửi RPC `Server_SendMove`. Server chạy `PerformSweepMove` và gửi trạng thái `ServerState` (`Position`, `Velocity`, `LastProcessedSequenceNumber`) về cho Client.
* **Step 3.3: Thuật toán Client Server Reconciliation**:
  * Khi nhận `ServerState`, Client loại bỏ các move có `SequenceNumber <= LastProcessedSequenceNumber`.
  * Snap Paddle về vị trí Authoritative của Server, sau đó **re-simulate** (chạy lại) tất cả các move chưa được Server xác nhận.
* **Step 3.4: Snapshot Interpolation cho Puck**:
  * Ở màn hình Remote Client, áp dụng nội suy mượt vị trí trái bóng Puck qua `OnRep_PuckState` giúp bóng di chuyển mượt mà không bị khựng giật.

---

### 🔹 Phase 4: Network Emulation & Verification
* **Step 4.1**: Bật giả lập mạng trong Unreal Editor: Latency $150\text{ms}$, Packet Loss $3\%$.
* **Step 4.2**: Kiểm tra di chuyển chuột của Paddle trên Local Client — đảm bảo không lag, không bị rubberbanding.
* **Step 4.3**: Kiểm tra phản xạ nảy bóng của Puck và tính điểm chuẩn xác 10 bàn thắng giữa Server và Client.

---

## 🚀 Hướng Dẫn Chạy Game Trong Unreal Engine Editor

1. **Yêu cầu hệ thống**:
   * Unreal Engine 5.0 trở lên.
   * Visual Studio 2022 (C++ Game Development Workload).
2. **Biên dịch & Khởi động**:
   * Chuột phải vào file `Air_Hockey.uproject` -> chọn **Generate Visual Studio project files**.
   * Mở file `Air_Hockey.sln` trong Visual Studio và nhấn **Build Solution** (`Ctrl + Shift + B`).
   * Mở Unreal Editor.
3. **Chạy thử nghiệm Network Emulation**:
   * Trong cửa sổ Unreal Editor, nhấp vào dấu 3 chấm `...` bên cạnh nút **Play**.
   * Đặt **Number of Players** thành `2`.
   * Chọn **Net Mode** thành `Play As Listen Server`.
   * (Tùy chọn) Bật **Network Emulation** trong Editor Preferences để test dưới điều kiện mạng lag $150\text{ms}$.

---
*Dự án được xây dựng với mục đích học tập và làm chủ C++ Network Programming & Custom Physics Simulation trong Unreal Engine.* 🏒🔥
