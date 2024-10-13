const std = @import("std");

const vk = @import("vulkan");
const glfw = @import("mach-glfw");

const vk_log = std.log.scoped(.vk);
const glfw_log = std.log.scoped(.glfw);

const apis: []const vk.ApiInfo = &.{
    vk.features.version_1_0,
    //vk.features.version_1_1,
    //vk.features.version_1_2,
};
const BaseDispatch = vk.BaseWrapper(apis);
//const InstanceDispatch = vk.InstanceWrapper(apis);
//const DeviceDispatch = vk.DeviceWrapper(apis);

fn logGLFWError(error_code: glfw.ErrorCode, description: [:0]const u8) void {
    glfw_log.err("{}: {s}\n", .{ error_code, description });
}

pub fn main() !void {
    glfw.setErrorCallback(logGLFWError);

    if (!glfw.init(.{})) {
        glfw_log.err("failed to initialize GLFW: {?s}", .{glfw.getErrorString()});
        return error.GLFWInitFailed;
    }
    defer glfw.terminate();

    const window = glfw.Window.create(640, 480, "scop", null, null, .{
        .client_api = .no_api,
        .resizable = true,
    }) orelse {
        glfw_log.err("failed to create GLFW window: {?s}", .{glfw.getErrorString()});
        return error.CreateWindowFailed;
    };
    defer window.destroy();

    // This line fails compilation
    _ = try BaseDispatch.load(@as(vk.PfnGetInstanceProcAddr, @ptrCast(&glfw.getInstanceProcAddress)));

    while (!window.shouldClose()) {
        glfw.pollEvents();
    }
}
