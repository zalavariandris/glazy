#pragma once

// std data structures
#include <set>
#include <vector>
#include <string>
#include <map>

#include "imgui.h"

/******************
 * Reactive Store *
 ******************/
class BaseVar {
protected:
    mutable bool dirty = true;
public:
    mutable std::set<BaseVar*> dependants;
    std::string name;
    ImVec2 pos = { 0,0 };
    void set_dirty(bool val = true) {
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
    State(T val, std::string _name = "") :cache(val) {
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
class Computed : public BaseVar {
private:
    mutable T cache;
    std::function<T()> evaluate;
public:
    Computed(std::function<T()> f, std::string _name = "") : evaluate(f) {
        nodes.insert(this);
        name = _name;
    };

    const T get() const {
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

// widget helpers
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
    for (auto [u, layer] : layers) {
        int v = 0;
        for (auto node : layer) {
            node->pos = { v * 150.f + 50, u * 120.f + 50 };
            v += 1;
        }
    }

    //for (auto node : nodes) {
    //    node->pos.x = (float)rand() / RAND_MAX * 300 + 50;
    //    node->pos.y = (float)rand() / RAND_MAX * 300 + 50;
    //}
}