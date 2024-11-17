BUILD_TARGET := scop
BUILD_DIR := ./build

.PHONY: all clean fclean re shaders

all: $(BUILD_DIR)/$(BUILD_TARGET) shaders

clean:
	rm -f $(BUILD_DIR)/$(BUILD_TARGET)

fclean: clean
	rm -rf $(BUILD_DIR)

re: fclean all

$(BUILD_DIR)/$(BUILD_TARGET): ./src/main.c ./src/obj_parser.c
	git submodule update --init --recursive
	mkdir -p $(BUILD_DIR)
	cmake -S . -B $(BUILD_DIR) -G "Unix Makefiles"
	make -C $(BUILD_DIR)

shaders: $(BUILD_DIR)/shaders/vert.spv $(BUILD_DIR)/shaders/frag.spv

$(BUILD_DIR)/shaders/vert.spv: ./shaders/shader.vert
	mkdir -p $(BUILD_DIR)/shaders/
	glslc shaders/shader.vert -o $(BUILD_DIR)/shaders/vert.spv

$(BUILD_DIR)/shaders/frag.spv: ./shaders/shader.frag
	mkdir -p $(BUILD_DIR)/shaders/
	glslc shaders/shader.frag -o $(BUILD_DIR)/shaders/frag.spv
