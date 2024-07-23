BUILD_TARGET := .make_scop
BUILD_DIR := ./build

.PHONY: all clean fclean re

all: $(BUILD_DIR)/$(BUILD_TARGET)

clean:
	rm -f $(BUILD_DIR)/$(BUILD_TARGET)
	rm -f scop

fclean: clean
	rm -rf $(BUILD_DIR)

re: fclean all

$(BUILD_DIR)/$(BUILD_TARGET):
	git submodule update --init --recursive
	mkdir -p $(BUILD_DIR)
	cmake -S . -B $(BUILD_DIR) -G "Unix Makefiles"
	make -C $(BUILD_DIR)
	mv $(BUILD_DIR)/scop ./scop
	touch $(BUILD_DIR)/$(BUILD_TARGET)
