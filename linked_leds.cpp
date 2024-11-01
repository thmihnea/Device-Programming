#include "mbed.h"
#include <algorithm>
#include <cstdint>
#include <chrono>
#include <cstdlib>
#include <memory>
#include <vector>

static InterruptIn BUTTON_INTERRUPT(BUTTON1);
static DigitalIn BUTTON_INPUT(BUTTON1);

static std::vector<DigitalOut> OUTPUTS = {
    DigitalOut(LED1), DigitalOut(LED2), DigitalOut(LED3)
};

static volatile uint64_t BUTTON_PRESS_TIME;
static volatile uint64_t SINGLE_PRESS_TIME;
static volatile uint64_t DOUBLE_PRESS_TIME;

static const uint64_t COLOUR_CHANGE_TIME = 1000;

static uint64_t get_time()
{
    auto now = Kernel::Clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

class ButtonState
{
private:
    static const uint32_t DEBOUNCE_TIME = 35;

public:
    ButtonState() {};

    bool get_state()
    {
        bool state = BUTTON_INPUT == 1 ? true : false;
        uint64_t now = get_time();

        if (now - BUTTON_PRESS_TIME > DEBOUNCE_TIME)
        {
            bool current_state = BUTTON_INPUT == 1 ? true : false;
            if (state != current_state)
            {
                state = !state;
                BUTTON_PRESS_TIME = now;
            }
        }

        return state;
    }
};

enum EventType
{
    NO_PRESS,
    SINGLE_PRESS,
    DOUBLE_PRESS
};

class EventManager
{
private:
    std::unique_ptr<ButtonState> state_controller;
    static const uint64_t DOUBLE_PRESS_TIMEOUT = 300;
    uint64_t button_down_time = 0;
    uint64_t button_up_time = 0;
    bool double_pending = false;
    bool button_down = false;

public:
    EventManager() : state_controller(std::make_unique<ButtonState>()) {};

    EventType get_event()
    {
        EventType result = NO_PRESS;
        uint64_t now = get_time();

        if (this->button_down != this->state_controller->get_state())
        {
            this->button_down = !this->button_down;
            if (this->button_down)
            {
                this->button_down_time = now;
            }
            else
            {
                this->button_up_time = now;
                if (this->double_pending)
                {
                    result = DOUBLE_PRESS;
                    this->double_pending = false;
                }
                else
                {
                    this->double_pending = true;
                }
            }
        }

        if (!this->button_down && this->double_pending && now - this->button_up_time > DOUBLE_PRESS_TIMEOUT)
        {
            this->double_pending = false;
            result = SINGLE_PRESS;
        }

        return result;
    }
};

class Node
{
private:
    std::shared_ptr<Node> next;
    int value;

public:
    Node(int value) : value(value), next(nullptr) {}

    void set_next(const std::shared_ptr<Node>& next)
    {
        this->next = next;
    }

    std::shared_ptr<Node> get_next() const
    {
        return this->next;
    }

    int get_data() const
    {
        return this->value;
    }

    void set_data(int data)
    {
        this->value = data;
    }
};

class LinkedSequence
{
private:
    std::shared_ptr<Node> head;
    std::vector<int> data;

public:
    LinkedSequence() : head(nullptr), data() {};

    LinkedSequence(std::vector<int> init_data) : head(nullptr), data(init_data)
    {
        this->convert();
    }

    void add_move(int move)
    {
        this->data.push_back(move);
    }

    void convert()
    {
        if (this->data.empty())
        {
            return;
        }

        this->head = nullptr;

        auto temp_head = std::make_shared<Node>(0);
        auto current = temp_head;

        for (size_t i = 0; i < this->data.size(); i++)
        {
            current->set_data(this->data[i]);
            if (i < this->data.size() - 1)
            {
                auto next = std::make_shared<Node>(0);
                current->set_next(next);
                current = next;
            }
            else
            {
                current->set_next(temp_head);
            }
        }

        this->head = temp_head;
        this->data.clear();
    }

    int get_data()
    {
        return this->head->get_data();
    }

    void step()
    {
        if (this->head)
        {
            this->head = this->head->get_next();
        }
    }
};

class StateController
{
private:
    std::unique_ptr<LinkedSequence> waiting;
    std::unique_ptr<LinkedSequence> current;
    uint64_t last_change = 0;
    bool editing = false;
    bool edited = false;
    uint32_t current_index = 0;

public:
    StateController()
        : waiting(std::make_unique<LinkedSequence>(std::vector<int>{0, 1, 2})),
          current(std::make_unique<LinkedSequence>()) {};

    void step()
    {
        LinkedSequence* sequence = this->editing ? this->waiting.get() : (this->edited ? this->current.get() : this->waiting.get());
        uint64_t now = get_time();
        if (now - this->last_change > COLOUR_CHANGE_TIME)
        {
            std::for_each(OUTPUTS.begin(), OUTPUTS.end(), [](auto& x) { x = 0; });
            this->current_index = sequence->get_data();
            if (this->current_index >= 0 && this->current_index < OUTPUTS.size())
            {
                OUTPUTS[this->current_index] = 1;
            }
            sequence->step();
            this->last_change = now;
        }
    }

    void switch_editing()
    {
        this->editing = !this->editing;
        if (!this->editing)
        {
            this->current->convert();
            this->edited = true;
        }
    }

    void select()
    {
        if (!this->editing) return;
        this->current->add_move(this->current_index);
    }
};

void update_time()
{
    uint64_t current_time = get_time();
    BUTTON_PRESS_TIME = current_time;
}

int main(int argc, char** argv)
{
    BUTTON_INTERRUPT.fall(update_time);
    auto event_manager = std::make_unique<EventManager>();
    auto state_controller = std::make_unique<StateController>();

    while (true)
    {
        EventType event = event_manager->get_event();
        switch (event)
        {
            case SINGLE_PRESS:
            {
                state_controller->select();
                break;
            }

            case DOUBLE_PRESS:
            {
                state_controller->switch_editing();
                break;
            }

            default:
            {
                break;
            }
        }
        uint64_t now = get_time();
        state_controller->step();
        ThisThread::sleep_for(10ms);
    }

    return EXIT_SUCCESS;
}
