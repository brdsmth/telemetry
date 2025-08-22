.PHONY: build upload monitor clean erase uploadfs all

# Load environment variables if .env exists
ifneq (,$(wildcard .env))
    include .env
    export
endif

build:
	pio run

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