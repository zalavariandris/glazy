#pragma once
#include <functional>

namespace Nodes
{
    template<typename T> class Inlet;
    template<typename T> class Outlet;


    class Node;


    template <typename T>
    class Inlet
    {

    public:
        std::string name;
        Inlet(std::string name):name(name){}

        void onTrigger(std::function<void(T)> handler)
        {
            handlers.push_back(handler);
        }

        template<typename T> friend class Outlet;
        ~Inlet() {
            for (Outlet<T>* outlet : sources)
            {
                auto it = std::find(outlet->_targets.begin(), outlet->_targets.end(), this);
                if (it != outlet->_targets.end()) {
                    outlet->_targets.erase(it);
                }
            }
        }
    private:
        std::vector<std::function<void(T)>> handlers;
        std::vector<Outlet<T>*> sources;
        friend class Outlet<T>;
    };

    template <typename T>
    class Outlet {
    private:
        T value;
        std::vector<const Inlet<T>*> _targets;
    public:


        void target(const Inlet<T>& inlet) {
            _targets.push_back(&inlet);
            //inlet.sources.push_back(this);
            for (const auto& handler : inlet.handlers)
            {
                handler(value);
            }
        }

        void trigger(T props) {
            value = props;
            for (const auto& inlet : _targets) {
                for (const auto& handler : inlet->handlers)
                {
                    handler(value);
                }
            }
        }

        const std::vector<const Inlet<T>*>& targets() {
            return _targets;
        }

        void reorder_target(int oldIndex, int newIndex)
        {
            assert(("selected index is out of range!", oldIndex >= 0 && oldIndex < _targets.size()));
            assert(("target index is out of range!", newIndex >= 0 && newIndex < _targets.size()));
            if (oldIndex == newIndex) return;

            // perform reorder
            if (oldIndex > newIndex)
                std::rotate(_targets.rend() - oldIndex - 1, _targets.rend() - oldIndex, _targets.rend() - newIndex);
            else
                std::rotate(_targets.begin() + oldIndex, _targets.begin() + oldIndex + 1, _targets.begin() + newIndex + 1);
        }

        //~Outlet() {
        //    for (const Inlet<T>* inlet : targets)
        //    {
        //        auto it = std::find(inlet->sources.begin(), inlet->sources.end(), this);
        //        if (it != inlet->sources.end()) {
        //            inlet->sources.erase(it);
        //        }
        //    }
        //}

        friend class Inlet<T>;
    };



    template <typename T>
    class Attribute {
        T value;
        std::vector<std::function<void(T)>> handlers;
    public:
        Attribute() {}

        Attribute(T val) :value(val) {

        }

        void set(T val) {
            for (const auto& handler : handlers) {
                handler(val);
            }
            value = val;
        }

        const T& get() const
        {
            return value;
        }

        void onChange(std::function<void(T)> handler)
        {
            handlers.push_back(handler);
        }
    };

}
