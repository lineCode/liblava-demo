// file      : liblava-demo/spawn.cpp
// copyright : Copyright (c) 2018-present, Lava Block OÜ
// license   : MIT; see accompanying LICENSE file

#include <liblava/lava.hpp>

#include <imgui.h>

using namespace lava;

int main(int argc, char* argv[]) {

    app app("lava spawn", { argc, argv });
    if (!app.setup())
        return error::not_ready;

    timer load_timer;

    // CC BY-SA 3.0 https://opengameart.org/content/lava-spawn
    auto spawn_mesh = load_mesh(app.device, "spawn/lava spawn game.obj");
    if (!spawn_mesh)
        return error::create_failed;

    auto mesh_load_time = load_timer.elapsed();

    auto default_texture = create_default_texture(app.device, { 4096, 4096 });
    if (!default_texture)
        return error::create_failed;

    app.staging.add(default_texture);

    app.camera.position = v3(1.150f, -0.386f, 2.172f);
    app.camera.rotation = v3(-0.800f, -25.770f, 0.f);

    auto spawn_model = glm::identity<mat4>();

    buffer spawn_model_buffer;
    if (!spawn_model_buffer.create_mapped(app.device, &spawn_model, sizeof(mat4), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT))
        return error::create_failed;

    graphics_pipeline::ptr spawn_pipeline;
    pipeline_layout::ptr spawn_pipeline_layout;

    VkDescriptorSet spawn_descriptor_set;
    descriptor::ptr spawn_descriptor;

    app.on_create = [&]() {

        spawn_pipeline = graphics_pipeline::make(app.device);

        if (!spawn_pipeline->add_shader("spawn/vertex.spirv", VK_SHADER_STAGE_VERTEX_BIT))
            return false;

        if (!spawn_pipeline->add_shader("spawn/fragment.spirv", VK_SHADER_STAGE_FRAGMENT_BIT))
            return false;

        spawn_pipeline->add_color_blend_attachment();

        spawn_pipeline->set_depth_test_and_write();
        spawn_pipeline->set_depth_compare_op(VK_COMPARE_OP_LESS_OR_EQUAL);

        spawn_pipeline->set_vertex_input_binding({ 0, sizeof(vertex), VK_VERTEX_INPUT_RATE_VERTEX });
        spawn_pipeline->set_vertex_input_attributes({
                                { 0, 0, VK_FORMAT_R32G32B32_SFLOAT,    to_ui32(offsetof(vertex, position)) },
                                { 1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, to_ui32(offsetof(vertex, color)) },
                                { 2, 0, VK_FORMAT_R32G32_SFLOAT,       to_ui32(offsetof(vertex, uv)) },
        });

        spawn_descriptor = descriptor::make();

        spawn_descriptor->add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
        spawn_descriptor->add_binding(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
        spawn_descriptor->add_binding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);

        if (!spawn_descriptor->create(app.device))
            return false;

        spawn_pipeline_layout = pipeline_layout::make();
        spawn_pipeline_layout->add(spawn_descriptor);

        if (!spawn_pipeline_layout->create(app.device))
            return false;

        spawn_pipeline->set_layout(spawn_pipeline_layout);

        spawn_descriptor_set = spawn_descriptor->allocate();

        VkWriteDescriptorSet const write_desc_ubo_camera
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = spawn_descriptor_set,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = app.camera.get_info(),
        };

        VkWriteDescriptorSet const write_desc_ubo_spawn
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = spawn_descriptor_set,
            .dstBinding = 1,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = spawn_model_buffer.get_info(),
        };

        VkWriteDescriptorSet const write_desc_sampler
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = spawn_descriptor_set,
            .dstBinding = 2,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = default_texture->get_info(),
        };

        app.device->vkUpdateDescriptorSets({ write_desc_ubo_camera, write_desc_ubo_spawn, write_desc_sampler });

        spawn_pipeline->on_process = [&](VkCommandBuffer cmd_buf) {

            spawn_pipeline_layout->bind(cmd_buf, spawn_descriptor_set);

            spawn_mesh->bind_draw(cmd_buf);
        };

        auto render_pass = app.forward_shading.get_render_pass();

        if (!spawn_pipeline->create(render_pass->get()))
            return false;

        render_pass->add_front(spawn_pipeline);

        return true;
    };

    app.on_destroy = [&]() {

        spawn_descriptor->free(spawn_descriptor_set);
        spawn_descriptor->destroy();

        spawn_pipeline->destroy();
        spawn_pipeline_layout->destroy();
    };

    auto spawn_position = v3(0.f);
    auto spawn_rotation = v3(0.f);
    auto spawn_scale = v3(1.f);

    auto update_spawn_matrix = false;

    app.gui.on_draw = [&]() {

        ImGui::SetNextWindowPos(ImVec2(300, 100), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(260, 360), ImGuiCond_FirstUseEver);

        ImGui::Begin(app.get_name(), nullptr, ImGuiWindowFlags_NoResize);

        auto camera_active = app.camera.is_active();
        if (ImGui::Checkbox("camera", &camera_active))
            app.camera.set_active(camera_active);

        ImGui::SameLine(0.f, 50.f);

        auto first_person = app.camera.type == camera_type::first_person;
        if (ImGui::Checkbox("first person##camera", &first_person))
            app.camera.type = first_person ? camera_type::first_person : camera_type::look_at;

        ImGui::Spacing();

        ImGui::DragFloat3("position##camera", (r32*)&app.camera.position, 0.01f);
        ImGui::DragFloat3("rotation##camera", (r32*)&app.camera.rotation, 0.1f);
        
        ImGui::Spacing();

        ImGui::Checkbox("lock rotation##camera", &app.camera.lock_rotation);

        ImGui::SameLine(0.f, 20.f);

        ImGui::Checkbox("lock z##camera", &app.camera.lock_z);

        ImGui::Spacing();

        if (ImGui::CollapsingHeader("speed")) {

            ImGui::DragFloat("movement##camera", &app.camera.movement_speed, 0.1f);
            ImGui::DragFloat("rotation##camera", &app.camera.rotation_speed, 0.1f);
            ImGui::DragFloat("zoom##camera", &app.camera.zoom_speed, 0.1f);
        }

        if (ImGui::CollapsingHeader("projection")) {

            auto update_projection = false;

            update_projection |= ImGui::DragFloat("fov", &app.camera.fov);
            update_projection |= ImGui::DragFloat("z near", &app.camera.z_near);
            update_projection |= ImGui::DragFloat("z far", &app.camera.z_far);
            update_projection |= ImGui::DragFloat("aspect", &app.camera.aspect_ratio);

            if (update_projection)
                app.camera.update_projection();
        }

        ImGui::Separator();

        ImGui::Text("spawn (load: %.3f sec)", to_sec(mesh_load_time));

        ImGui::Spacing();

        update_spawn_matrix |= ImGui::DragFloat3("position##spawn", (r32*)&spawn_position, 0.01f);
        update_spawn_matrix |= ImGui::DragFloat3("rotation##spawn", (r32*)&spawn_rotation, 0.1f);
        update_spawn_matrix |= ImGui::DragFloat3("scale##spawn", (r32*)&spawn_scale, 0.1f);

        ImGui::Spacing();

        ImGui::Text("vertices: %d", spawn_mesh->get_vertices_count());

        auto texture_size = default_texture->get_size();
        ImGui::Text("texture: %d x %d", texture_size.x, texture_size.y);

        app.draw_about();

        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("space = first person\nr = lock rotation\nz = lock z");

        ImGui::End();
    };

    app.input.key.listeners.add([&](key_event::ref event) {

        if (app.gui.want_capture_mouse())
            return false;

        if (event.pressed(key::space)) {

            app.camera.type = app.camera.type == camera_type::first_person ? 
                                    camera_type::look_at : camera_type::first_person;
            return true;
        }

        if (event.pressed(key::r))
            app.camera.lock_rotation = !app.camera.lock_rotation;

        if (event.pressed(key::z))
            app.camera.lock_z = !app.camera.lock_z;

        return false;
    });

    gamepad pad(gamepad_id::_1);

    app.on_update = [&](milliseconds delta) {

        if (app.camera.is_active()) {

            app.camera.update_view(delta, app.input.get_mouse_position());

            if (pad.ready() && pad.update())
                app.camera.update_view(delta, pad);
        }

        if (update_spawn_matrix) {

            spawn_model = glm::translate(mat4(1.f), spawn_position);

            spawn_model = glm::rotate(spawn_model, glm::radians(spawn_rotation.x), v3(1.f, 1.f, 0.f));
            spawn_model = glm::rotate(spawn_model, glm::radians(spawn_rotation.y), v3(0.f, 1.f, 0.f));
            spawn_model = glm::rotate(spawn_model, glm::radians(spawn_rotation.z), v3(0.f, 0.f, 1.f));

            spawn_model = glm::scale(spawn_model, spawn_scale);

            memcpy(as_ptr(spawn_model_buffer.get_mapped_data()), &spawn_model, sizeof(mat4));

            update_spawn_matrix = false;
        }

        return true;
    };
 
    app.add_run_end([&]() {

        spawn_model_buffer.destroy();

        default_texture->destroy();
        spawn_mesh->destroy();
    });

    return app.run();
}
