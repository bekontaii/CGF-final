#pragma once
#include "weather.hpp"
#include <memory>
#include <string>
namespace ww {
struct Camera { float yaw=42.f, pitch=27.f, distance=24.f; void reset(){yaw=42.f;pitch=27.f;distance=24.f;} };
struct RenderSettings { bool shadows=true, highQuality=true; float lightAngle=-35.f; };
struct DrawStats { int objects=0, particles=0, shadowSize=0; };
class Renderer {
 struct Impl; std::unique_ptr<Impl> impl_;
 public:
 Renderer(); ~Renderer();
 void initialize(const std::string& assetRoot);
 DrawStats draw(const WeatherState&, const Camera&, const RenderSettings&, int width, int height, float time, float dt);
 bool screenshot(const std::string& path, int width, int height);
};
}
