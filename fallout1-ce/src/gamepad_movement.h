#ifndef FALLOUT_GAMEPAD_MOVEMENT_H_
#define FALLOUT_GAMEPAD_MOVEMENT_H_

#include <algorithm>
#include <cmath>

namespace fallout {

// Error diffusion alternates neighboring hexes for vertical/diagonal input,
// instead of locking those directions to one side of the isometric grid.
struct GamepadSteering {
    float crossTrackError = 0;
    float lastX = 0, lastY = 0;

    int choose(float x, float y, const int dx[6], const int dy[6])
    {
        float length = std::sqrt(x * x + y * y);
        if (length == 0) { reset(); return -1; }
        x /= length;
        y /= length;
        if (x * lastX + y * lastY < 0.8f) crossTrackError = 0;
        lastX = x;
        lastY = y;
        int best = -1;
        float bestDistance = 1e9f;
        for (int i = 0; i < 6; ++i) {
            float stepLength = std::sqrt(float(dx[i] * dx[i] + dy[i] * dy[i]));
            if (stepLength == 0 || x * dx[i] + y * dy[i] < -stepLength * 0.5f) continue;
            float error = crossTrackError + x * dy[i] - y * dx[i];
            float turn = stepLength - (x * dx[i] + y * dy[i]);
            float distance = error * error + turn * turn * 0.15f;
            if (distance < bestDistance) { best = i; bestDistance = distance; }
        }
        return best;
    }

    void commit(int dx, int dy)
    {
        crossTrackError = std::clamp(crossTrackError + lastX * dy - lastY * dx, -40.0f, 40.0f);
    }

    void reset() { crossTrackError = lastX = lastY = 0; }
};

// Pixel-level follow with a quiet center region, bounded speed, and fractional
// accumulation. It does not snap the map to each newly entered hex.
struct GamepadCamera {
    float remainderX = 0, remainderY = 0;
    float panVelocityX = 0, panVelocityY = 0;
    float followDelay = 0;

    void step(float errorX, float errorY, float seconds, int& dx, int& dy,
        float panX = 0, float panY = 0, bool follow = true)
    {
        seconds = std::clamp(seconds, 0.0f, 0.05f);
        // Manual panning has priority even when the player is also walking.
        // Ease its velocity, then give it a brief quiet interval before follow
        // resumes. A stationary player leaves the inspected view in place.
        if (panX || panY) followDelay = 0.5f;
        else followDelay = std::max(0.0f, followDelay - seconds);
        float panFactor = 1.0f - std::exp(-18.0f * seconds);
        panVelocityX += (panX - panVelocityX) * panFactor;
        panVelocityY += (panY - panVelocityY) * panFactor;
        if (!panX && !panY && std::hypot(panVelocityX, panVelocityY) < 2) {
            panVelocityX = panVelocityY = 0;
        }
        auto outside = [](float error, float radius) {
            return error > radius ? error - radius : error < -radius ? error + radius : 0.0f;
        };
        float factor = 1.0f - std::exp(-8.0f * seconds);
        float x = follow ? outside(errorX, 24) * factor : 0;
        float y = follow ? outside(errorY, 18) * factor : 0;
        float length = std::sqrt(x * x + y * y);
        float limit = 240 * seconds;
        if (length > limit && length > 0) { x *= limit / length; y *= limit / length; }
        if (followDelay > 0 || panX || panY) {
            x = panVelocityX * seconds;
            y = panVelocityY * seconds;
        }
        remainderX += x;
        remainderY += y;
        dx = static_cast<int>(remainderX);
        dy = static_cast<int>(remainderY);
        remainderX -= dx;
        remainderY -= dy;
    }

    void blocked() { remainderX = remainderY = panVelocityX = panVelocityY = 0; }
    void reset() { blocked(); followDelay = 0; }
};

} // namespace fallout
#endif
