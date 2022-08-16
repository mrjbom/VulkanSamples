#pragma once

#include <../imgui/imgui.h>
#include <chrono>

namespace UIOverlay
{
    // Do ImGui::Begin()
    // Set window size
    // Set window pos
    // Unmovable and unsizable by default
    inline bool windowBegin(const char* name, bool* open, const ImVec2& pos, const ImVec2& size, ImGuiWindowFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize)
    {
        bool result = ImGui::Begin(name, open, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
        ImGui::SetWindowPos(pos);
        ImGui::SetWindowSize(size);
        return result;
    }

    // Do ImGui::End()
    inline void windowEnd()
    {
        ImGui::End();
    }

    // Displays the FPS number using the frametime value in milliseconds.
    // Refreshes the text not every frame, but with a certain frequency(so that the values do not flicker)
    inline void printFPS(float frameTime, float textUpdateFrequencyInMS)
    {
        static float timeElapsed = 0;
        timeElapsed += frameTime;
        static float nextValidElapsedTime = textUpdateFrequencyInMS;
        static int FPS = 0;
        static float tmpFrameTime = 0;

        if (timeElapsed > (int)nextValidElapsedTime) {
            FPS = int(1000 / frameTime);
            tmpFrameTime = frameTime;
            nextValidElapsedTime += textUpdateFrequencyInMS;
        }

        ImGui::Text("%i FPS (%.3f ms)", FPS, tmpFrameTime);
    }
}
