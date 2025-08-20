.PHONY: build upload monitor clean

build:
	pio run

upload:
	pio run --target upload -v

monitor:
	pio device monitor

clean:
	pio run --target clean -v

all: clean build upload monitor 