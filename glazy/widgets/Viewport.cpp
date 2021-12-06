#include "Viewport.h"

#include "../imdraw/imdraw.h"
#include "../imdraw/imdraw_internal.h"

GLuint current_viewport_color = 0;
ImVec2 current_viewport_size{ -1,-1 };
ImVec2 current_viewport_pos{ -1,-1 };
GLint current_viewport_restore_fbo;
GLint current_viewport_restore_viewport[4];

void ControlCamera(Camera* main_camera, const ImVec2& size) {
	ImGui::Button("camera control", size);
	auto io = ImGui::GetIO();
	if (ImGui::IsItemActive()) {
		if (ImGui::IsItemActive()) {
			if (ImGui::IsMouseDragging(0) && (io.KeyMods == ImGuiKeyModFlags_Alt)) {
				main_camera->pan(-io.MouseDelta.x / size.x, -io.MouseDelta.y / size.y);
			}
			if (ImGui::IsMouseDragging(0) && (io.KeyMods == (ImGuiKeyModFlags_Ctrl | ImGuiKeyModFlags_Alt))) {
				main_camera->orbit(-io.MouseDelta.x * 0.006, -io.MouseDelta.y * 0.006);
			}
			if (ImGui::IsMouseDragging(0) && (io.KeyMods == ImGuiKeyModFlags_Ctrl)) {
				auto target_distance = main_camera->get_target_distance();
				main_camera->dolly(-io.MouseDelta.y * target_distance * 0.005);
			}
		}
	}

	if (ImGui::IsItemHovered()) {
		if (io.MouseWheel != 0 && !io.KeyMods) {
			auto target_distance = main_camera->get_target_distance();
			main_camera->dolly(-io.MouseWheel * target_distance * 0.2);
		}
	}
}

void ViewportBegin(GLuint* main_fbo, GLuint* main_color, GLuint* main_depth, Camera* main_camera) {
	assert(viewport_color_tex == 0);
	// calc item pos and scale
	auto item_pos = ImGui::GetCursorPos();
	auto item_size = ImGui::GetContentRegionAvail();

	main_camera->aspect = item_size.x / item_size.y;

	// create button
	ControlCamera(main_camera, item_size);
	ImGui::SetItemAllowOverlap(); // allow overlay buttons

	// display texture with imgui
	ImGui::SetCursorPos(current_viewport_pos);
	ImGui::Image((ImTextureID)current_viewport_color, current_viewport_size, ImVec2(0, 1), ImVec2(1, 0));

	ImGui::SetCursorPos(current_viewport_pos);
	ImGui::Button("reset camera");
	ImGui::Button("pan");
	ImGui::Button("orbit");
	ImGui::Button("dolly");

	// on item resize update fbo resolution
	static ImVec2 initial_widget_size;
	if (initial_widget_size.x != item_size.x || initial_widget_size.y != item_size.y) {
		initial_widget_size = item_size;

		// update fbo and attachments
		glDeleteTextures(1, main_color);
		glDeleteTextures(1, main_depth);
		glDeleteFramebuffers(1, main_fbo);
		*main_color = imdraw::make_texture(item_size.x, item_size.y, NULL, GL_RGB, GL_RGB, GL_FLOAT);
		*main_depth = imdraw::make_texture(item_size.x, item_size.y, NULL, GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_FLOAT);
		*main_fbo = imdraw::make_fbo(*main_color, *main_depth);
	}

	current_viewport_color = *main_color;
	current_viewport_pos = item_pos;
	current_viewport_size = item_size;

	// push viewport FBO and viewport
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &current_viewport_restore_fbo);
	glGetIntegerv(GL_VIEWPORT, current_viewport_restore_viewport);

	glBindFramebuffer(GL_FRAMEBUFFER, *main_fbo);
	glViewport(0, 0, item_size.x, item_size.y);
}

void ViewportEnd() {
	assert(current_viewport_color > 0);

	// restore viewport FBO and viewport
	glBindFramebuffer(GL_FRAMEBUFFER, current_viewport_restore_fbo);
	glViewport(
		current_viewport_restore_viewport[0],
		current_viewport_restore_viewport[1],
		current_viewport_restore_viewport[2],
		current_viewport_restore_viewport[3]
	);
}