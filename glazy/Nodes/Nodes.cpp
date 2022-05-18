// Nodes.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>

#include <vector>
#include <functional>
#include <any>

#include "glazy.h"
#include "FileSequence.h";

template<typename T> class MyInlet;
template<typename T> class MyOutlet;

class Node;

template <typename T>
class MyInlet
{

public:
    void onTrigger(std::function<void(T)> handler)
    {
        handlers.push_back(handler);
    }

    template<typename T> friend class MyOutlet;
    ~MyInlet() {
        for (MyOutlet<T>* outlet : sources)
        {
            auto it = std::find(outlet->targets.begin(), outlet->targets.end(), this);
            if (it!=outlet->targets.end()) {
                outlet->targets.erase(it);
            }
        }
    }
private:
    std::vector<std::function<void(T)>> handlers;
    std::vector<MyOutlet<T>*> sources;
    friend class MyOutlet<T>;


};


template <typename T>
class MyOutlet {
private:
    T value;
    std::vector<const MyInlet<T>*> targets;
public:
    

    void target(const MyInlet<T>& inlet) {
        targets.push_back(&inlet);
        //inlet.sources.push_back(this);
        for (const auto& handler : inlet.handlers)
        {
            handler(value);
        }
    }

    void trigger(T props) {
        value = props;
        for (const auto& inlet : targets) {
            for (const auto& handler : inlet->handlers)
            {
                handler(value);
            }
        }
    }

    //~MyOutlet() {
    //    for (const MyInlet<T>* inlet : targets)
    //    {
    //        auto it = std::find(inlet->sources.begin(), inlet->sources.end(), this);
    //        if (it != inlet->sources.end()) {
    //            inlet->sources.erase(it);
    //        }
    //    }
    //}

    friend class MyInlet<T>;
};

#include <filesystem>
#include <tuple>


template <typename T>
class Attribute {
    T value;
    std::vector<std::function<void(T)>> handlers;
public:
    void set(T val) {
        for (const auto& handler : handlers) {
            handler(val);
            value = val;
        }
    }

    const T& get()
    {
        return value;
    }

    void onChange(std::function<void(T)> handler)
    {
        handlers.push_back(handler);
    }
};

class FilesequenceNode
{
    FileSequence sequence;

public:
    Attribute<std::filesystem::path> pattern;
    Attribute<int> frame;
    MyOutlet<std::filesystem::path> out;

    FilesequenceNode()
    {
        pattern.onChange([&](auto filepath){
            evaluate();
        });

        frame.onChange([&](int F) {
            evaluate();
        });
    }

    void evaluate() {
        auto result = sequence.item(frame.get());
        out.trigger(result);
    }

    void onGui()
    {
        if(ImGui::Button("open"))
        {
            auto filename = glazy::open_file_dialog("EXR images (*.exr)\0*.exr\0JPEG images\0*.jpg");
            sequence = FileSequence(filename);
            frame.set(sequence.first_frame);
            pattern.set( sequence.pattern );
        }
        ImGui::LabelText("pattern", "%s", pattern.get().string().c_str());

        int _frame_ = frame.get();
        if (ImGui::SliderInt("frame", &_frame_, 1, 9)) {
            frame.set(_frame_);
        }
        //ImGui::LabelText("frame", "%d", frame.get());
    }
};

class ReadNode
{
private:
    std::filesystem::path filename;
    void* memory=NULL;
public:
    MyInlet<std::filesystem::path> filename_in;
    MyInlet<void*> memory_in;
    MyOutlet<std::tuple<void*,int, int, int,int>> plate_out;

    ReadNode()
    {
        filename_in.onTrigger([this](auto path) {
            filename = path;
            read();
        });

        memory_in.onTrigger([this](auto buffer) {
            memory = buffer;
            read();
        });
    }

    void read()
    {
        if (memory == NULL) return;
        if (!std::filesystem::exists(filename)) return;

        // read file to pixels

        plate_out.trigger({ memory,0,0,512,512 });
    }

    void onGUI() {
        ImGui::LabelText("filename", "%s", filename.string().c_str());
    }
};

class ViewportNode {
    MyInlet<std::tuple<void*, int, int,int,int>> image_in;

    ViewportNode() {
        image_in.onTrigger([](auto plate) {
            auto [ptr, x, y, w, h] = plate;
        });
    }
};



auto read_node = ReadNode();
auto filesequence_node = FilesequenceNode();
int main()
{
    auto outlet = MyOutlet<int>();
    auto inlet = MyInlet<int>();
    
    inlet.onTrigger([](int props) {
        std::cout << "inlet got value: " << props << "\n";
    });

    outlet.target(inlet);
    outlet.trigger(5);
    outlet.trigger(10);
    
    std::cout << "Hello World!\n";

    filesequence_node.out.target(read_node.filename_in);
    glazy::init();
    while (glazy::is_running()) {
        glazy::new_frame();

        filesequence_node.onGui();
        read_node.onGUI();
        glazy::end_frame();
    }
    glazy::destroy();

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
