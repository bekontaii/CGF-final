#include "weather.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <locale>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <utility>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winhttp.h>
#else
#include <curl/curl.h>
#endif

namespace ww {
namespace {
constexpr std::size_t maxResponseBytes = 1024 * 1024;
constexpr std::array<WeatherKind, 6> challengeWeather = {
    WeatherKind::Clear, WeatherKind::Rain, WeatherKind::Snow,
    WeatherKind::Storm, WeatherKind::Cloudy, WeatherKind::Fog
};
constexpr std::array<bool, 6> challengeDay = {true, false, true, false, true, false};

void validateCity(const City& city) {
    if (!std::isfinite(city.latitude) || !std::isfinite(city.longitude) ||
        city.latitude < -90. || city.latitude > 90. ||
        city.longitude < -180. || city.longitude > 180.) {
        throw std::invalid_argument("City coordinates must be finite and within latitude/longitude bounds.");
    }
    if (city.name.empty() || city.name.size() > 120) {
        throw std::invalid_argument("City name must contain 1 to 120 characters.");
    }
}

float requiredNumber(const nlohmann::json& object, const char* name, float low, float high) {
    if (!object.contains(name) || !object.at(name).is_number()) {
        throw std::runtime_error(std::string("Weather response has no valid ") + name + ".");
    }
    const double value = object.at(name).get<double>();
    if (!std::isfinite(value) || value < low || value > high) {
        throw std::runtime_error(std::string("Weather response has an out-of-range ") + name + ".");
    }
    return static_cast<float>(value);
}

int requiredInteger(const nlohmann::json& object, const char* name, int low, int high) {
    if (!object.contains(name) || !object.at(name).is_number()) {
        throw std::runtime_error(std::string("Weather response has no valid ") + name + ".");
    }
    const double value = object.at(name).get<double>();
    if (!std::isfinite(value) || value < low || value > high || std::floor(value) != value) {
        throw std::runtime_error(std::string("Weather response has a non-integer ") + name + ".");
    }
    return static_cast<int>(value);
}

std::string requestPath(const City& city) {
    std::ostringstream path;
    path.imbue(std::locale::classic());
    path << std::fixed << std::setprecision(6)
         << "/v1/forecast?latitude=" << city.latitude << "&longitude=" << city.longitude
         << "&current=temperature_2m,weather_code,is_day,cloud_cover,wind_speed_10m,precipitation"
         << "&temperature_unit=celsius&wind_speed_unit=kmh&precipitation_unit=mm&timezone=auto";
    return path.str();
}

#ifdef _WIN32
struct InternetDeleter { void operator()(void* handle) const { if (handle) WinHttpCloseHandle(handle); } };
using InternetHandle = std::unique_ptr<void, InternetDeleter>;
[[noreturn]] void winError(const char* operation) {
    throw std::runtime_error(std::string(operation) + " failed (Windows error " + std::to_string(GetLastError()) + ").");
}
std::string httpGet(const std::string& path) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
    InternetHandle session(WinHttpOpen(L"WeatherWorld/1.0 educational project",
        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session) winError("HTTP initialization");
    if (!WinHttpSetTimeouts(session.get(), 4000, 4000, 5000, 5000)) winError("HTTP timeout setup");
    InternetHandle connection(WinHttpConnect(session.get(), L"api.open-meteo.com", INTERNET_DEFAULT_HTTPS_PORT, 0));
    if (!connection) winError("HTTPS connection");
    const std::wstring widePath(path.begin(), path.end());
    InternetHandle request(WinHttpOpenRequest(connection.get(), L"GET", widePath.c_str(), nullptr,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE));
    if (!request) winError("HTTPS request creation");
    DWORD disabled = WINHTTP_DISABLE_REDIRECTS;
    if (!WinHttpSetOption(request.get(), WINHTTP_OPTION_DISABLE_FEATURE, &disabled, sizeof(disabled))) winError("HTTP redirect policy");
    // Default WinHTTP certificate and hostname checks remain enabled.
    if (!WinHttpSendRequest(request.get(), L"Accept: application/json\r\n", static_cast<DWORD>(-1),
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) winError("HTTPS send");
    if (!WinHttpReceiveResponse(request.get(), nullptr)) winError("HTTPS receive");
    DWORD status = 0, statusSize = sizeof(status);
    if (!WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX)) winError("HTTP status check");
    if (status != 200) throw std::runtime_error("Open-Meteo returned HTTP " + std::to_string(status) + ".");
    std::string body;
    std::array<char, 8192> buffer{};
    while (true) {
        if (std::chrono::steady_clock::now() >= deadline) throw std::runtime_error("Weather request timed out.");
        DWORD received = 0;
        if (!WinHttpReadData(request.get(), buffer.data(), static_cast<DWORD>(buffer.size()), &received)) winError("HTTPS body read");
        if (received == 0) break;
        if (body.size() + received > maxResponseBytes) throw std::runtime_error("Weather response exceeds the 1 MB limit.");
        body.append(buffer.data(), received);
    }
    return body;
}
#else
struct CurlGlobal {
    CurlGlobal() { if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) throw std::runtime_error("Unable to initialize HTTPS library."); }
    ~CurlGlobal() { curl_global_cleanup(); }
};
struct CurlDeleter { void operator()(CURL* curl) const { if (curl) curl_easy_cleanup(curl); } };
struct ResponseBuffer { std::string body; bool exceeded = false; };
std::size_t receiveBody(char* data, std::size_t size, std::size_t count, void* user) noexcept {
    auto& buffer = *static_cast<ResponseBuffer*>(user);
    if (size != 0 && count > std::numeric_limits<std::size_t>::max() / size) return 0;
    const std::size_t bytes = size * count;
    if (bytes > maxResponseBytes - buffer.body.size()) { buffer.exceeded = true; return 0; }
    try { buffer.body.append(data, bytes); } catch (...) { return 0; }
    return bytes;
}
std::string httpGet(const std::string& path) {
    static CurlGlobal global;
    std::unique_ptr<CURL, CurlDeleter> curl(curl_easy_init());
    if (!curl) throw std::runtime_error("Unable to create HTTPS request.");
    ResponseBuffer buffer;
    std::array<char, CURL_ERROR_SIZE> error{};
    const std::string url = "https://api.open-meteo.com" + path;
    curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_USERAGENT, "WeatherWorld/1.0 educational project");
    curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT_MS, 4000L);
    curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT_MS, 15000L);
    curl_easy_setopt(curl.get(), CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, receiveBody);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &buffer);
    curl_easy_setopt(curl.get(), CURLOPT_ERRORBUFFER, error.data());
    const CURLcode result = curl_easy_perform(curl.get());
    if (buffer.exceeded) throw std::runtime_error("Weather response exceeds the 1 MB limit.");
    if (result != CURLE_OK) {
        const std::string detail = error[0] ? error.data() : curl_easy_strerror(result);
        throw std::runtime_error("Weather request failed: " + detail);
    }
    long status = 0;
    curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &status);
    if (status != 200) throw std::runtime_error("Open-Meteo returned HTTP " + std::to_string(status) + ".");
    return buffer.body;
}
#endif
} // namespace

