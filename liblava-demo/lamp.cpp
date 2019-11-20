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

    graphics_pipeline::ptr lamp_pipeline;
    pipeline_layout::ptr lamp_pipeline_layout;

    VkDescriptorSet lamp_descriptor_set;
    descriptor::ptr lamp_descriptor;

    app.on_create = [&]() {

        lamp_pipeline = graphics_pipeline::make(app.device);

        if (!lamp_pipeline->add_shader("lamp/vertex.spirv", VK_SHADER_STAGE_VERTEX_BIT))
            return false;

        if (!lamp_pipeline->add_shader("lamp/fragment.spirv", VK_SHADER_STAGE_FRAGMENT_BIT))
            return false;

        lamp_pipeline->add_color_blend_attachment();

        lamp_pipeline->set_rasterization_cull_mode(VK_CULL_MODE_FRONT_BIT);
        lamp_pipeline->set_rasterization_front_face(VK_FRONT_FACE_COUNTER_CLOCKWISE);

        lamp_descriptor = descriptor::make();
        if (!lamp_descriptor->create(app.device))
            return false;

        lamp_pipeline_layout = pipeline_layout::make();
        lamp_pipeline_layout->add(lamp_descriptor);
        lamp_pipeline_layout->add_range({ VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(r32) * 8 });

        if (!lamp_pipeline_layout->create(app.device))
            return false;

        lamp_pipeline->set_layout(lamp_pipeline_layout);
        lamp_pipeline->set_auto_size(true);

        lamp_descriptor_set = lamp_descriptor->allocate();

        lamp_pipeline->on_process = [&](VkCommandBuffer cmd_buf) {

            lamp_pipeline_layout->bind(cmd_buf, lamp_descriptor_set);

            auto viewport = lamp_pipeline->get_viewport();

            r32 pc_resolution[2];
            pc_resolution[0] = viewport.width - viewport.x;
            pc_resolution[1] = viewport.height - viewport.y;
            app.device->call().vkCmdPushConstants(cmd_buf, lamp_pipeline_layout->get(), VK_SHADER_STAGE_FRAGMENT_BIT,
                                                           sizeof(r32) * 0, sizeof(r32) * 2, pc_resolution);

            auto pc_time = to_r32(to_sec(app.run_time.current));
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
            return false;

        render_pass->add_front(lamp_pipeline);

        return true;
    };

    app.on_destroy = [&]() {

        lamp_descriptor->free(lamp_descriptor_set);
        lamp_descriptor->destroy();

        lamp_pipeline->destroy();
        lamp_pipeline_layout->destroy();
    };

    auto auto_play = true;

    app.input.key.listeners.add([&](key_event::ref event) {

        if (app.gui.want_capture_mouse())
            return false;

        if (event.pressed(key::enter)) {

            auto_play = !auto_play;
            return true;
        }

        return false;
    });

    app.gui.on_draw = [&]() {

        ImGui::SetNextWindowPos(ImVec2(400, 100), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(190, 190), ImGuiCond_FirstUseEver);

        ImGui::Begin(app.get_name());

        auto lamp_active = lamp_pipeline->is_active();
        if (ImGui::Checkbox("active", &lamp_active))
            lamp_pipeline->toggle();

        ImGui::SameLine(0.f, 10.f);

        ImGui::Checkbox("auto play", &auto_play);

        ImGui::Spacing();

        ImGui::DragFloat("depth", &lamp_depth, 0.0001f, 0.01f, 1.f, "%.4f");
        ImGui::ColorEdit4("color", (r32*)&lamp_color);

        auto clear_color = app.forward_shading.get_render_pass()->get_clear_color();
        if (ImGui::ColorEdit3("ground", (r32*)&clear_color))
            app.forward_shading.get_render_pass()->set_clear_color(clear_color);

        ImGui::DragFloat("speed", &app.run_time.speed, 0.001f, -10.f, 10.f, "x %.3f");

        app.draw_about();

        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("enter = auto play");

        ImGui::End();
    };

    struct dimmer {

        explicit dimmer() {

            next_factor();
        }

        r32 update(delta dt, r32 value) {

            auto next = factor * dt;

            if (add)
                value += next;
            else
                value -= next;

            if (value > max) {

                add = false;
                next_factor();
            }
            else if (value < min) {

                add = true;
                next_factor();
            }

            return value;
        }

        void next_factor() {

            factor = random(factor_min, factor_max);
        }

        r32 factor = 0.f;
        r32 factor_min = 0.00001f;
        r32 factor_max = 0.0001f;

        bool add = false;
        r32 min = 0.01f;
        r32 max = 0.03f;
    };

    dimmer depth_dimmer;
    dimmer color_dimmer;
    color_dimmer.min = 0.f;
    color_dimmer.max = 1.f;
    color_dimmer.factor_min = 0.0005f;
    color_dimmer.factor_max = 0.02f;
    color_dimmer.next_factor();
    
    auto r_dimmer = color_dimmer;
    r_dimmer.add = true;

    auto g_dimmer = color_dimmer;
    g_dimmer.add = true;

    auto b_dimmer = color_dimmer;
    auto a_dimmer = color_dimmer;
    a_dimmer.min = 0.2f;

    app.on_update = [&](delta dt) {

        if (!auto_play || !lamp_pipeline->is_active())
            return true;
        
        lamp_depth = depth_dimmer.update(dt, lamp_depth);

        lamp_color.r = r_dimmer.update(dt, lamp_color.r);
        lamp_color.g = g_dimmer.update(dt, lamp_color.g);
        lamp_color.b = b_dimmer.update(dt, lamp_color.b);
        lamp_color.a = a_dimmer.update(dt, lamp_color.a);

        return true;
    };

    return app.run();
}
