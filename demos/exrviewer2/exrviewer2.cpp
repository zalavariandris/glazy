// exrviewer2.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>

#include "glazy.h"
#include "pathutils.h"
#include "OpenImageIO/imageio.h""
#include "OpenImageIO/imagecache.h"
#include <set>
#include <deque>


class BaseVar {
protected:
    mutable bool dirty=true;
public:
    mutable std::set<BaseVar*> dependants;
    std::string name;
    ImVec2 pos={0,0};
    void set_dirty(bool val=true) {
        dirty = val;
    }
};

std::vector<BaseVar*>depstack;
std::set<BaseVar*> nodes;

template <typename T>
class State : public BaseVar {
private:
    mutable T cache;
public:
    State(T val, std::string _name="") :cache(val) {
        nodes.insert(this);
        name = _name;
    };
    void set(T val) {
        cache = val;
        for (auto dep : dependants) dep->set_dirty();
    }
    T get() const {
        if (!depstack.empty()) {
            dependants.insert(depstack.back());
        }
        return cache;
    }
};

template <typename T>
class Lazy : public BaseVar{
private:
    mutable T cache;
    std::function<T()> evaluate;
public:
    Lazy(std::function<T()> f, std::string _name="") : evaluate(f) {
        nodes.insert(this);
        name = _name;
    };

    T get() const {
        if (!depstack.empty()) {
            dependants.insert(depstack.back());
        }
        if (dirty) {
            depstack.push_back((BaseVar*)this);
            cache = evaluate();
            depstack.pop_back();
            dirty = false;
            for (auto dep : dependants) dep->set_dirty();
        }
        
        return cache;
    }
};

auto state_pattern = State<std::filesystem::path>("C:/Users/andris/Desktop/openexr-images-master/Beachball/singlepart.%04d.exr", "pattern");
auto state_frame = State<int>(1, "frame");
auto selected_group = State<int>(0, "selected\ngroup");
auto selected_subimage = State<int>(0, "selected\nsubimage");

auto computed_input_frame = Lazy<std::filesystem::path>([]() {
    std::cout << "evaluate input_frame" << "\n";
    return sequence_item(state_pattern.get(), state_frame.get());
}, "input frame");

auto subimages = Lazy<std::vector<std::string>>([]() {
    std::vector<std::string> result;
    if(!std::filesystem::exists(computed_input_frame.get())) return result;

    std::cout << "evaluate subimages" << "\n";
    auto name = computed_input_frame.get().string();

    auto in = OIIO::ImageInput::open(name);
    int nsubimages = 0;
    while (in->seek_subimage(nsubimages, 0)) {
        result.push_back( in->spec().get_string_attribute("name"));
        ++nsubimages;
    }
    return result;
}, "subimages");

auto groups = Lazy<std::vector<std::string>>([]()->std::vector<std::string> {
    if (!std::filesystem::exists(computed_input_frame.get())) return {};

    auto name = computed_input_frame.get().string();
    auto spec = OIIO::ImageSpec();
    auto in = OIIO::ImageInput::open(name, &spec);
    if (in->seek_subimage(selected_subimage.get(), 0)) {
        std::vector<std::string> channel_names;
        for (auto c = 0; c < in->spec().nchannels; c++) {
            auto channel_name = in->spec().channel_name(c);
            channel_names.push_back(channel_name);
        }
        return channel_names;
    }

    return {};
}, "groups");

auto texture = Lazy<GLuint>([]()->GLuint {

    return 0;
}, "texture");

//std::filesystem::path INPUT_PATTERN = "C:/Users/andris/Desktop/openexr-images-master/Beachball/singlepart.%04d.exr";
int START_FRAME = 1;
int END_FRAME = 8;
int SELECTED_SUBIMAGE = 0;
int WIDTH = 0;
int HEIGHT = 0;
std::vector<std::string> SUBIMAGES;
int SELECTED_GROUP = 0;

