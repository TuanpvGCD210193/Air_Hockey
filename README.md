# 🏒 Air Hockey Multiplayer C++ (Unreal Engine 5)

A high-performance, physics-driven, multiplayer **Air Hockey game engine** built from scratch in C++ for Unreal Engine 5. Featuring zero-latency local mouse tracking, server-authoritative physics, 120Hz+ uncapped RPC position streaming, Server Direct RPC Relay, Client-Side Dead Reckoning, Cubic Hermite Spline interpolation, and a native C++ replicated HUD.

---

## ✨ Key Features & Technical Highlights

### 🎯 1. Zero-Latency Local Mouse LineTrace & Dynamic Clamping (Step 1)
- **3D Raycast Deprojection**: Binds mouse cursor coordinates directly to the 3D table surface plane ($Z = 35.0f$) via `GetHitResultUnderCursorByChannel` for instantaneous 1:1 control.
- **Dynamic 2D Boundary Formula**: Mathematically restricts Player 1 ($X \in [-940, -60]$, $Y \in [-440, +440]$) and Player 2 ($X \in [+60, +940]$, $Y \in [-440, +440]$) based on table dimensions and paddle radius.
- **Out-of-Bounds Cursor Holding**: Freezes paddle position at `LastValidTargetInput` when the mouse leaves the game viewport window (0% origin teleporting), smoothly gliding back to the cursor upon re-entry.

### 🌐 2. Adaptive Network Tick Rate & Curvature Scaling (Step 7)
- **Dynamic Curvature Detection**: Measures local mouse angular speed $\text{AngularSpeed} = \frac{\arccos(\hat{V}_{old} \cdot \hat{V}_{new})}{\Delta t}$ in `AAirHockeyPaddle::Tick()`.
- **Bandwidth Optimization**: Automatically scales RPC send frequency dynamically based on angular acceleration.

### ⚡ 3. Uncapped Mouse Frame-Rate Streaming (120Hz / 144Hz / 240Hz) (Step 11)
- **Unthrottled High-DPI Input**: Removes all packet send delays on the client. Mouse movements are streamed on EVERY single frame tick at the monitor's native refresh rate (120Hz / 144Hz / 240Hz).

### 🔀 4. Server Direct RPC Relay (Unthrottled Forwarding) (Step 10)
- **Bypassing Engine Replication Bottlenecks**: Bypasses Unreal Engine's default `NetDriver` property replication throttling (~20Hz).
- **0ms Direct Relay**: The Server immediately relays incoming movement RPCs directly to the opponent's possessed paddle actor using `Client_ReceiveOpponentPaddlePosition`, delivering 100% of mouse samples to Client 2 in real-time.

### 🌀 5. Client-Side Snapshot Buffer & Cubic Hermite Spline Interpolation (Step 8)
- **Snapshot Ring Buffer (`SnapshotBuffer`)**: Maintains a 5-element sliding window of recent position and velocity samples ($P_n, V_n, T_n$) on non-controlled proxy actors.
- **Cubic Hermite Spline Curve (`FMath::CubicInterp`)**: Interpolates smooth cubic curves between snapshots using real-time velocity tangents, eliminating sharp diamond/rhombus angular artifacts.

### 🛑 6. Jitter Buffering & Initial Playback Delay (Step 9)
- **Spike Detection**: Automatically detects sudden fast mouse swings ($V > 500\text{ cm/s}$) when snapshot count is low.
- **Initial Playback Delay**: Holds rendering for a $35\text{ms}$ buffer window to accumulate 3 fresh snapshots before releasing smooth Hermite Spline playback.

### 🔮 7. Client-Side Dead Reckoning & Velocity Prediction (Step 12)
- **Zero-Stutter Extrapolation**: Extrapolates future opponent paddle positions using recent velocity vectors: $Pos_{predicted} = Pos_{last} + \vec{V} \times \Delta t$.
- **Seamless Blending**: Blends predicted trajectory with incoming network snapshots, ensuring the opponent paddle glides continuously forward without micro-stuttering even during network jitter.

