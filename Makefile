ENV ?= nucleo_l432kc
PIO ?= pio
PORT ?= /dev/ttyACM0
BAUD ?= 9600
ELF := .pio/build/$(ENV)/firmware.elf

OCD_IFACE ?= interface/stlink.cfg
OCD_TARGET ?= target/stm32l4x.cfg

GDB ?= gdb

.PHONY: help info build upload clean monitor compiledb \
        debug-server gdb debug reset halt flash

help:
	@echo "Targets:"
	@echo "  make info         - show environment + connected devices"
	@echo "  make build        - compile (PlatformIO)"
	@echo "  make upload       - flash via stlink (PlatformIO)"
	@echo "  make clean        - clean build"
	@echo "  make monitor      - serial monitor (PORT=$(PORT) BAUD=$(BAUD))"
	@echo "  make compiledb    - generate compile_commands.json for clangd"
	@echo ""
	@echo "Debug (OpenOCD + GDB):"
	@echo "  make debug-server - start OpenOCD (ST-LINK) on :3333"
	@echo "  make gdb          - start arm-none-eabi-gdb and attach to OpenOCD"
	@echo "  make debug        - alias for debug-server"
	@echo ""
	@echo "Vars:"
	@echo "  ENV=nucleo_l432kc  PORT=/dev/ttyACM0  BAUD=115200"
	@echo ""
	@echo "Examples:"
	@echo "  make upload"
	@echo "  make monitor PORT=/dev/ttyACM1"
	@echo "  make compiledb"
	@echo "  make debug-server   (in one terminal)"
	@echo "  make gdb            (in another terminal)"

info:
	@echo "ENV=$(ENV)"
	@$(PIO) --version
	@echo
	@echo "PlatformIO environments:"
	@$(PIO) project config --json-output | head -n 40 || true
	@echo
	@echo "Devices:"
	@$(PIO) device list || true

build:
	$(PIO) run -e $(ENV)

upload:
	$(PIO) run -e $(ENV) -t upload

clean:
	$(PIO) run -e $(ENV) -t clean

monitor:
	$(PIO) device monitor --port $(PORT) --baud $(BAUD)

# Generate compile_commands.json for clangd
compiledb:
	$(PIO) run -e $(ENV) -t compiledb
	@if [ -f ".pio/build/$(ENV)/compile_commands.json" ]; then \
		ln -sf ".pio/build/$(ENV)/compile_commands.json" "./compile_commands.json"; \
		echo "Linked ./compile_commands.json -> .pio/build/$(ENV)/compile_commands.json"; \
	else \
		echo "WARN: compile_commands.json not found under .pio/build/$(ENV)/"; \
		echo "Check where PlatformIO wrote it and symlink it to project root."; \
	fi

# ---------------------------
# Debug: OpenOCD + GDB
# ---------------------------

# Start OpenOCD debug server (keep running)
debug-server:
	openocd -f $(OCD_IFACE) -c "transport select hla_swd" -f $(OCD_TARGET)

debug: debug-server

# Attach GDB to OpenOCD and load firmware

gdb: $(ELF)
	@# Fail fast if OpenOCD isn't listening
	@nc -z 127.0.0.1 3333 || (echo "ERROR: OpenOCD not running on :3333. Run: make debug-server"; exit 2)
	$(GDB) -q $(ELF) \
		-ex "set architecture arm" \
		-ex "set confirm off" \
		-ex "target extended-remote :3333" \
		-ex "monitor reset halt" \
		-ex "load" \
		-ex "monitor reset init" \
		-ex "set confirm on"

# Convenience actions (require OpenOCD running)
reset:
	@echo "resetting via OpenOCD..."
	@echo "reset run; shutdown" | nc -N 127.0.0.1 4444 || true

halt:
	@echo "halting via OpenOCD..."
	@echo "halt; shutdown" | nc -N 127.0.0.1 4444 || true

flash: upload

