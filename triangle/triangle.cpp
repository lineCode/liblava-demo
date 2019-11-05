// file      : triangle/triangle.cpp
// copyright : Copyright (c) 2018-present, Lava Block OÜ
// license   : MIT; see accompanying LICENSE file

#include <liblava/lava.hpp>

#include <imgui.h>

using namespace lava;

int main(int argc, char* argv[]) {

    frame_config config;
    config.app = "lava triangle";
    config.cmd_line = { argc, argv };

    frame frame(config);
    if (!frame.ready())
        return error::not_ready;

    window window(config.app);
    if (!window.create())
        return error::create_failed;

    input input;
    window.assign(&input);

    auto device = frame.create_device();
    if (!device)
        return error::create_failed;

    auto render_target = create_target(&window, device);
    if (!render_target)
        return error::create_failed;

    auto frame_count = render_target->get_frame_count();

    forward_shading forward_shading;
    if (!forward_shading.create(render_target))
        return error::create_failed;

    auto render_pass = forward_shading.get_render_pass();

    auto triangle = load_mesh(device, mesh_type::triangle);
    if (!triangle)
        return error::create_failed;

    auto& triangle_data = triangle->get_data();
    triangle_data.vertices.at(0).color = v3(1.f, 0.f, 0.f);
    triangle_data.vertices.at(1).color = v3(0.f, 1.f, 0.f);
    triangle_data.vertices.at(2).color = v3(0.f, 0.f, 1.f);

    if (!triangle->reload())
        return error::create_failed;

    auto triangle_pipeline = graphics_pipeline::make(device);
    pipeline_layout::ptr triangle_pipeline_layout;

    descriptor::ptr triangle_descriptor;
    VkDescriptorSet triangle_descriptor_set;

    {
        if (!triangle_pipeline->add_shader("triangle/vertex.spirv", VK_SHADER_STAGE_VERTEX_BIT))
            return error::create_failed;

        if (!triangle_pipeline->add_shader("triangle/fragment.spirv", VK_SHADER_STAGE_FRAGMENT_BIT))
            return error::create_failed;

        triangle_pipeline->add_color_blend_attachment();

        triangle_pipeline->set_vertex_input_binding({ 0, sizeof(vertex), VK_VERTEX_INPUT_RATE_VERTEX });
        triangle_pipeline->set_vertex_input_attributes({

                            { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, to_ui32(offsetof(vertex, position)) },
                            { 1, 0, VK_FORMAT_R32G32B32_SFLOAT, to_ui32(offsetof(vertex, color)) },
        });

        triangle_descriptor = descriptor::make();
        if (!triangle_descriptor->create(device))
            return error::create_failed;

        triangle_pipeline_layout = pipeline_layout::make();
        triangle_pipeline_layout->add(triangle_descriptor);

        if (!triangle_pipeline_layout->create(device))
            return error::create_failed;

        triangle_pipeline->set_layout(triangle_pipeline_layout);

        triangle_descriptor_set = triangle_descriptor->allocate();

        triangle_pipeline->on_process = [&](VkCommandBuffer cmd_buf) {

            triangle_pipeline_layout->bind(cmd_buf, triangle_descriptor_set);

            triangle->bind_draw(cmd_buf);
        };

        if (!triangle_pipeline->create(render_pass->get()))
            return error::create_failed;

        render_pass->add(triangle_pipeline);
    }

    gui gui(window.get());
    if (!gui.create(device, frame_count, render_pass->get()))
        return error::create_failed;

    render_pass->add(gui.get_pipeline());

    auto fonts = texture::make();
    if (!gui.upload_fonts(fonts))
        return error::create_failed;

    staging staging;
    staging.add(fonts);

    block block;
    if (!block.create(device, frame_count, device->graphics_queue().family))
        return false;

    block.add_command([&](VkCommandBuffer cmd_buf) {

        auto current_frame = block.get_current_frame();

        staging.stage(cmd_buf, current_frame);

        render_pass->process(cmd_buf, current_frame);
    });

    auto show_editor = true;

    gui.on_draw = [&]() {

        if (!show_editor)
            return;

        ImGui::SetNextWindowPos(ImVec2(400, 100), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(200, 70), ImGuiCond_FirstUseEver);

        ImGui::Begin(config.app, &show_editor, ImGuiWindowFlags_NoResize);

        ImGui::Text("%s %s", _liblava_, to_string(_version).c_str());
        ImGui::Text("%.f fps", ImGui::GetIO().Framerate);

        ImGui::End();
    };

    input.add(&gui);

    input.key.listeners.add([&](key_event::ref event) {

        if (gui.want_capture_mouse())
            return;

        if (event.pressed(key::tab))
            show_editor = !show_editor;

        if (event.pressed(key::escape))
            frame.shut_down();
    });

    renderer simple_renderer;
    if (!simple_renderer.create(render_target->get_swapchain()))
        return error::create_failed;

    frame.add_run([&]() {

        input.handle_events();

        if (window.has_close_request())
            return frame.shut_down();

        if (window.has_resize_request())
            return window.handle_resize();

        if (window.iconified()) {

            frame.set_wait_for_events(true);
            return true;

        } else {

            if (frame.waiting_for_events())
                frame.set_wait_for_events(false);
        }

        auto frame_index = simple_renderer.begin_frame();
        if (!frame_index)
            return true;

        if (!block.process(*frame_index))
            return false;

        return simple_renderer.end_frame(block.get_buffers());
    });

    frame.add_run_end([&]() {

        triangle->destroy();

        triangle_descriptor->free(triangle_descriptor_set);
        triangle_descriptor->destroy();

        triangle_pipeline->destroy();
        triangle_pipeline_layout->destroy();

        input.remove(&gui);
        gui.destroy();

        fonts->destroy();

        block.destroy();
        forward_shading.destroy();

        simple_renderer.destroy();
        render_target->destroy();
    });

    return frame.run() ? 0 : error::aborted;
}
