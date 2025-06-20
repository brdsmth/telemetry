APP_NAME := ingester
BUILD_DIR := build

.PHONY: all run build clean

all: build

run:
	@echo "🔧 Running $(APP_NAME)..."
	go run main.go

build:
	@echo "📦 Building $(APP_NAME)..."
	@mkdir -p $(BUILD_DIR)
	GOOS=linux GOARCH=amd64 go build -o $(BUILD_DIR)/$(APP_NAME) main.go

clean:
	@echo "🧹 Cleaning build artifacts..."
	@rm -rf $(BUILD_DIR)
