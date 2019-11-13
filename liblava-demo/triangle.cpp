// file      : liblava-demo/triangle.cpp
// copyright : Copyright (c) 2018-present, Lava Block OÜ
// license   : MIT; see accompanying LICENSE file

#include <liblava/lava.hpp>

#include <imgui.h>

using namespace lava;

int main(int argc, char* argv[]) {

    app app("lava triangle", { argc, argv });
    if (!app.setup())
        return error::not_ready;

    auto triangle = load_mesh(app.device, mesh_type::triangle);
    if (!triangle)
        return error::create_failed;

    auto& triangle_data = triangle->get_data();
    triangle_data.vertices.at(0).color = v4(1.f, 0.f, 0.f, 1.f);
    triangle_data.vertices.at(1).color = v4(0.f, 1.f, 0.f, 1.f);
    triangle_data.vertices.at(2).color = v4(0.f, 0.f, 1.f, 1.f);

    if (!triangle->reload())
        return error::create_failed;

    auto triangle_pipeline = graphics_pipeline::make(app.device);

    if (!triangle_pipeline->add_shader("triangle/vertex.spirv", VK_SHADER_STAGE_VERTEX_BIT))
        return error::create_failed;

    if (!triangle_pipeline->add_shader("triangle/fragment.spirv", VK_SHADER_STAGE_FRAGMENT_BIT))
        return error::create_failed;

    triangle_pipeline->add_color_blend_attachment();

    triangle_pipeline->set_vertex_input_binding({ 0, sizeof(vertex), VK_VERTEX_INPUT_RATE_VERTEX });
    triangle_pipeline->set_vertex_input_attributes({
                            { 0, 0, VK_FORMAT_R32G32B32_SFLOAT,    to_ui32(offsetof(vertex, position)) },
                            { 1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, to_ui32(offsetof(vertex, color)) },
    });

    auto triangle_descriptor = descriptor::make();
    if (!triangle_descriptor->create(app.device))
        return error::create_failed;

    auto triangle_pipeline_layout = pipeline_layout::make();
    triangle_pipeline_layout->add(triangle_descriptor);

    if (!triangle_pipeline_layout->create(app.device))
        return error::create_failed;

    triangle_pipeline->set_layout(triangle_pipeline_layout);

    auto triangle_descriptor_set = triangle_descriptor->allocate();

    triangle_pipeline->on_process = [&](VkCommandBuffer cmd_buf) {

        triangle_pipeline_layout->bind(cmd_buf, triangle_descriptor_set);

        triangle->bind_draw(cmd_buf);
    };

    auto render_pass = app.forward_shading.get_render_pass();

    if (!triangle_pipeline->create(render_pass->get()))
        return error::create_failed;

    render_pass->add_front(triangle_pipeline);

    app.gui.on_draw = [&]() {

        ImGui::SetNextWindowPos(ImVec2(400, 100), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(200, 70), ImGuiCond_FirstUseEver);

        ImGui::Begin(app.get_name(), nullptr, ImGuiWindowFlags_NoResize);

        ImGui::Text("%s %s", _liblava_, to_string(_version).c_str());
        ImGui::Text("%.f fps", ImGui::GetIO().Framerate);

        ImGui::End();
    };

    app.add_run_end([&]() {

        triangle->destroy();

        triangle_descriptor->free(triangle_descriptor_set);
        triangle_descriptor->destroy();

        triangle_pipeline->destroy();
        triangle_pipeline_layout->destroy();
    });

    return app.run() ? 0 : error::aborted;
}
