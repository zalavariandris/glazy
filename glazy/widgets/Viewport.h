#include <glad/glad.h>
#include "imgui.h"
#include "../Camera.h"

/**
* Button to control camera
*/
void ControlCamera(Camera* camera, const ImVec2& size = ImVec2(-1, -1));
void ViewportBegin(GLuint* main_fbo, GLuint* main_color, GLuint* main_depth, Camera* main_camera);
void ViewportEnd();