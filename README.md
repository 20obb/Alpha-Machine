# Alpha-Machine Chess Engine ♟️

**Alpha-Machine** is a professional chess engine being built from scratch in C++ (for core performance) and Python (for analytical tools and self-learning later). 
The engine relies on a hybrid architecture with a focus on self-learning and progressive development.

## 🚀 Where are we in development?

We have successfully completed **Milestones 1, 2, and 3** of the project.
The engine is now fully capable of understanding the board, accurately generating all legal moves, evaluating positions, and searching for the best move.

### Completed Milestones (1-3):
- [x] **Data Structure (Bitboards)**: Board representation using `uint64_t` for extreme speed.
- [x] **Magic Bitboards**: Blazing fast sliding piece attack calculation (Bishops and Rooks).
- [x] **Move Generation (MoveGen)**: Generation of all pseudo-legal and legal moves including Castling, En Passant, and Promotions.
- [x] **Zobrist Hashing**: Generating a unique 64-bit signature for every board position (ready for memory tables and repeated position detection).
- [x] **Evaluation**: Positional evaluation based on piece values (Material), their locations (Piece-Square Tables), and a Bishop Pair bonus.
- [x] **Search Engine**: Alpha-Beta pruning with Negamax, enhanced by Quiescence Search (to prevent Horizon effect blunders in tactical positions) and Iterative Deepening.
- [x] **Correctness Tests (Perft)**: Passed 22/22 rigorous move generation tests on highly complex positional test suites.
- [x] **Speed Optimization**: Reaching up to **1.6 million nodes per second (nps)** during search execution.

### Next Steps (Milestone 4):
- [ ] **Transposition Table (TT)**: Advanced hash table to store previously evaluated positions globally to astronomically speed up the search.
- [ ] **Enhanced Move Ordering**: Implementing Killer Moves and History Heuristics to improve Alpha-Beta cut-offs.
- [ ] **UCI Protocol**: Allowing the engine to connect and play matches automatically on graphical chess interfaces (Arena, Lichess, etc).
- [ ] **Time Management**: Allowing the engine to distribute its time effectively in matches.

---

## 🛠️ How to Build and Run

The project is designed to run on a Linux environment using the `g++` compiler and `make`.

### 1. Compile the Engine
Open your terminal in the project directory and execute:

```bash
# Build for Release mode (maximum speed and strength):
make release

# Build for Debug mode (for development and memory diagnostics):
make debug

# Clean output files:
make clean
```

### 2. How to Run and Test

Since the UCI protocol is not finished yet, you communicate with the engine through the command line to test its completed functionalities:

```bash
# 1- Default Run (Displays the starting position and runs a quick performance test):
./alpha-machine

# 2- Full Move Generation Tests (Run strict Perft Tests to ensure zero logic bugs):
./alpha-machine perft

# 3- Deep Search from Start Position (e.g. up to depth 8):
./alpha-machine search 8

# 4- Tactical Checkmate Tests (Run quick "Mate in N" puzzle validations):
./alpha-machine mate
```

---

*This version is currently intended for developers. In upcoming milestones (with the inclusion of UCI protocol support), you'll be able to hook this engine directly into a standard chess GUI and play full interactive games against it with a mouse and graphic board.*