void layout_nodes() {
    // collect edges
    std::set<std::tuple<BaseVar*, BaseVar*>> edges;
    for (auto node : nodes) {
        for (auto dep : node->dependants) {
            edges.insert({ node, dep });
        }
    }

    std::map<int, std::set<BaseVar*>> layers;
    

    // Find Roots
    std::vector<BaseVar*> roots;
    for (auto node : nodes) roots.push_back(node);
    // collect nodes with depencies
    std::set<BaseVar*> nodes_with_dependencies;
    for (auto node : nodes) {
        for (auto dep : node->dependants) {
            nodes_with_dependencies.insert(dep);
        }
    }
    // remove nodes with dependencies from queue->keep root nodes only!
    roots.erase(std::remove_if(roots.begin(), roots.end(), [&](auto n) {
        return nodes_with_dependencies.contains(n);
    }), roots.end());

    auto Q = roots;
    std::set<BaseVar*> visited;
    int current_layer = 0;
    while (!Q.empty()) {
        auto v = Q.back(); Q.pop_back();
        layers[current_layer].insert(v);

        for (auto w : v->dependants) {
            if (!visited.contains(w)) {
                current_layer += 1;
                visited.insert(w);
                Q.push_back(w);
            }
        }
    }

    // position nodes on layers
    for (auto [u, layer]:layers) {
        int v = 0;
        for (auto node : layer) {
            node->pos = { v*150.f+100, u*120.f+300 };
            v += 1;
        }
    }

    //for (auto node : nodes) {
    //    node->pos.x = (float)rand() / RAND_MAX * 300 + 50;
    //    node->pos.y = (float)rand() / RAND_MAX * 300 + 50;
    //}
}

int main()
{

    glazy::init();
    while (glazy::is_running())
    {
        glazy::new_frame();

        if (ImGui::Begin("DAG")) {
            layout_nodes();
            auto drawlist = ImGui::GetWindowDrawList();
            drawlist->AddCallback
            auto windowpos = ImGui::GetWindowPos();

            int i = 0;
            for (auto node : nodes) {
                drawlist->AddCircleFilled({windowpos.x+node->pos.x, windowpos.y+node->pos.y}, 10, ImColor(255, 255, 255));
                drawlist->AddText({ windowpos.x+node->pos.x+10, windowpos.y+node->pos.y-20 }, ImColor(255, 255, 255), node->name.c_str());
                i++;
            }

            // draw edges
            for (auto node : nodes) {
                ImVec2 P1 = { windowpos.x + node->pos.x, windowpos.y + node->pos.y };
                for (auto dep : node->dependants) {
                    ImVec2 P2 = { windowpos.x + dep->pos.x, windowpos.y + dep->pos.y };
                    drawlist->AddLine(ImVec2(P1.x-(P1.x-P2.x)*0.2, P1.y - (P1.y - P2.y) * 0.2), P2, ImColor(200, 200, 200), 1.0);
                    
                }
            }
            
            ImGui::End();
        }

        if (ImGui::Begin("Read"))
        {
            if (ImGui::Button("open")) {
                auto filepath = glazy::open_file_dialog("EXR images (*.exr)\0*.exr\0");
                if (!filepath.empty()) {
                    auto [pattern, first, last] = scan_sequence(filepath);
                    state_pattern.set(pattern);
                    selected_subimage = 0;

                    START_FRAME = first;
                    END_FRAME = last;
                    if (state_frame.get() < START_FRAME) state_frame.set(START_FRAME);
                    if (state_frame.get() > END_FRAME) state_frame.set(END_FRAME);
                }
            }

            static int slider_val=state_frame.get();
            if (ImGui::SliderInt("frame", &slider_val, START_FRAME, END_FRAME)) {
                state_frame.set(slider_val);
            }
            ImGui::Text("state_frame: %d", state_frame.get());
            ImGui::Text("state_pattern:  %s", state_pattern.get().string().c_str());
            ImGui::Text("frame filename: %s", computed_input_frame.get().string().c_str());

            if (!subimages.get().empty()) {
                if (ImGui::BeginCombo("subimage", subimages.get()[SELECTED_SUBIMAGE].c_str())) {
                    for (auto s = 0; s < subimages.get().size(); s++) {
                        bool is_selected = s == SELECTED_SUBIMAGE;
                        if (ImGui::Selectable(subimages.get()[s].c_str(), is_selected)) {
                            SELECTED_SUBIMAGE = s;
                        }

                        if (is_selected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
            }
            

            ImGui::End();
        }


        glazy::end_frame();
    }
    glazy::destroy();
    std::cout << "Hello World!\n";
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
