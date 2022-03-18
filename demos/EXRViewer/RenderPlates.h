#pragma once
#include "imdraw/imdraw_internal.h"

class PolkaPlate
{
private:
    int mWidth;
    int mHeight;
    glm::mat4 mView;
    glm::mat4 mProjection;
public:
    PolkaPlate(int width, int height) {
        mWidth = width;
        mHeight = height;
    }

    void set_view(glm::mat4 view) {
        mView = view;
    }

    void set_projection(glm::mat4 projection)
    {
        mProjection = projection;
    }

    void draw()
    {
        ZoneScopedN("draw polka");
        static std::filesystem::path fragment_path{ "./shaders/polka.frag" };
        static std::string fragment_code;
        static std::filesystem::path vertex_path{ "./shaders/PASS_THROUGH_CAMERA.vert" };
        static std::string vertex_code;


        static GLuint polka_program;
        if (glazy::is_file_modified(fragment_path) && glazy::is_file_modified(vertex_path)) {
            // reload shader
            ZoneScopedN("recompile polka shader");
            fragment_code = glazy::read_text(fragment_path.string().c_str());
            vertex_code = glazy::read_text(vertex_path.string().c_str());
            if (glIsProgram(polka_program)) glDeleteProgram(polka_program); // recompile shader
            polka_program = imdraw::make_program_from_source(
                vertex_code.c_str(),
                fragment_code.c_str()
            );
        }

        /// Draw quad with fragment shader
        static GLuint vbo = imdraw::make_vbo(std::vector<glm::vec3>({ {-1,-1,0}, {1,-1,0}, {-1,1,0}, {1,1,0} }));
        static auto vao = imdraw::make_vao(polka_program, { {"aPos", {vbo, 3}} });

        imdraw::push_program(polka_program);
        imdraw::set_uniforms(polka_program, {
            {"projection", mProjection},
            {"view", mView},
            {"model", glm::mat4(1)},
            {"uResolution", glm::vec2(mWidth, mHeight)},
            {"radius", 0.01f}
            });

        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);
        imdraw::pop_program();
    }
};

class CheckerPlate
{
private:
    glm::mat4 mView{1};
    glm::mat4 mProjection{1};
    GLuint mProgram;
public:
    CheckerPlate() {}

    /*void set_view(glm::mat4 view) {
        mView = view;
    }

    void set_projection(glm::mat4 projection)
    {
        mProjection = projection;
    }*/

    void set_uniforms(std::map<std::string, imdraw::UniformVariant> uniforms) {
        imdraw::set_uniforms(mProgram, uniforms);
    }

    void draw()
    {
        ZoneScopedN("draw polka");
        static std::filesystem::path fragment_path{ "./shaders/checker.frag" };
        static std::string fragment_code;
        static std::filesystem::path vertex_path{ "./shaders/checker.vert" };
        static std::string vertex_code;

       

        if (glazy::is_file_modified(fragment_path) || glazy::is_file_modified(vertex_path)) {
            // reload shader
            //ZoneScopedN("recompile polka shader");
            std::cout << "recompile checker shader" << "\n";
            fragment_code = glazy::read_text(fragment_path.string().c_str());
            vertex_code = glazy::read_text(vertex_path.string().c_str());
            if (glIsProgram(mProgram)) glDeleteProgram(mProgram); // recompile shader
            mProgram = imdraw::make_program_from_source(
                vertex_code.c_str(),
                fragment_code.c_str()
            );
        }

        /// Draw quad with fragment shader
        static GLuint vbo = imdraw::make_vbo(std::vector<glm::vec3>({ {-1,-1,0}, {1,-1,0}, {-1,1,0}, {1,1,0} }));
        static auto vao = imdraw::make_vao(mProgram, { {"aPos", {vbo, 3}} });

        imdraw::push_program(mProgram);
        //set_uniforms({
        //    {"projection", mProjection},
        //    {"view", mView},
        //    {"model", glm::mat4(1)},
        //    {"uResolution", glm::vec2(mWidth, mHeight)},
        //    {"radius", 0.01f}
        //});

        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);
        imdraw::pop_program();
    }
};