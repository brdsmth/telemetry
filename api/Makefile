APP_NAME := api
BUILD_DIR := build

.PHONY: all run build clean test vet

all: build

run:
	@echo "🔧 Running $(APP_NAME)..."
	go run ./cmd/api

build:
	@echo "📦 Building $(APP_NAME)..."
	@mkdir -p $(BUILD_DIR)
	go build -o $(BUILD_DIR)/$(APP_NAME) ./cmd/api

test:
	go test ./...

vet:
	go vet ./...

clean:
	@echo "🧹 Cleaning build artifacts..."
	@rm -rf $(BUILD_DIR)

setup:
	@echo "🔧 Setting up $(APP_NAME)..."
	go run setup/setup.go