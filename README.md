# 🏒 Air Hockey Multiplayer C++ (Unreal Engine 5)

A high-performance, physics-driven, multiplayer **Air Hockey game engine** built from scratch in C++ for Unreal Engine 5. Featuring zero-latency local mouse tracking, server-authoritative physics, fast unreliable RPC position streaming, anti-tunneling stationary bumper physics, and a native C++ replicated HUD.

---

## ✨ Key Features & Technical Highlights

### 🎯 1. Zero-Latency Local Mouse LineTrace & Dynamic Clamping
- **3D Raycast Deprojection**: Binds mouse cursor coordinates directly to the 3D table surface plane ($Z = 35.0f$) via `GetHitResultUnderCursorByChannel` for instantaneous 60 FPS 1:1 control.
- **Dynamic 2D Boundary Formula**: Mathematically restricts Player 1 ($X \in [-940, -60]$, $Y \in [-440, +440]$) and Player 2 ($X \in [+60, +940]$, $Y \in [-440, +440]$) based on table dimensions and paddle radius.
- **Out-of-Bounds Cursor Holding**: Freezes paddle position at `LastValidTargetInput` when the mouse leaves the game viewport window (0% origin teleporting), smoothly gliding back to the cursor upon re-entry.

### 🌐 2. Multiplayer Unreliable RPC & Opponent Interpolation
- **60 FPS Unreliable RPC Sync**: Uses `Server, Unreliable` RPCs (`Server_SendPaddlePosition`) to stream position updates without TCP packet queuing, permanently eliminating client snapbacks/jitter.
- **Silky-Smooth Remote Interpolation**: Remote opponent paddles (`!IsLocallyControlled()`) smoothly interpolate target positions using `FMath::VInterpTo(CurrentPos, ServerPos, DeltaTime, 30.0f)`.

### 💥 3. Server-Authoritative Puck Physics & Anti-Tunneling
- **Active Swing Impact**: Calculates real-time 2D mouse swing velocity $\vec{V}_{paddle}$ and transfers momentum to the Puck: $\vec{V}_{puck} = \vec{N}_{hit} \times 250 + \vec{V}_{paddle} \times 0.85$.
- **Stationary Bumper Bounce**: Evaluated on the Server inside `UpdatePuckPhysics` every frame. Puck bounces off stationary/standing paddles with 50% energy reduction, permanently solving puck pass-through/tunneling bugs.

### ⚽ 4. Goal Scoring, Service Resets & Champion Rule (10 Points)
- **Goal Line Triggers**: Triggers goal scoring when Puck crosses Left Goal ($X < -1000, |Y| \le 150$) or Right Goal ($X > 1000, |Y| \le 150$).
- **Service Reset**: Resets Puck to table center $(0, 0, 35)$ and launches a soft service towards the victim player.
- **Match Victory**: First player to reach 10 points triggers `bIsGameOver = true`, freezing movement and declaring the Champion.

### 🖥️ 5. Replicated Native C++ Canvas HUD
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
│   │   └── AirHockeyPaddle.h          # Pawn class with LineTrace, bounds clamping & RPCs
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
   git clone https://github.com/your-username/Air_Hockey.cmd
   ```
2. Right-click `Air_Hockey.uproject` $\rightarrow$ Select **Generate Visual Studio project files**.
3. Open `Air_Hockey.sln` in Visual Studio and press **`Ctrl + Shift + B`** to Build Solution.
4. Launch Unreal Editor.
5. In Unreal Editor, set Play mode to **Play in Editor (PIE)** with **2 Players**.
6. Enjoy the fast-paced multiplayer Air Hockey match! 🏒

---

## 📜 License
This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
