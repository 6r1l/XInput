// c++17

#include <array>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <wchar.h>
#include <windows.h>
#include <xinput.h>

#pragma comment(lib, "XInput.lib")

#define XUSER_DEFAULT                   0
#define XINPUT_GAMEPAD_LEFT_TRIGGER     0x10000
#define XINPUT_GAMEPAD_RIGHT_TRIGGER    0x20000
#define FRAME_RATE                      20


class XController {
private:
    BYTE xuser;
    DWORD last_packet;
    uint32_t button_to_release;
public:
    XController(int);
    boolean is_connected();
    void vibrate(uint16_t, int);
    uint32_t get_buttons();
    void set_button_to_release(uint32_t);
};


XController::XController(int xuser) {
    this->xuser = xuser;
    this->last_packet = NULL;
    this->button_to_release = NULL;
}


boolean XController::is_connected() {
    XINPUT_STATE state;
    SecureZeroMemory(&state, sizeof(XINPUT_STATE));

    DWORD dwResult = XInputGetState(XUSER_DEFAULT, &state);

    return dwResult == ERROR_SUCCESS;
}


void XController::vibrate(uint16_t power, int milliseconds) {
    XINPUT_VIBRATION vibration;
    SecureZeroMemory(&vibration, sizeof(XINPUT_VIBRATION));

    vibration.wLeftMotorSpeed = power; // use any value between 0-65535 here
    vibration.wRightMotorSpeed = power; // use any value between 0-65535 here

    XInputSetState(xuser, &vibration);

    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));

    SecureZeroMemory(&vibration, sizeof(XINPUT_VIBRATION));
    XInputSetState(this->xuser, &vibration);
}


uint32_t XController::get_buttons() {
    XINPUT_STATE state;
    SecureZeroMemory(&state, sizeof(XINPUT_STATE));

    XInputGetState(XUSER_DEFAULT, &state);
    
    if (this->last_packet == state.dwPacketNumber)
        return NULL;
    
    uint32_t buttons = state.Gamepad.wButtons;

    if (state.Gamepad.bLeftTrigger)
        buttons |= XINPUT_GAMEPAD_LEFT_TRIGGER;
    if (state.Gamepad.bRightTrigger)
        buttons |= XINPUT_GAMEPAD_RIGHT_TRIGGER;

    if (this->button_to_release)
    {
        if (buttons & this->button_to_release)
            return NULL;
        else
            this->button_to_release = NULL;
    }

    return buttons;
}


void XController::set_button_to_release(uint32_t button_to_release) {
    this->button_to_release = button_to_release;
}


uint32_t get_random_button(std::uniform_int_distribution<> *distr, std::mt19937 *gen) {
    std::array<int, 8> buttons = {
        XINPUT_GAMEPAD_LEFT_SHOULDER,   XINPUT_GAMEPAD_RIGHT_SHOULDER,
        XINPUT_GAMEPAD_A,               XINPUT_GAMEPAD_B,
        XINPUT_GAMEPAD_X,               XINPUT_GAMEPAD_Y,
        XINPUT_GAMEPAD_LEFT_TRIGGER,    XINPUT_GAMEPAD_RIGHT_TRIGGER
    };
    return buttons[(*distr)(*gen)];
}

std::string button_str(uint32_t button) {
    if (button & XINPUT_GAMEPAD_LEFT_SHOULDER)
        return "LS";
    else if (button & XINPUT_GAMEPAD_RIGHT_SHOULDER)
        return "RS";
    else if (button & XINPUT_GAMEPAD_A)
        return "A";
    else if (button & XINPUT_GAMEPAD_B)
        return "B";
    else if (button & XINPUT_GAMEPAD_X)
        return "X";
    else if (button & XINPUT_GAMEPAD_Y)
        return "Y";
    else if (button & XINPUT_GAMEPAD_LEFT_TRIGGER)
        return "LT";
    else if (button & XINPUT_GAMEPAD_RIGHT_TRIGGER)
        return "RT";
    return NULL;
}


int main(int, char**) {
    // console to virtual terminal
    auto hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    std::cout << "XInput!" << std::endl;

    auto xcontroller = XController(XUSER_DEFAULT);

    if (xcontroller.is_connected())
    {
        std::thread(&XController::vibrate, &xcontroller, 30000, 600).detach();
        std::cout << "\033[1;32m" << "Connected" << "\033[0m" << std::endl;
    }
    else 
    {
        std::cout << "\033[1;31m" << "Disconnected" << "\033[0m" << std::endl;
        exit(1);
    }

    std::random_device rd; // seed for the random number engine
    std::mt19937 gen(rd()); // standard mersenne_twister_engine seeded with rd()
    std::uniform_int_distribution<> distrib(0, 7); // range

    auto frame_gap_ms = 1000 / FRAME_RATE;
    auto next_frame = std::chrono::steady_clock::now();
    auto button_miss = 0;
    auto button_found = 0;
    auto button_to_find = get_random_button(&distrib, &gen);
    std::cout << button_str(button_to_find);

    while (true) {
        next_frame += std::chrono::milliseconds(frame_gap_ms);

        uint32_t buttons = xcontroller.get_buttons();        
        if (buttons)
        {
            if (buttons & XINPUT_GAMEPAD_BACK)
            {
                std::cout << std::endl;
                break;
            }

            auto spaces = std::string(3 - button_str(button_to_find).size(), ' ');
            if (buttons == button_to_find)
            {
                button_found++;
                std::cout << spaces << "\033[1;32m" << "OK" << "\033[0m" << std::endl;
                button_to_find = get_random_button(&distrib, &gen);
            }
            else
            {
                button_miss++;
                std::thread(&XController::vibrate, &xcontroller, 30000, 200).detach();
                std::cout << spaces << "\033[1;31m" << "KO" << "\033[0m" << std::endl;
            }
            std::cout << button_str(button_to_find);
            xcontroller.set_button_to_release(buttons);
        }

        std::this_thread::sleep_until(next_frame);
    }

    std::cout << "Errors: " << button_miss << "/" << button_found << std::endl;
}