### 💥 8. Server-Authoritative Puck Physics & Anti-Tunneling (Step 3)
- **Active Swing Impact**: Calculates real-time 2D mouse swing velocity $\vec{V}_{paddle}$ and transfers momentum to the Puck: $\vec{V}_{puck} = \vec{N}_{hit} \times 250 + \vec{V}_{paddle} \times 0.85$.
- **Stationary Bumper Bounce**: Evaluated on the Server inside `UpdatePuckPhysics` every frame. Puck bounces off stationary/standing paddles with 50% energy reduction, permanently solving puck pass-through/tunneling bugs.

### ⚽ 9. Goal Scoring, Service Resets & Champion Rule (10 Points) (Step 4)
- **Goal Line Triggers**: Triggers goal scoring when Puck crosses Left Goal ($X < -1000, |Y| \le 150$) or Right Goal ($X > 1000, |Y| \le 150$).
- **Service Reset**: Resets Puck to table center $(0, 0, 35)$ and launches a soft service towards the victim player.
- **Match Victory**: First player to reach 10 points triggers `bIsGameOver = true`, freezing movement and declaring the Champion.

### 🖥️ 10. Replicated Native C++ Canvas HUD (Step 5)
- **Zero-Latency Rendering**: Subclasses native `AHUD` (`AAirHockeyHUD`) to render glassmorphism header scoreboards directly onto the 2D viewport canvas.
- **Real-Time Replication**: Binds directly to `AAirHockeyGameState` replicated properties (`Player1Score`, `Player2Score`, `bIsGameOver`, `WinningPlayerId`) to update scores simultaneously across all clients.

---

## 📁 Codebase Architecture

```text
Source/Air_Hockey/
├── Public/
│   ├── Core/
│   │   ├── AirHockeyGameMode.h        # Server game rules, spawning & service resets
│   │   └── AirHockeyGameState.h       # Replicated scores, match state & win conditions
│   ├── Paddle/
│   │   └── AirHockeyPaddle.h          # Pawn class with LineTrace, bounds clamping, Dead Reckoning & RPC Relays
│   ├── Puck/
│   │   └── AirHockeyPuck.h            # Actor class with server physics & wall/paddle bounce
│   ├── Player/
│   │   └── AirHockeyPlayerController.h # Top-down camera binding & mouse input setup
│   ├── UI/
│   │   └── AirHockeyHUD.h             # Native C++ Canvas HUD for scoreboards & victory overlay
│   └── Types/
│       └── AirHockeyNetTypes.h        # Network RPC structs (FPaddleMove, FPaddleState, FPuckState)
└── Private/
    ├── Core/
    │   ├── AirHockeyGameMode.cpp
    │   └── AirHockeyGameState.cpp
    ├── Paddle/
    │   └── AirHockeyPaddle.cpp
    ├── Puck/
    │   └── AirHockeyPuck.cpp
    ├── Player/
    │   └── AirHockeyPlayerController.cpp
    └── UI/
        └── AirHockeyHUD.cpp
```

---

## 🚀 How to Build & Run

### Prerequisites
- **Unreal Engine 5.4+** installed.
- **Visual Studio 2022** with C++ Game Development Workload.

### Build Steps
1. Clone the repository:
   ```bash
   git clone https://github.com/TuanpvGCD210193/Air_Hockey.git
   ```
2. Right-click `Air_Hockey.uproject` $\rightarrow$ Select **Generate Visual Studio project files**.
3. Open `Air_Hockey.sln` in Visual Studio and press **`Ctrl + Shift + B`** to Build Solution.
4. Launch Unreal Editor.
5. In Unreal Editor, set Play mode to **Play in Editor (PIE)** with **2 Players**.
6. Enjoy the fast-paced, silky-smooth multiplayer Air Hockey match! 🏒

---

## Emperor Approved
<p align="center">
  <img
    width="588"
    height="425"
    alt="Emperor Approved"
    src="https://github.com/user-attachments/assets/e24f0939-49d9-4d0e-8191-4348c73bec94"
  />
</p>

---

## 📜 License
This project is licensed under the WTFPL License - see the [LICENSE](https://github.com/TuanpvGCD210193/Air_Hockey/blob/main/LICENSE) file for details.
