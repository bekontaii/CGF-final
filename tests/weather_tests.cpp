#include "weather.hpp"
#include <nlohmann/json.hpp>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {
int checks = 0;
void check(bool condition, const std::string& message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}
template<class F> void rejects(F function, const std::string& message) {
    bool threw = false;
    try { function(); } catch (const std::exception&) { threw = true; }
    check(threw,message);
}
nlohmann::json fixture() {
    return {
        {"timezone","Asia/Almaty"},
        {"current", {{"temperature_2m",-4.5},{"weather_code",73},{"is_day",0},
            {"cloud_cover",92},{"wind_speed_10m",16.2},{"precipitation",0.4},
            {"time","2026-11-05T10:15"}}}
    };
}
}
int main() {
    using namespace ww;
    try {
        const std::vector<std::pair<WeatherKind,std::vector<int>>> groups = {
            {WeatherKind::Clear,{0,1}}, {WeatherKind::Cloudy,{2,3}}, {WeatherKind::Fog,{45,48}},
            {WeatherKind::Rain,{51,53,55,56,57,61,63,65,66,67,80,81,82}},
            {WeatherKind::Snow,{71,73,75,77,85,86}}, {WeatherKind::Storm,{95,96,99}}
        };
        for (const auto& group:groups) for (int code:group.second) check(classifyWeather(code)==group.first,"Wrong WMO mapping for "+std::to_string(code));
        for (int code : {-1,4,44,49,58,69,78,90,94,97,100}) rejects([code]{classifyWeather(code);},"Unsupported WMO accepted");
        auto input=fixture();
        const auto weather=parseWeather(input.dump(),cities().front());
        check(weather.kind==WeatherKind::Snow,"Snow fixture classification");
        check(weather.temperature==-4.5f && !weather.isDay,"Fixture temperature/day");
        check(weather.city=="Astana" && weather.time=="2026-11-05T10:15 Asia/Almaty","City/time source attribution");
        check(std::abs(weather.windSpeed-16.2f)<0.001f,"Wind units");
        rejects([]{parseWeather("not JSON",cities()[0]);},"Malformed JSON accepted");
        rejects([]{parseWeather("[]",cities()[0]);},"Non-object JSON accepted");
        rejects([]{parseWeather("{\"error\":true,\"reason\":\"Invalid coordinates\"}",cities()[0]);},"API error accepted");
        rejects([]{parseWeather("{}",cities()[0]);},"Missing current accepted");
        for (const auto* field : {"temperature_2m","weather_code","is_day","cloud_cover","wind_speed_10m","precipitation","time"}) {
            input=fixture(); input["current"].erase(field);
            rejects([&]{parseWeather(input.dump(),cities()[0]);},std::string("Missing field accepted: ")+field);
            input=fixture(); input["current"][field]=nullptr;
            rejects([&]{parseWeather(input.dump(),cities()[0]);},std::string("Null field accepted: ")+field);
        }
        const std::vector<std::pair<std::string,nlohmann::json>> badValues = {
            {"temperature_2m",-101}, {"temperature_2m",81}, {"temperature_2m","-5"},
            {"weather_code",97}, {"weather_code",73.5}, {"weather_code",73.0000001}, {"is_day",2}, {"is_day",0.5},
            {"cloud_cover",-1}, {"cloud_cover",101}, {"wind_speed_10m",-1},
            {"precipitation",-0.1}, {"time",""}
        };
        for (const auto& entry:badValues) {
            input=fixture(); input["current"][entry.first]=entry.second;
            rejects([&]{parseWeather(input.dump(),cities()[0]);},"Invalid value accepted: "+entry.first);
        }
        rejects([&]{parseWeather(fixture().dump(),{"Invalid",91,0});},"Invalid coordinates accepted");
        rejects([&]{parseWeather(fixture().dump(),{"Invalid",0,std::numeric_limits<double>::quiet_NaN()});},"NaN coordinates accepted");
        rejects([&]{parseWeather(std::string(1024*1024+1,' '),cities()[0]);},"Oversized response accepted");
        const auto invalidFetch=fetchWeather({"Invalid",1000,0});
        check(!invalidFetch.data && !invalidFetch.error.empty(),"fetchWeather must preserve errors");
        check(cities().size()==8,"City presets");
        for (const auto& city:cities()) check(!city.name.empty() && std::isfinite(city.latitude),"Invalid bundled city");
        for (const auto& group:groups) {
            const auto day=preset(group.first,true),night=preset(group.first,false);
            check(day.kind==group.first && day.isDay && !night.isDay,"Preset mode mismatch");
            check(day.temperature-night.temperature==5.f,"Night preset temperature adjustment");
        }
        Challenge challenge;
        check(!challenge.active() && !challenge.finished() && challenge.round()==0,"Challenge initial state");
        check(!challenge.submit(WeatherKind::Clear,true),"Submission before start accepted");
        challenge.start();
        check(challenge.active() && challenge.remaining()==15.f && challenge.round()==1,"Challenge start state");
        for (int round=1;round<=6;++round) {
            check(challenge.round()==round,"Incorrect challenge round");
            check(challenge.submit(challenge.targetKind(),challenge.targetDay()),"Correct answer rejected");
            check(!challenge.submit(challenge.targetKind(),challenge.targetDay()),"Duplicate scoring allowed");
            check(challenge.score()==round*100,"Score mismatch");
            challenge.update(14.f);
            check(challenge.active() && challenge.round()==round && challenge.remaining()==1.f,"Round ended too early");
            challenge.update(1.f);
        }
        check(!challenge.active() && challenge.finished() && challenge.score()==600 && challenge.remaining()==0.f,"90-second completion state");
        check(!challenge.submit(WeatherKind::Fog,false),"Scoring after completion allowed");
        challenge.reset();
        check(!challenge.active() && !challenge.finished() && challenge.score()==0 && challenge.round()==0,"Reset failed");
        challenge.start();
        check(!challenge.submit(challenge.targetKind(),!challenge.targetDay()),"Day/night mismatch scored");
        challenge.update(-2.f); challenge.update(std::numeric_limits<float>::quiet_NaN());
        check(challenge.remaining()==15.f,"Invalid timestep altered countdown");
        challenge.update(46.f);
        check(challenge.round()==4 && challenge.remaining()==14.f,"Large timestep did not advance rounds");
        challenge.update(1000.f);
        check(challenge.finished() && challenge.score()==0,"Missed-round failure path did not finish");
        challenge.start();
        check(challenge.active() && challenge.score()==0 && challenge.round()==1,"Restart failed");
        WeatherClient client;
        check(!client.busy() && !client.poll(),"Idle client reports work");
        check(client.request({"Invalid",1000,0}),"Async validation request was not queued");
        check(!client.request(cities()[0]),"Overlapping request accepted");
        std::optional<WeatherResult> result;
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
        while (!result && std::chrono::steady_clock::now()<deadline) {
            result=client.poll();
            if (!result) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        check(result && !result->data && !result->error.empty(),"Async error was not delivered");
        check(!client.busy() && !client.poll(),"Completed client did not reset");
        std::cout << "PASS: " << checks << " weather and challenge checks\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
}
