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

    auto spawn_pipeline = graphics_pipeline::make(app.device);

    if (!spawn_pipeline->add_shader("spawn/vertex.spirv", VK_SHADER_STAGE_VERTEX_BIT))
        return error::create_failed;

    if (!spawn_pipeline->add_shader("spawn/fragment.spirv", VK_SHADER_STAGE_FRAGMENT_BIT))
        return error::create_failed;

    spawn_pipeline->add_color_blend_attachment();

    spawn_pipeline->set_depth_test_and_write();
    spawn_pipeline->set_depth_compare_op(VK_COMPARE_OP_LESS_OR_EQUAL);

    spawn_pipeline->set_vertex_input_binding({ 0, sizeof(vertex), VK_VERTEX_INPUT_RATE_VERTEX });
    spawn_pipeline->set_vertex_input_attributes({
                            { 0, 0, VK_FORMAT_R32G32B32_SFLOAT,    to_ui32(offsetof(vertex, position)) },
                            { 1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, to_ui32(offsetof(vertex, color)) },
                            { 2, 0, VK_FORMAT_R32G32_SFLOAT,       to_ui32(offsetof(vertex, uv)) },
    });

    auto spawn_descriptor = descriptor::make();

    spawn_descriptor->add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
    spawn_descriptor->add_binding(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
    spawn_descriptor->add_binding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);

    if (!spawn_descriptor->create(app.device))
        return error::create_failed;

    auto spawn_pipeline_layout = pipeline_layout::make();
    spawn_pipeline_layout->add(spawn_descriptor);

    if (!spawn_pipeline_layout->create(app.device))
        return error::create_failed;

    spawn_pipeline->set_layout(spawn_pipeline_layout);

    auto spawn_descriptor_set = spawn_descriptor->allocate();

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
        return error::create_failed;

    render_pass->add_front(spawn_pipeline);

    app.gui.on_draw = [&]() {

        ImGui::SetNextWindowPos(ImVec2(300, 100), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(220, 180), ImGuiCond_FirstUseEver);

        ImGui::Begin(app.get_name(), nullptr, ImGuiWindowFlags_NoResize);

        ImGui::DragFloat3("cam pos", (r32*)&app.camera.position, 0.01f);
        ImGui::DragFloat3("cam rot", (r32*)&app.camera.rotation, 0.1f);

        ImGui::Separator();

        ImGui::Text("mesh load time: %.3f sec", to_sec(mesh_load_time));
        ImGui::Text("mesh vertices: %d", spawn_mesh->get_vertices_count());

        ImGui::Spacing();

        auto texture_size = default_texture->get_size();
        ImGui::Text("texture size: %d x %d", texture_size.x, texture_size.y);

        ImGui::Separator();

        ImGui::Text("%s %s", _liblava_, to_string(_version).c_str());
        ImGui::Text("%.f fps", ImGui::GetIO().Framerate);

        ImGui::End();
    };

    app.add_run_end([&]() {

        spawn_model_buffer.destroy();

        default_texture->destroy();
        spawn_mesh->destroy();

        spawn_descriptor->free(spawn_descriptor_set);
        spawn_descriptor->destroy();

        spawn_pipeline->destroy();
        spawn_pipeline_layout->destroy();
    });

    return app.run() ? 0 : error::aborted;
}