const char* weatherName(WeatherKind kind) {
    switch (kind) {
        case WeatherKind::Clear: return "Clear";
        case WeatherKind::Cloudy: return "Cloudy";
        case WeatherKind::Rain: return "Rain";
        case WeatherKind::Snow: return "Snow";
        case WeatherKind::Storm: return "Thunderstorm";
        case WeatherKind::Fog: return "Fog";
    }
    return "Unknown";
}

WeatherKind classifyWeather(int code) {
    switch (code) {
        case 0: case 1: return WeatherKind::Clear;
        case 2: case 3: return WeatherKind::Cloudy;
        case 45: case 48: return WeatherKind::Fog;
        case 51: case 53: case 55: case 56: case 57:
        case 61: case 63: case 65: case 66: case 67:
        case 80: case 81: case 82: return WeatherKind::Rain;
        case 71: case 73: case 75: case 77: case 85: case 86: return WeatherKind::Snow;
        case 95: case 96: case 99: return WeatherKind::Storm;
        default: throw std::invalid_argument("Unsupported WMO weather code: " + std::to_string(code) + ".");
    }
}

WeatherState preset(WeatherKind kind, bool day) {
    WeatherState state;
    state.kind = kind;
    state.isDay = day;
    state.time = "Simulation";
    switch (kind) {
        case WeatherKind::Clear: state.temperature=22.f; state.windSpeed=6.f; state.cloudCover=8.f; break;
        case WeatherKind::Cloudy: state.temperature=14.f; state.windSpeed=16.f; state.cloudCover=88.f; break;
        case WeatherKind::Rain: state.temperature=9.f; state.windSpeed=22.f; state.cloudCover=96.f; state.precipitation=3.f; break;
        case WeatherKind::Snow: state.temperature=-5.f; state.windSpeed=12.f; state.cloudCover=85.f; state.precipitation=1.5f; break;
        case WeatherKind::Storm: state.temperature=11.f; state.windSpeed=42.f; state.cloudCover=100.f; state.precipitation=9.f; break;
        case WeatherKind::Fog: state.temperature=7.f; state.windSpeed=3.f; state.cloudCover=98.f; break;
        default: throw std::invalid_argument("Invalid simulation preset.");
    }
    if (!day) state.temperature -= 5.f;
    return state;
}

