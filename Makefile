CC = gcc -std=c99
FLAGS = -Werror -Wall -Wextra -Wpedantic

BUILD_DIR = build
BIN = markupdown

OUT = $(BUILD_DIR)/$(BIN)

.Phony: build
build: markupdown.c
	mkdir -p build
	$(CC) $(FLAGS) -o $(OUT) $^

.Phony: clean
clean:
	rm -r build

.Phony: install
install: $(OUT)
	cp $(OUT) $(HOME)/.local/bin

.Phony: uninstall
	rm $HOME/.local/bin/$(BIN)


