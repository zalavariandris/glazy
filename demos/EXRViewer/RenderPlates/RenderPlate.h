#pragma once
#include "imdraw/imdraw_internal.h"

class RenderPlate
{
private:
    std::filesystem::path mVertexPath;
    std::filesystem::path mFragmentPath;
    GLuint mProgram;

public:
    bool autoreload{true};

    RenderPlate(std::filesystem::path vertex_path, std::filesystem::path fragment_path)
    {
        assert(("vertex file does not exist", std::filesystem::exists(vertex_path)));
        assert(("fragment file does not exist", std::filesystem::exists(fragment_path)));
        
        mVertexPath = vertex_path;
        mFragmentPath = fragment_path;
    }

    void set_uniforms(std::map<std::string, imdraw::UniformVariant> uniforms) {
        imdraw::set_uniforms(mProgram, uniforms);
    }

    void reload(bool force=false)
    {
        //ZoneScoped;
        bool HasChanged = glazy::is_file_modified(mFragmentPath) || glazy::is_file_modified(mVertexPath);
        if (!force && !HasChanged) return;

        std::cout << "recompile checker shader" << "\n";

        // reload shader code
        auto fragment_code = glazy::read_text(mFragmentPath.string().c_str());
        auto vertex_code = glazy::read_text(mVertexPath.string().c_str());

        // link new program
        GLuint program = imdraw::make_program_from_source(
            vertex_code.c_str(),
            fragment_code.c_str()
        );

        // swap programs
        if (glIsProgram(mProgram)) glDeleteProgram(mProgram);
        mProgram = program;
    }

    void render()
    {
        //ZoneScopedN("draw polka");

        // Auto reload shaders
        if(autoreload) reload();

        /// Create geometry
        static GLuint vbo = imdraw::make_vbo(std::vector<glm::vec3>({ {-1,-1,0}, {1,-1,0}, {-1,1,0}, {1,1,0} }));
        static auto vao = imdraw::make_vao(mProgram, { {"aPos", {vbo, 3}} });

        /// Draw quad with fragment shader
        imdraw::push_program(mProgram);
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);
        imdraw::pop_program();
    }

    ~RenderPlate() {
        if (glIsProgram(mProgram)) glDeleteProgram(mProgram);
    }
};