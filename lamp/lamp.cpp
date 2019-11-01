// file      : lamp/lamp.cpp
// copyright : Copyright (c) 2018-present, Lava Block OÜ
// license   : MIT; see accompanying LICENSE file

#include <liblava/lava.hpp>

#include <imgui.h>

using namespace lava;

int main(int argc, char* argv[]) {

    frame_config config;
    config.app = "lava lamp";
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

    auto lamp_depth = .03f;
    auto lamp_color = v4(.3f, .15f, .15f, 1.f);

    auto lamp_pipeline = graphics_pipeline::make(device);
    pipeline_layout::ptr lamp_pipeline_layout;
    
    descriptor::ptr lamp_descriptor;
    VkDescriptorSet lamp_descriptor_set = {};

    {
        if (!lamp_pipeline->add_shader_stage("lamp/vertex.spirv", VK_SHADER_STAGE_VERTEX_BIT))
            return error::create_failed;

        if (!lamp_pipeline->add_shader_stage("lamp/fragment.spirv", VK_SHADER_STAGE_FRAGMENT_BIT))
            return error::create_failed;

        lamp_pipeline->add_color_blend_attachment();

        lamp_pipeline->set_rasterization_cull_mode(VK_CULL_MODE_FRONT_BIT);
        lamp_pipeline->set_rasterization_front_face(VK_FRONT_FACE_COUNTER_CLOCKWISE);

        lamp_descriptor = descriptor::make();
        if (!lamp_descriptor->create(device))
            return error::create_failed;

        lamp_pipeline_layout = pipeline_layout::make();
        lamp_pipeline_layout->add(lamp_descriptor);
        lamp_pipeline_layout->add_range({ VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(r32) * 8 });

        if (!lamp_pipeline_layout->create(device))
            return error::create_failed;

        lamp_pipeline->set_layout(lamp_pipeline_layout);
        lamp_pipeline->set_auto_bind(true);
        lamp_pipeline->set_auto_size(true);

        lamp_descriptor_set = lamp_descriptor->allocate_set();

        lamp_pipeline->on_process = [&](VkCommandBuffer cmd_buf) {

            lamp_pipeline_layout->bind_descriptor_set(cmd_buf, lamp_descriptor_set);

            auto viewport = lamp_pipeline->get_viewport();

            r32 pc_resolution[2];
            pc_resolution[0] = viewport.width - viewport.x;
            pc_resolution[1] = viewport.height - viewport.y;
            vkCmdPushConstants(cmd_buf, lamp_pipeline_layout->get(), VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(r32) * 0, sizeof(r32) * 2, pc_resolution);

            r32 pc_time_depth[2];
            pc_time_depth[0] = to_r32(to_sec(frame.get_running_time()));
            pc_time_depth[1] = lamp_depth;
            vkCmdPushConstants(cmd_buf, lamp_pipeline_layout->get(), VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(r32) * 2, sizeof(r32) * 2, pc_time_depth);

            r32 pc_color[4];
            pc_color[0] = lamp_color.r;
            pc_color[1] = lamp_color.g;
            pc_color[2] = lamp_color.b;
            pc_color[3] = lamp_color.a;
            vkCmdPushConstants(cmd_buf, lamp_pipeline_layout->get(), VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(r32) * 4, sizeof(r32) * 4, pc_color);

            vkCmdDraw(cmd_buf, 3, 1, 0, 0);
        };

        if (!lamp_pipeline->create(render_pass->get()))
            return error::create_failed;

        render_pass->add(lamp_pipeline);
    }

    gui gui(window.get());
    if (!gui.create(device, frame_count))
        return error::create_failed;

    {
        auto gui_pipeline = gui.get_pipeline();

        if (!gui_pipeline->create(render_pass->get()))
            return error::create_failed;

        render_pass->add(gui_pipeline);
    }

    auto fonts = texture::make();
    if (!gui.upload_fonts(fonts))
        return error::create_failed;

    staging staging;
    staging.add(fonts);

    block block;
    if (!block.create(device, frame_count, device->graphics_queue().family))
        return false;

    block.add_command([&](VkCommandBuffer cmd_buf) {

        staging.stage(cmd_buf, block.get_current_frame());

        render_pass->process(cmd_buf, block.get_current_frame());
    });

    auto show_editor = true;

    gui.on_draw = [&]() {

        if (!show_editor)
            return;

        ImGui::SetNextWindowPos(ImVec2(400, 100), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(200, 150), ImGuiCond_FirstUseEver);

        ImGui::Begin(config.app, &show_editor, ImGuiWindowFlags_NoResize);

        auto lamp_active = lamp_pipeline->is_active();
        if (ImGui::Checkbox("active", &lamp_active))
            lamp_pipeline->toggle();

        ImGui::Spacing();

        ImGui::DragFloat("depth", &lamp_depth, 0.0001f, 0.01f, 1.f);
        ImGui::ColorEdit4("color", (r32*)&lamp_color);

        ImGui::Separator();

        ImGui::Text("%s %s", _liblava_, to_string(_version).c_str());
        ImGui::Text("%.f fps", ImGui::GetIO().Framerate);

        ImGui::End();
    };

    input.add(&gui);

    input.key.listeners.add([&](key_event::ref event) {

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

        lamp_descriptor->free_set(lamp_descriptor_set);
        lamp_descriptor->destroy();

        lamp_pipeline->destroy();
        lamp_pipeline_layout->destroy();

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
