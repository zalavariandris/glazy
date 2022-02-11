#pragma once

#include <filesystem>
#include <optional>

#include <glad/glad.h>
#include <glm/glm.hpp>

#include "imdraw/imdraw.h"
#include "imgeo/imgeo.h"
#include "imdraw/imdraw_internal.h"

const char* PASS_CAMERA_VERTEX_CODE = R"(
#version 330 core
layout (location = 0) in vec3 aPos;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

out vec3 nearPoint;
out vec3 farPoint;

uniform bool use_geometry;

vec3 UnprojectPoint(float x, float y, float z, mat4 view, mat4 projection) {
    mat4 viewInv = inverse(view);
    mat4 projInv = inverse(projection);
    vec4 unprojectedPoint =  viewInv * projInv * vec4(x, y, z, 1.0);
    return unprojectedPoint.xyz / unprojectedPoint.w;
}
				
void main()
{
    nearPoint = UnprojectPoint(aPos.x, aPos.y, 0.0, view, projection).xyz;
    farPoint = UnprojectPoint(aPos.x, aPos.y, 1.0, view, projection).xyz;
    gl_Position = use_geometry ? (projection * view * model * vec4(aPos, 1.0)) : vec4(aPos, 1.0);
}
)";

class ShaderToy {
private:
    std::filesystem::path fragment_path;
    std::filesystem::file_time_type last_compile_time;
    GLuint m_program;
    //imdraw::UniformVariant uniform;

    std::optional<imgeo::Trimesh> geometry;
    GLuint vbo;
    GLuint ebo;
    GLuint vao;

public:
    ShaderToy(const std::filesystem::path& fragment_path, const std::optional<imgeo::Trimesh>& geometry = std::nullopt) :
        fragment_path(fragment_path),
        m_program(0),
        geometry(geometry)
    {
        autoreload();
        if (geometry) {
            auto geo = geometry.value();
            vao = imdraw::make_vao(m_program, {
                {"aPos",    {imdraw::make_vbo(geo.positions),      3}},
                {"aUV",     {imdraw::make_vbo(geo.uvs.value()),    2}},
                {"aNormal", {imdraw::make_vbo(geo.normals.value()),3}}
                });
            ebo = imdraw::make_ebo(geo.indices);
        }
        else {
            auto geo = imgeo::quad();
            vao = imdraw::make_vao(m_program, {
                {"aPos",    {imdraw::make_vbo(geo.positions),      3}},
                {"aUV",     {imdraw::make_vbo(geo.uvs.value()),    2}},
                {"aNormal", {imdraw::make_vbo(geo.normals.value()),3}}
            });
        }
    }

    // if source files has changed, reload code and recompile shaders
    bool autoreload()
    {
        auto last_write_time = std::filesystem::last_write_time(fragment_path);
        if (last_write_time != last_compile_time) {
            last_compile_time = last_write_time;
            static auto vertex_shader = imdraw::make_shader(GL_VERTEX_SHADER, PASS_CAMERA_VERTEX_CODE);
            if (glIsProgram(m_program)) {
                glDeleteProgram(m_program);
            }
            // read
            auto fragment_code = glazy::read_text(fragment_path.string().c_str());
            // compile
            auto fragment_shader = imdraw::make_shader(GL_FRAGMENT_SHADER, fragment_code.c_str());
            // link
            m_program = imdraw::make_program_from_shaders(vertex_shader, fragment_shader);

            glDeleteShader(vertex_shader);
            glDeleteShader(fragment_shader);
            return true;
        }
        return false;
    }

    void draw(const std::map<std::string, imdraw::UniformVariant>& uniforms) const
    {
        imdraw::push_program(m_program);
        /// Draw quad with fragment shader
       
        imdraw::set_uniforms(m_program, {
            {"projection", uniforms.contains("projection") ? uniforms.at("projection") : glm::ortho(-1,1,-1,1)},
            {"view", uniforms.contains("view") ? uniforms.at("view") : glm::mat4(1)},
            {"model", uniforms.contains("model") ? uniforms.at("model") : glm::mat4(1)},
            {"uResolution", uniforms.contains("uResolution") ? uniforms.at("uResolution") : glm::ivec2(1,1)},
            {"use_geometry", geometry.has_value()}
            });
        //glBindTexture(GL_TEXTURE_2D, tex);
        if (geometry.has_value()) {
            glBindVertexArray(vao);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
            glDrawElements(geometry.value().mode, geometry.value().indices.size(), GL_UNSIGNED_INT, NULL);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
            glBindVertexArray(0);
        }
        else {
            glBindVertexArray(vao);
            glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
            glBindVertexArray(0);
        }
        //glBindTexture(GL_TEXTURE_2D, 0);
        imdraw::pop_program();
    }
};