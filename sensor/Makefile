.PHONY: build upload monitor clean erase uploadfs test all

# Load environment variables if .env exists
ifneq (,$(wildcard .env))
    include .env
    export
endif

build:
	pio run -e esp32dev

test:
	pio test -e native

upload:
	pio run --target upload

uploadfs: 
	pio run --target uploadfs

monitor:
	pio device monitor

clean:
	pio run --target clean

erase: 
	pio run --target erase

all: build upload monitor 