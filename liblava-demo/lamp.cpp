// file      : liblava-demo/lamp.cpp
// copyright : Copyright (c) 2018-present, Lava Block OÜ
// license   : MIT; see accompanying LICENSE file

#include <liblava/lava.hpp>

#include <imgui.h>

using namespace lava;

int main(int argc, char* argv[]) {

    app app("lava lamp", { argc, argv });
    if (!app.setup())
        return error::not_ready;

    auto lamp_depth = .03f;
    auto lamp_color = v4(.3f, .15f, .15f, 1.f);

    auto lamp_pipeline = graphics_pipeline::make(app.device);
    
    if (!lamp_pipeline->add_shader("lamp/vertex.spirv", VK_SHADER_STAGE_VERTEX_BIT))
        return error::create_failed;

    if (!lamp_pipeline->add_shader("lamp/fragment.spirv", VK_SHADER_STAGE_FRAGMENT_BIT))
        return error::create_failed;

    lamp_pipeline->add_color_blend_attachment();

    lamp_pipeline->set_rasterization_cull_mode(VK_CULL_MODE_FRONT_BIT);
    lamp_pipeline->set_rasterization_front_face(VK_FRONT_FACE_COUNTER_CLOCKWISE);

    auto lamp_descriptor = descriptor::make();
    if (!lamp_descriptor->create(app.device))
        return error::create_failed;

    auto lamp_pipeline_layout = pipeline_layout::make();
    lamp_pipeline_layout->add(lamp_descriptor);
    lamp_pipeline_layout->add_range({ VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(r32) * 8 });

    if (!lamp_pipeline_layout->create(app.device))
        return error::create_failed;

    lamp_pipeline->set_layout(lamp_pipeline_layout);
    lamp_pipeline->set_auto_size(true);

    auto lamp_descriptor_set = lamp_descriptor->allocate();

    lamp_pipeline->on_process = [&](VkCommandBuffer cmd_buf) {

        lamp_pipeline_layout->bind(cmd_buf, lamp_descriptor_set);

        auto viewport = lamp_pipeline->get_viewport();

        r32 pc_resolution[2];
        pc_resolution[0] = viewport.width - viewport.x;
        pc_resolution[1] = viewport.height - viewport.y;
        app.device->call().vkCmdPushConstants(cmd_buf, lamp_pipeline_layout->get(), VK_SHADER_STAGE_FRAGMENT_BIT,
                                                       sizeof(r32) * 0, sizeof(r32) * 2, pc_resolution);

        auto pc_time = to_r32(to_sec(app.get_running_time()));
        app.device->call().vkCmdPushConstants(cmd_buf, lamp_pipeline_layout->get(), VK_SHADER_STAGE_FRAGMENT_BIT,
                                                       sizeof(r32) * 2, sizeof(r32), &pc_time);

        app.device->call().vkCmdPushConstants(cmd_buf, lamp_pipeline_layout->get(), VK_SHADER_STAGE_FRAGMENT_BIT,
                                                       sizeof(r32) * 3, sizeof(r32), &lamp_depth);

        app.device->call().vkCmdPushConstants(cmd_buf, lamp_pipeline_layout->get(), VK_SHADER_STAGE_FRAGMENT_BIT,
                                                       sizeof(r32) * 4, sizeof(r32) * 4, glm::value_ptr(lamp_color));

        app.device->call().vkCmdDraw(cmd_buf, 3, 1, 0, 0);
    };

    auto render_pass = app.forward_shading.get_render_pass();

    if (!lamp_pipeline->create(render_pass->get()))
        return error::create_failed;

    render_pass->add_front(lamp_pipeline);

    app.gui.on_draw = [&]() {

        ImGui::SetNextWindowPos(ImVec2(400, 100), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(200, 150), ImGuiCond_FirstUseEver);

        ImGui::Begin(app.get_name(), nullptr, ImGuiWindowFlags_NoResize);

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

    app.add_run_end([&]() {

        lamp_descriptor->free(lamp_descriptor_set);
        lamp_descriptor->destroy();

        lamp_pipeline->destroy();
        lamp_pipeline_layout->destroy();
    });

    return app.run() ? 0 : error::aborted;
}
