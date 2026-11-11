#pragma once
#include <future>
#include <optional>
#include <string>
#include <vector>
namespace ww {
enum class WeatherKind { Clear, Cloudy, Rain, Snow, Storm, Fog };
struct City { std::string name; double latitude, longitude; };
struct WeatherState {
 WeatherKind kind = WeatherKind::Clear;
 float temperature = 18.f, windSpeed = 8.f, cloudCover = 15.f, precipitation = 0.f;
 bool isDay = true;
 std::string city = "Astana", time = "Simulation";
};
struct WeatherResult { std::optional<WeatherState> data; std::string error; };
const char* weatherName(WeatherKind kind);
WeatherKind classifyWeather(int code);
WeatherState preset(WeatherKind kind, bool day = true);
const std::vector<City>& cities();
WeatherState parseWeather(const std::string& json, const City& city);
WeatherResult fetchWeather(const City& city);
class WeatherClient {
 std::future<WeatherResult> pending_;
 public:
 bool request(const City& city);
 bool busy() const;
 std::optional<WeatherResult> poll();
};
// Six 15-second rounds. Match weather and day/night, then submit.
class Challenge {
 public:
 void start(); void reset(); void update(float dt); bool submit(WeatherKind kind, bool isDay);
 bool active() const; bool finished() const; int round() const; int score() const;
 float remaining() const; WeatherKind targetKind() const; bool targetDay() const;
 const std::string& feedback() const;
 private:
 int round_ = 0, score_ = 0; float remaining_ = 15.f; bool active_ = false, finished_ = false, answered_ = false;
 std::string feedback_;
};
}
