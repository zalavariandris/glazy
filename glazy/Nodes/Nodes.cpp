// Nodes.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>

#include <vector>
#include <functional>
#include <any>

class Inlet
{
public:
    std::vector<std::function<void(std::any)>> handles;

    void onTrigger(std::function<void(std::any)> handle)
    {
        handles.push_back(handle);
    }
};

class Outlet {
private:
    std::any value;
public:
    std::any value;
    std::vector<Inlet> targets;

    void target(const Inlet& inlet) {
        targets.push_back(inlet);
        for (const auto& handle : inlet.handles) {
            handle(value);
        }
    }

    void trigger(std::any props) {
        value = props;
        for (const auto& inlet : targets) {
            for (const auto& handle : inlet.handles)
            {
                handle(value);
            }
        }
    }
};


int main()
{
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