const std::vector<City>& cities() {
    static const std::vector<City> values = {
        {"Astana",51.1694,71.4491}, {"Almaty",43.2389,76.8897},
        {"London",51.5074,-0.1278}, {"Tokyo",35.6762,139.6503},
        {"New York",40.7128,-74.0060}, {"Paris",48.8566,2.3522},
        {"Reykjavik",64.1466,-21.9426}, {"Sydney",-33.8688,151.2093}
    };
    return values;
}

WeatherState parseWeather(const std::string& text, const City& city) {
    validateCity(city);
    if (text.empty() || text.size() > maxResponseBytes) throw std::runtime_error("Empty or oversized weather response.");
    const auto root = nlohmann::json::parse(text);
    if (!root.is_object()) throw std::runtime_error("Weather response must be a JSON object.");
    if (root.value("error",false)) {
        const auto reason = root.find("reason");
        throw std::runtime_error("Open-Meteo error: " + ((reason != root.end() && reason->is_string()) ? reason->get<std::string>() : "request rejected"));
    }
    if (!root.contains("current") || !root.at("current").is_object()) throw std::runtime_error("Weather response has no current conditions.");
    const auto& current = root.at("current");
    WeatherState state;
    state.kind = classifyWeather(requiredInteger(current,"weather_code",0,99));
    state.temperature = requiredNumber(current,"temperature_2m",-100.f,80.f);
    state.windSpeed = requiredNumber(current,"wind_speed_10m",0.f,500.f);
    state.cloudCover = requiredNumber(current,"cloud_cover",0.f,100.f);
    state.precipitation = requiredNumber(current,"precipitation",0.f,1000.f);
    state.isDay = requiredInteger(current,"is_day",0,1) == 1;
    if (!current.contains("time") || !current.at("time").is_string()) throw std::runtime_error("Weather response has no observation time.");
    state.time = current.at("time").get<std::string>();
    if (state.time.empty() || state.time.size() > 64) throw std::runtime_error("Weather observation time is invalid.");
    if (root.contains("timezone") && root.at("timezone").is_string()) state.time += " " + root.at("timezone").get<std::string>();
    state.city = city.name;
    return state;
}

WeatherResult fetchWeather(const City& city) {
    try {
        validateCity(city);
        return {parseWeather(httpGet(requestPath(city)),city),{}};
    } catch (const std::exception& error) {
        return {std::nullopt,error.what()};
    } catch (...) {
        return {std::nullopt,"Unexpected weather request failure."};
    }
}

bool WeatherClient::request(const City& city) {
    if (pending_.valid()) return false;
    pending_ = std::async(std::launch::async,[city] { return fetchWeather(city); });
    return true;
}
bool WeatherClient::busy() const {
    return pending_.valid();
}
std::optional<WeatherResult> WeatherClient::poll() {
    if (!pending_.valid() || pending_.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) return std::nullopt;
    return pending_.get();
}

void Challenge::start() {
    round_ = 0; score_ = 0; remaining_ = 15.f; active_ = true; finished_ = false; answered_ = false;
    feedback_ = "Match the target, then submit. One attempt per round.";
}
void Challenge::reset() {
    round_ = 0; score_ = 0; remaining_ = 15.f; active_ = false; finished_ = false; answered_ = false;
    feedback_.clear();
}
void Challenge::update(float dt) {
    if (!active_ || !std::isfinite(dt) || dt <= 0.f) return;
    remaining_ -= dt;
    while (remaining_ <= 0.f && active_) {
        ++round_;
        if (round_ >= static_cast<int>(challengeWeather.size())) {
            active_ = false; finished_ = true; remaining_ = 0.f;
            feedback_ = score_ >= 400 ? "Challenge complete! Great forecasting." : "Challenge complete. Try again to reach 400 points.";
        } else {
            remaining_ += 15.f; answered_ = false;
            feedback_ = "New target. Match the weather and day or night.";
        }
    }
}
bool Challenge::submit(WeatherKind kind, bool day) {
    if (!active_ || answered_) return false;
    answered_ = true;
    const bool correct = kind == targetKind() && day == targetDay();
    if (correct) { score_ += 100; feedback_ = "Correct! +100 points. Next target when the timer ends."; }
    else { feedback_ = "Missed. Match both the weather and day or night next round."; }
    return correct;
}
bool Challenge::active() const { return active_; }
bool Challenge::finished() const { return finished_; }
int Challenge::round() const { return (active_ || finished_) ? std::min(round_+1,6) : 0; }
int Challenge::score() const { return score_; }
float Challenge::remaining() const { return remaining_; }
WeatherKind Challenge::targetKind() const { return challengeWeather[std::min(std::max(round_,0),5)]; }
bool Challenge::targetDay() const { return challengeDay[std::min(std::max(round_,0),5)]; }
const std::string& Challenge::feedback() const { return feedback_; }
} // namespace ww
