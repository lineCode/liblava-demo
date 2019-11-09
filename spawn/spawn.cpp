// file      : spawn/spawn.cpp
// copyright : Copyright (c) 2018-present, Lava Block OÜ
// license   : MIT; see accompanying LICENSE file

#include <liblava/lava.hpp>

#include <imgui.h>

using namespace lava;

int main(int argc, char* argv[]) {

    frame_config config;
    config.app = "lava spawn";
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

    // CC BY-SA 3.0 https://opengameart.org/content/lava-spawn
    auto spawn_mesh = load_mesh(device, "spawn/lava spawn game.obj");
    if (!spawn_mesh)
        return error::create_failed;

    auto default_texture = create_default_texture(device, { 4096, 4096 });
    if (!default_texture)
        return error::create_failed;

    camera camera;
    camera.position = v3(1.150f, -0.386f, 2.172f);
    camera.rotation = v3(-0.800f, -25.770, 0.f);

    if (!camera.create(device))
        return error::create_failed;

    auto spawn_model = glm::identity<mat4>();

    buffer spawn_model_buffer;
    if (!spawn_model_buffer.create_mapped(device, &spawn_model, sizeof(mat4), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT))
        return error::create_failed;

    auto spawn_pipeline = graphics_pipeline::make(device);
    pipeline_layout::ptr spawn_pipeline_layout;

    descriptor::ptr spawn_descriptor;
    VkDescriptorSet spawn_descriptor_set;

    {
        if (!spawn_pipeline->add_shader("spawn/vertex.spirv", VK_SHADER_STAGE_VERTEX_BIT))
            return error::create_failed;

        if (!spawn_pipeline->add_shader("spawn/fragment.spirv", VK_SHADER_STAGE_FRAGMENT_BIT))
            return error::create_failed;

        spawn_pipeline->add_color_blend_attachment();

        spawn_pipeline->set_depth_test_and_write();
        spawn_pipeline->set_depth_compare_op(VK_COMPARE_OP_LESS_OR_EQUAL);

        spawn_pipeline->set_vertex_input_binding({ 0, sizeof(vertex), VK_VERTEX_INPUT_RATE_VERTEX });
        spawn_pipeline->set_vertex_input_attributes({

                            { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, to_ui32(offsetof(vertex, position)) },
                            { 1, 0, VK_FORMAT_R32G32B32_SFLOAT, to_ui32(offsetof(vertex, color)) },
                            { 2, 0, VK_FORMAT_R32G32_SFLOAT,    to_ui32(offsetof(vertex, uv)) },
        });

        spawn_descriptor = descriptor::make();

        spawn_descriptor->add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
        spawn_descriptor->add_binding(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
        spawn_descriptor->add_binding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);

        if (!spawn_descriptor->create(device))
            return error::create_failed;

        spawn_pipeline_layout = pipeline_layout::make();
        spawn_pipeline_layout->add(spawn_descriptor);

        if (!spawn_pipeline_layout->create(device))
            return error::create_failed;

        spawn_pipeline->set_layout(spawn_pipeline_layout);

        spawn_descriptor_set = spawn_descriptor->allocate();

        VkWriteDescriptorSet write_desc_ubo_camera
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = spawn_descriptor_set,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &camera.data->get_info(),
        };

        VkWriteDescriptorSet write_desc_ubo_spawn
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = spawn_descriptor_set,
            .dstBinding = 1,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &spawn_model_buffer.get_info(),
        };

        VkWriteDescriptorSet write_desc_sampler
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = spawn_descriptor_set,
            .dstBinding = 2,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &default_texture->get_info(),
        };

        device->vkUpdateDescriptorSets({ write_desc_ubo_camera, write_desc_ubo_spawn, write_desc_sampler });

        spawn_pipeline->on_process = [&](VkCommandBuffer cmd_buf) {

            spawn_pipeline_layout->bind(cmd_buf, spawn_descriptor_set);

            spawn_mesh->bind_draw(cmd_buf);
        };

        if (!spawn_pipeline->create(render_pass->get()))
            return error::create_failed;

        render_pass->add(spawn_pipeline);
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
    staging.add(default_texture);

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

        ImGui::SetNextWindowPos(ImVec2(300, 100), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(210, 120), ImGuiCond_FirstUseEver);

        ImGui::Begin(config.app, &show_editor, ImGuiWindowFlags_NoResize);

        ImGui::DragFloat3("cam pos", (r32*)&camera.position, 0.01f);
        ImGui::DragFloat3("cam rot", (r32*)&camera.rotation, 0.1f);

        ImGui::Separator();

        ImGui::Text("%s %s", _liblava_, to_string(_version).c_str());
        ImGui::Text("%.f fps", ImGui::GetIO().Framerate);

        ImGui::End();
    };

    input.add(&gui);

    input.key.listeners.add([&](key_event::ref event) {

        if (gui.want_capture_mouse()) {

            camera.stop();
            return;
        }

        if (event.pressed(key::tab))
            show_editor = !show_editor;

        if (event.pressed(key::escape))
            frame.shut_down();

        if (camera.handle(event))
            return;
    });

    input.mouse_button.listeners.add([&](mouse_button_event::ref event) {

        if (gui.want_capture_mouse())
            return;

        if (camera.handle(event, input.get_mouse_position()))
            return;
    });

    input.scroll.listeners.add([&](scroll_event::ref event) {

        if (gui.want_capture_mouse())
            return;

        if (camera.handle(event))
            return;
    });

    renderer simple_renderer;
    if (!simple_renderer.create(render_target->get_swapchain()))
        return error::create_failed;

    auto last_time = now();

    frame.add_run([&]() {

        input.handle_events();
        input.set_mouse_position(window.get_mouse_position());

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

        auto delta = now() - last_time;
        camera.update_view(delta, input.get_mouse_position());
        last_time = now();

        auto frame_index = simple_renderer.begin_frame();
        if (!frame_index)
            return true;

        if (!block.process(*frame_index))
            return false;

        return simple_renderer.end_frame(block.get_buffers());
    });

    frame.add_run_end([&]() {

        spawn_model_buffer.destroy();
        camera.destroy();

        default_texture->destroy();
        spawn_mesh->destroy();

        spawn_descriptor->free(spawn_descriptor_set);
        spawn_descriptor->destroy();

        spawn_pipeline->destroy();
        spawn_pipeline_layout->destroy();

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
