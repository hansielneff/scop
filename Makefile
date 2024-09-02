BUILD_TARGET := .make_scop
BUILD_DIR := ./build

.PHONY: all clean fclean re shaders

all: $(BUILD_DIR)/$(BUILD_TARGET)

clean:
	rm -f $(BUILD_DIR)/$(BUILD_TARGET)
	rm -f scop

fclean: clean
	rm -rf $(BUILD_DIR)

re: fclean all

shaders:
	mkdir -p $(BUILD_DIR)/shaders/
	glslc shaders/shader.vert -o $(BUILD_DIR)/shaders/vert.spv
	glslc shaders/shader.frag -o $(BUILD_DIR)/shaders/frag.spv

$(BUILD_DIR)/$(BUILD_TARGET):
	git submodule update --init --recursive
	mkdir -p $(BUILD_DIR)
	cmake -S . -B $(BUILD_DIR) -G "Unix Makefiles"
	make -C $(BUILD_DIR)
	mv $(BUILD_DIR)/scop ./scop
	touch $(BUILD_DIR)/$(BUILD_TARGET)
