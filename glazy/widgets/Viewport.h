#include <glad/glad.h>
#include "imgui.h"
#include "../Camera.h"


void ControlCamera(Camera* main_camera, const ImVec2& size = ImVec2(-1, -1));
void ViewportBegin(GLuint* main_fbo, GLuint* main_color, GLuint* main_depth, Camera* main_camera);
void ViewportEnd();