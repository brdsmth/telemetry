APP_NAME := api
BUILD_DIR := build

.PHONY: all run build clean
.PHONY: setup

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

setup:
	@echo "🔧 Setting up $(APP_NAME)..."
	go run setup/setup.go