

CXX      = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -Wpedantic -DNDEBUG
DBGFLAGS = -std=c++17 -g -O0 -Wall -Wextra -Wpedantic -fsanitize=address,undefined
LDFLAGS  =

SRC_DIR   = src
TEST_DIR  = tests
BUILD_DIR = build

# Source files
SRCS     = $(wildcard $(SRC_DIR)/*.cpp)
OBJS     = $(SRCS:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
LIB_OBJS = $(filter-out $(BUILD_DIR)/main.o, $(OBJS))

# Test files
TEST_SRCS = $(wildcard $(TEST_DIR)/*.cpp)
TEST_OBJS = $(TEST_SRCS:$(TEST_DIR)/%.cpp=$(BUILD_DIR)/%.o)

TARGET      = alpha-machine
TEST_TARGET = test_runner

# ── Build Targets ────────────────────────────────────────────────────────────

.PHONY: all clean test debug release

all: release

release: CXXFLAGS := $(CXXFLAGS)
release: clean $(TARGET)
	@echo "✅  Built $(TARGET) (release)"

debug: CXXFLAGS := $(DBGFLAGS)
debug: clean $(TARGET)
	@echo "✅  Built $(TARGET) (debug)"

$(TARGET): $(OBJS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR)/%.o: $(TEST_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(DBGFLAGS) -I$(SRC_DIR) -c -o $@ $<

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# ── Tests ────────────────────────────────────────────────────────────────────

$(TEST_TARGET): $(TEST_OBJS) $(LIB_OBJS) | $(BUILD_DIR)
	$(CXX) $(DBGFLAGS) -o $@ $^ $(LDFLAGS)

test: CXXFLAGS := $(DBGFLAGS)
test: clean $(TEST_TARGET)
	@echo "🧪  Running tests..."
	@./$(TEST_TARGET)

# ── Clean ────────────────────────────────────────────────────────────────────

clean:
	rm -rf $(BUILD_DIR) $(TARGET) $(TEST_TARGET)
	@echo "🧹  Cleaned"
