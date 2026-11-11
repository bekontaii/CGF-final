#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <nlohmann/json.hpp>
#include "renderer.hpp"
#include "weather.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#else
#include <unistd.h>
#endif
namespace fs = std::filesystem;
using namespace ww;
namespace {
struct Options {
 bool hidden=false, network=true, night=false, shadows=true, highQuality=true, tour=false, challenge=false;
 int frames=0; float duration=0.f; WeatherKind weather=WeatherKind::Clear;
 fs::path screenshot, recordDir;
};
WeatherKind parseKind(const std::string& text) {
 const std::array<std::string,6> names={"clear","cloudy","rain","snow","storm","fog"};
 auto it=std::find(names.begin(),names.end(),text);
 if(it==names.end()) throw std::runtime_error("Unknown weather: "+text);
 return static_cast<WeatherKind>(std::distance(names.begin(),it));
}
void printHelp() {
 std::cout << "Weather World - C++17 / OpenGL 3.3 weather visualization\n"
 "Usage: WeatherWorld [options]\n"
 "  --no-network           Start in offline simulation mode\n"
 "  --weather KIND         clear|cloudy|rain|snow|storm|fog (simulation)\n"
 "  --night                Start at night (simulation)\n"
 "  --no-shadows           Disable PCF shadow mapping\n"
 "  --low-quality          Use fewer particles and lower shadow resolution\n"
 "  --hidden               Render without a visible window (GL context required)\n"
 "  --frames N             Exit after N rendered frames\n"
 "  --duration SECONDS     Exit after this simulation duration\n"
 "  --screenshot PATH      Save final framebuffer as PNG\n"
 "  --tour                 Cycle weather and camera every ten seconds\n"
 "  --challenge            Start the 90-second weather challenge\n"
 "  --record-dir DIR       Save deterministic 30 FPS PNG frames; use --duration\n"
 "  --help                 Show this help\n\n"
 "Keys: 1-6 weather, N day/night, Space pause, T shadows, Q quality,\n"
 "R reset camera, Left/Right rotate light, F refresh, F1/H help, Esc close.\n"
 "Drag outside the panel to orbit. Scroll to zoom.\n";
}
Options parseOptions(int argc,char** argv) {
 Options o;
 for(int i=1;i<argc;++i) {
  std::string a=argv[i];
  auto value=[&](){if(i+1>=argc)throw std::runtime_error("Missing value after "+a);return std::string(argv[++i]);};
  if(a=="--help" || a=="-h") {printHelp();std::exit(0);}
  else if(a=="--hidden")o.hidden=true;
  else if(a=="--no-network")o.network=false;
  else if(a=="--night") {o.night=true;o.network=false;}
  else if(a=="--no-shadows")o.shadows=false;
  else if(a=="--low-quality")o.highQuality=false;
  else if(a=="--tour") {o.tour=true;o.network=false;}
  else if(a=="--challenge") {o.challenge=true;o.network=false;}
  else if(a=="--weather") {o.weather=parseKind(value());o.network=false;}
  else if(a=="--frames") {o.frames=std::stoi(value());if(o.frames<1)throw std::runtime_error("--frames must be positive");}
  else if(a=="--duration") {o.duration=std::stof(value());if(!std::isfinite(o.duration)||o.duration<=0)throw std::runtime_error("--duration must be positive");}
  else if(a=="--screenshot")o.screenshot=value();
  else if(a=="--record-dir") {o.recordDir=value();o.network=false;}
  else throw std::runtime_error("Unknown option: "+a+". Use --help.");
 }
 if(!o.recordDir.empty()&&o.frames==0&&o.duration==0)o.duration=70;
 if(o.hidden&&o.frames==0&&o.duration==0)o.frames=180;
 return o;
}
fs::path executableDirectory(const char* argv0) {
#ifdef _WIN32
 std::vector<wchar_t> buffer(32768);
 DWORD length=GetModuleFileNameW(nullptr,buffer.data(),static_cast<DWORD>(buffer.size()));
 if(length>0 && length<buffer.size())return fs::path(std::wstring(buffer.data(),length)).parent_path();
#elif defined(__APPLE__)
 uint32_t size=0;_NSGetExecutablePath(nullptr,&size);std::vector<char> buffer(size+1);
 if(_NSGetExecutablePath(buffer.data(),&size)==0)return fs::weakly_canonical(buffer.data()).parent_path();
#else
 std::array<char,4096> buffer{};ssize_t length=readlink("/proc/self/exe",buffer.data(),buffer.size()-1);
 if(length>0)return fs::path(std::string(buffer.data(),static_cast<size_t>(length))).parent_path();
#endif
 return fs::absolute(argv0).parent_path();
}
fs::path findAssetRoot(const char* argv0) {
 std::vector<fs::path> roots={executableDirectory(argv0),fs::current_path(),fs::path(WEATHER_SOURCE_DIR)};
 for(const auto& p:roots)if(fs::is_directory(p/"shaders")&&fs::is_directory(p/"assets"))return p;
 throw std::runtime_error("Cannot locate shaders/ and assets/. Keep them beside the executable.");
}
struct Team {
 std::string name="Team name not entered",group="Group not entered",topic="Weather World - Real-Time Weather Visualization and Shadow Mapping",number="Not supplied",members;
};
Team readTeam(const fs::path& root) {
 Team t;std::ifstream input(root/"assets"/"team.json");
 if(!input){std::cerr<<"[config] assets/team.json missing; edit team details before submission.\n";return t;}
 try {
  nlohmann::json j;input>>j;
  t.name=j.value("team_name",t.name);t.group=j.value("group",t.group);t.topic=j.value("topic_title",t.topic);
  if(j.contains("topic_number"))t.number=j["topic_number"].is_string()?j["topic_number"].get<std::string>():j["topic_number"].dump();
  if(j.contains("members")&&j["members"].is_array())for(const auto& m:j["members"]){
   std::string name=m.is_string()?m.get<std::string>():m.value("name","");
   if(!name.empty()){if(!t.members.empty())t.members+=" / ";t.members+=name;}
  }
 }catch(const std::exception& e){std::cerr<<"[config] Invalid team.json: "<<e.what()<<'\n';}
 return t;
}
void theme() {
 ImGui::StyleColorsDark();auto& s=ImGui::GetStyle();
 s.WindowRounding=0;s.ChildRounding=10;s.FrameRounding=6;s.GrabRounding=5;s.PopupRounding=8;
 s.WindowPadding={22,16};s.FramePadding={10,5};s.ItemSpacing={8,6};s.ScrollbarSize=7;s.WindowBorderSize=0;
 auto* c=s.Colors;
 c[ImGuiCol_WindowBg]={.043f,.071f,.098f,1};c[ImGuiCol_ChildBg]={.065f,.103f,.132f,1};
 c[ImGuiCol_PopupBg]={.065f,.103f,.132f,1};c[ImGuiCol_TitleBg]={.043f,.071f,.098f,1};c[ImGuiCol_TitleBgActive]={.065f,.103f,.132f,1};c[ImGuiCol_Text]={.91f,.95f,.94f,1};
 c[ImGuiCol_TextDisabled]={.47f,.60f,.63f,1};c[ImGuiCol_FrameBg]={.10f,.16f,.19f,1};
 c[ImGuiCol_FrameBgHovered]={.14f,.24f,.26f,1};c[ImGuiCol_FrameBgActive]={.18f,.32f,.32f,1};
 c[ImGuiCol_Button]={.11f,.18f,.21f,1};c[ImGuiCol_ButtonHovered]={.18f,.33f,.33f,1};
 c[ImGuiCol_ButtonActive]={.20f,.45f,.39f,1};c[ImGuiCol_CheckMark]={.49f,.89f,.72f,1};
 c[ImGuiCol_SliderGrab]={.45f,.83f,.68f,1};c[ImGuiCol_SliderGrabActive]={.67f,.96f,.78f,1};
 c[ImGuiCol_Header]={.13f,.25f,.27f,1};c[ImGuiCol_HeaderHovered]={.19f,.35f,.35f,1};
 c[ImGuiCol_Separator]={.15f,.23f,.26f,1};c[ImGuiCol_Border]={.15f,.23f,.26f,1};
}
void muted(const char* text){ImGui::TextColored({.49f,.64f,.66f,1},"%s",text);}
void label(const char* text){ImGui::Spacing();ImGui::TextColored({.56f,.78f,.73f,1},"%s",text);}
bool accentButton(const char* text,ImVec2 size={0,0}) {
 ImGui::PushStyleColor(ImGuiCol_Button,{.28f,.65f,.50f,1});
 ImGui::PushStyleColor(ImGuiCol_ButtonHovered,{.38f,.77f,.59f,1});
 ImGui::PushStyleColor(ImGuiCol_Text,{.025f,.10f,.08f,1});bool clicked=ImGui::Button(text,size);ImGui::PopStyleColor(3);return clicked;
}
void helpWindow(bool& show) {
 if(show&&!ImGui::IsPopupOpen("Controls and project"))ImGui::OpenPopup("Controls and project");
 ImGui::SetNextWindowSize({620,std::min(ImGui::GetIO().DisplaySize.y-40.f,650.f)},ImGuiCond_Appearing);
 if(ImGui::BeginPopupModal("Controls and project",&show,ImGuiWindowFlags_NoResize)) {
  ImGui::TextWrapped("Explore a small 3D world driven by real weather or your own settings. Dynamic PCF shadow mapping is the headline graphics technique.");
  ImGui::Separator();
  if(ImGui::BeginTable("keys",2,ImGuiTableFlags_RowBg|ImGuiTableFlags_BordersInnerH|ImGuiTableFlags_SizingStretchProp)) {
   ImGui::TableSetupColumn("Key",ImGuiTableColumnFlags_WidthFixed,190.f);
   ImGui::TableSetupColumn("Action",ImGuiTableColumnFlags_WidthStretch);
   const char* rows[][2]={{"Mouse drag / wheel","Orbit / zoom outside the panel"},{"1 / 2 / 3 / 4 / 5 / 6","Clear / cloudy / rain / snow / storm / fog"},{"N","Toggle day and night in simulation"},{"Space","Pause scene animation and challenge timer"},{"T","Toggle dynamic PCF shadows"},{"Q","Toggle high / low quality"},{"Left / Right","Rotate the directional light"},{"R","Reset camera"},{"F","Refresh selected city's live weather"},{"F1 or H","Show this help"},{"Escape","Close help, then close application"}};
   for(const auto& r:rows){ImGui::TableNextRow();ImGui::TableNextColumn();ImGui::TextUnformatted(r[0]);ImGui::TableNextColumn();ImGui::TextWrapped("%s",r[1]);}
   ImGui::EndTable();
  }
  ImGui::Spacing();ImGui::TextWrapped("Challenge: six 15-second rounds. Set the requested weather and day/night, then submit once per round. Correct answers earn 100 points; reach 400 of 600 to win. The next round begins when its timer expires. Reset or replay without restarting the app.");
  ImGui::TextWrapped("Live data: Open-Meteo (open-meteo.com), updated every 10 minutes. No API key is required. A network failure is shown on screen; simulation remains available.");
  ImGui::Spacing();if(accentButton("Back to the world",{-1,0})){show=false;ImGui::CloseCurrentPopup();}
  ImGui::EndPopup();
 }
}
struct WindowGuard {
 GLFWwindow* window=nullptr;bool context=false,glfwBackend=false,glBackend=false;
 ~WindowGuard(){if(glBackend)ImGui_ImplOpenGL3_Shutdown();if(glfwBackend)ImGui_ImplGlfw_Shutdown();if(context)ImGui::DestroyContext();if(window)glfwDestroyWindow(window);glfwTerminate();}
};
}
int main(int argc,char** argv) {
 try {
  Options options=parseOptions(argc,argv);const auto root=findAssetRoot(argv[0]);const Team team=readTeam(root);
  if(!options.recordDir.empty())fs::create_directories(options.recordDir);
  if(!options.screenshot.empty()&&options.screenshot.has_parent_path())fs::create_directories(options.screenshot.parent_path());
  glfwSetErrorCallback([](int code,const char* description){std::cerr<<"[GLFW "<<code<<"] "<<description<<'\n';});
  if(!glfwInit())throw std::runtime_error("GLFW initialization failed. A desktop OpenGL session is required.");
  WindowGuard guard;
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
  glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT,GL_TRUE);
  glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER,GLFW_FALSE);
#endif
  glfwWindowHint(GLFW_VISIBLE,options.hidden?GLFW_FALSE:GLFW_TRUE);
  guard.window=glfwCreateWindow(1280,720,"Weather World | Real-time Weather + PCF Shadows",nullptr,nullptr);
  if(!guard.window)throw std::runtime_error("OpenGL 3.3 core window could not be created.");
  glfwSetWindowSizeLimits(guard.window,1000,640,GLFW_DONT_CARE,GLFW_DONT_CARE);
  glfwMakeContextCurrent(guard.window);glfwSwapInterval((options.hidden||!options.recordDir.empty())?0:1);
  if(!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))throw std::runtime_error("GLAD failed to load OpenGL functions.");
  std::cout<<"OpenGL: "<<glGetString(GL_VERSION)<<"\nGPU: "<<glGetString(GL_RENDERER)<<"\nVendor: "<<glGetString(GL_VENDOR)<<"\nAssets: "<<root.string()<<'\n';
  IMGUI_CHECKVERSION();ImGui::CreateContext();guard.context=true;auto& io=ImGui::GetIO();io.IniFilename=nullptr;
  ImFontConfig fontConfig;fontConfig.SizePixels=16.f;
  const auto fontPath=root/"assets"/"Lato-Regular.ttf";
  ImFont* bodyFont=nullptr;ImFont* titleFont=nullptr;ImFont* heroFont=nullptr;ImFont* metricFont=nullptr;
  if(fs::exists(fontPath)){
   bodyFont=io.Fonts->AddFontFromFileTTF(fontPath.string().c_str(),16.f);
   titleFont=io.Fonts->AddFontFromFileTTF(fontPath.string().c_str(),28.f);
   heroFont=io.Fonts->AddFontFromFileTTF(fontPath.string().c_str(),34.f);
   metricFont=io.Fonts->AddFontFromFileTTF(fontPath.string().c_str(),28.f);
  }
  if(!bodyFont)bodyFont=io.Fonts->AddFontDefault(&fontConfig);io.FontDefault=bodyFont;
  theme();
  if(!ImGui_ImplGlfw_InitForOpenGL(guard.window,true))throw std::runtime_error("ImGui GLFW backend initialization failed.");guard.glfwBackend=true;
  if(!ImGui_ImplOpenGL3_Init("#version 330 core"))throw std::runtime_error("ImGui OpenGL backend initialization failed.");guard.glBackend=true;
  Renderer renderer;renderer.initialize(root.string());Camera camera;RenderSettings settings;
  settings.shadows=options.shadows;settings.highQuality=options.highQuality;
  WeatherState simulation=preset(options.weather,!options.night);std::optional<WeatherState> liveData;
  WeatherClient client;Challenge challenge;bool wantLive=options.network,paused=false,help=false,submitted=false;
  std::string networkError;int cityIndex=0,previousRound=0;double lastRequest=-600.;float simTime=0,elapsed=0;
  const auto& availableCities=cities();if(availableCities.empty())throw std::runtime_error("No cities configured.");
  simulation.city=availableCities[0].name;
  auto refresh=[&](){if(client.request(availableCities[static_cast<size_t>(cityIndex)])){lastRequest=glfwGetTime();networkError.clear();}};
  if(wantLive)refresh();
  if(options.challenge){challenge.start();previousRound=challenge.round();}
  std::array<bool,GLFW_KEY_LAST+1> previousKeys{};
  auto lastTick=std::chrono::steady_clock::now();auto measurementStart=lastTick;
  double measuredSeconds=0;int measuredFrames=0,totalFrames=0,lastWidth=0,lastHeight=0;float displayedFps=0;
  DrawStats stats;
  while(!glfwWindowShouldClose(guard.window)) {
   glfwPollEvents();auto now=std::chrono::steady_clock::now();
   double realDt=std::chrono::duration<double>(now-lastTick).count();lastTick=now;
   float dt=options.recordDir.empty()?std::min(static_cast<float>(realDt),.1f):1.f/30.f;
   elapsed+=dt;if(!paused){simTime+=dt;challenge.update(dt);}
   if(auto result=client.poll()){if(result->data){if(result->data->city==availableCities[static_cast<size_t>(cityIndex)].name){liveData=*result->data;networkError.clear();}}else{networkError=result->error;std::cerr<<"[weather] "<<networkError<<'\n';}}
   if(wantLive&&!client.busy()&&glfwGetTime()-lastRequest>=600.)refresh();
   if(options.tour&&!challenge.active()){
    int segment=static_cast<int>(simTime/10.f)%7;
    simulation=preset(segment==6?WeatherKind::Clear:static_cast<WeatherKind>(segment),segment!=6);
    simulation.city=availableCities[static_cast<size_t>(cityIndex)].name;
    camera.yaw=42.f+simTime*2.2f;camera.pitch=27.f+3.f*std::sin(simTime*.12f);
   }
   if(challenge.round()!=previousRound){submitted=false;previousRound=challenge.round();}
   ImGui_ImplOpenGL3_NewFrame();ImGui_ImplGlfw_NewFrame();ImGui::NewFrame();
   auto pressed=[&](int key){bool down=glfwGetKey(guard.window,key)==GLFW_PRESS;bool p=down&&!previousKeys[static_cast<size_t>(key)];previousKeys[static_cast<size_t>(key)]=down;return p;};
   bool esc=pressed(GLFW_KEY_ESCAPE),f1=pressed(GLFW_KEY_F1),h=pressed(GLFW_KEY_H);
   if(esc){if(help){help=false;ImGui::CloseCurrentPopup();}else glfwSetWindowShouldClose(guard.window,GLFW_TRUE);}
   if(f1||h)help=!help;
   bool useKeyboard=!io.WantTextInput&&!help;
   for(int i=0;i<6;++i)if(pressed(GLFW_KEY_1+i)&&useKeyboard){wantLive=false;simulation=preset(static_cast<WeatherKind>(i),simulation.isDay);simulation.city=availableCities[static_cast<size_t>(cityIndex)].name;}
   if(pressed(GLFW_KEY_N)&&useKeyboard){wantLive=false;simulation.isDay=!simulation.isDay;}
   if(pressed(GLFW_KEY_SPACE)&&useKeyboard)paused=!paused;
   if(pressed(GLFW_KEY_T)&&useKeyboard)settings.shadows=!settings.shadows;
   if(pressed(GLFW_KEY_Q)&&useKeyboard)settings.highQuality=!settings.highQuality;
   if(pressed(GLFW_KEY_R)&&useKeyboard)camera.reset();
   if(pressed(GLFW_KEY_F)&&useKeyboard&&!challenge.active()){wantLive=true;refresh();}
   if(useKeyboard){if(glfwGetKey(guard.window,GLFW_KEY_LEFT)==GLFW_PRESS)settings.lightAngle-=35.f*dt;if(glfwGetKey(guard.window,GLFW_KEY_RIGHT)==GLFW_PRESS)settings.lightAngle+=35.f*dt;}
   if(!io.WantCaptureMouse&&!help){
    if(ImGui::IsMouseDragging(ImGuiMouseButton_Left)||ImGui::IsMouseDragging(ImGuiMouseButton_Right)){camera.yaw+=io.MouseDelta.x*.34f;camera.pitch=std::clamp(camera.pitch-io.MouseDelta.y*.25f,8.f,75.f);}
    camera.distance=std::clamp(camera.distance-io.MouseWheel*1.4f,11.f,42.f);
   }
   WeatherState current=(wantLive&&liveData)?*liveData:simulation;
   float panelWidth=io.DisplaySize.x*.25f;
   ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({panelWidth,io.DisplaySize.y});
   ImGui::Begin("Weather controls",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings);
   muted("REALTIME GRAPHICS LAB");ImGui::PushFont(titleFont);ImGui::TextUnformatted("Weather World");ImGui::PopFont();
   muted("A small world. Every atmosphere.");ImGui::Spacing();ImGui::Separator();
   float half=(ImGui::GetContentRegionAvail().x-8.f)*.5f;
   if(!wantLive){if(accentButton("Simulation",{half,0})){wantLive=false;}}else if(ImGui::Button("Simulation",{half,0}))wantLive=false;
   ImGui::SameLine();
   ImGui::BeginDisabled(challenge.active());
   if(wantLive){if(accentButton("Live weather",{half,0}))refresh();}else if(ImGui::Button("Live weather",{half,0})){wantLive=true;refresh();}
   ImGui::EndDisabled();
   ImGui::BeginDisabled(client.busy()||challenge.active());
   ImGui::SetNextItemWidth(-1);
   if(ImGui::BeginCombo("##city",availableCities[static_cast<size_t>(cityIndex)].name.c_str())){
    for(size_t i=0;i<availableCities.size();++i)if(ImGui::Selectable(availableCities[i].name.c_str(),cityIndex==static_cast<int>(i))){cityIndex=static_cast<int>(i);simulation.city=availableCities[i].name;liveData.reset();if(wantLive)refresh();}
    ImGui::EndCombo();
   }
   ImGui::EndDisabled();
   if(client.busy())muted("Fetching Open-Meteo data...");
   else if(wantLive&&liveData){muted("LIVE / refreshes every 10 min");}
   else if(wantLive)muted("No live data / simulation shown");
   else muted("SIMULATION / manual controls");
   if(!networkError.empty()){
    ImGui::PushStyleColor(ImGuiCol_Text,{1,.67f,.40f,1});ImGui::TextWrapped("%s",networkError.c_str());ImGui::PopStyleColor();
    if(wantLive&&liveData)muted("Last successful observation retained.");
    ImGui::BeginDisabled(client.busy());if(ImGui::SmallButton("Retry connection"))refresh();ImGui::EndDisabled();
   }
   ImGui::Spacing();
   ImGui::BeginChild("Weather metrics",{0,72},ImGuiChildFlags_None,ImGuiWindowFlags_NoScrollbar);
   ImGui::PushFont(metricFont);ImGui::Text("%.0f C",current.temperature);ImGui::PopFont();
   ImGui::SameLine(112);ImGui::Text("%s",weatherName(current.kind));
   ImGui::Text("Wind %.0f km/h   Clouds %.0f%%",current.windSpeed,current.cloudCover);
   ImGui::EndChild();
   if(wantLive&&liveData){ImGui::TextWrapped("Observed %s",liveData->time.c_str());ImGui::BeginDisabled(client.busy());if(ImGui::SmallButton("Refresh now [F]"))refresh();ImGui::EndDisabled();}
   label("WEATHER PRESETS");
   const char* names[]={"1  Clear","2  Cloudy","3  Rain","4  Snow","5  Storm","6  Fog"};
   for(int i=0;i<6;++i){
    bool selected=!wantLive&&simulation.kind==static_cast<WeatherKind>(i);
    if(selected)ImGui::PushStyleColor(ImGuiCol_Button,{.22f,.43f,.36f,1});
    if(ImGui::Button(names[i],{half,0})){wantLive=false;simulation=preset(static_cast<WeatherKind>(i),simulation.isDay);simulation.city=availableCities[static_cast<size_t>(cityIndex)].name;}
    if(selected)ImGui::PopStyleColor();if(i%2==0)ImGui::SameLine();
   }
   bool night=!(wantLive&&liveData?liveData->isDay:simulation.isDay);
   if(ImGui::Checkbox("Night atmosphere [N]",&night)){wantLive=false;simulation.isDay=!night;}
   ImGui::Checkbox("Pause animation [Space]",&paused);
   label("RENDERING");
   ImGui::Checkbox("PCF shadows [T]",&settings.shadows);ImGui::SameLine();
   ImGui::TextColored(settings.shadows?ImVec4(.54f,.91f,.73f,1):ImVec4(.95f,.62f,.37f,1),"%s",settings.shadows?"ON":"OFF");
   ImGui::Checkbox("High quality [Q]",&settings.highQuality);
   ImGui::SetNextItemWidth(-1);ImGui::SliderFloat("##light",&settings.lightAngle,-180.f,180.f,"Light azimuth %.0f deg");
   ImGui::Spacing();
   if(!challenge.active()&&!challenge.finished()){
    if(accentButton("Start 90-second challenge",{-1,0})){wantLive=false;challenge.start();submitted=false;previousRound=challenge.round();}
   }else if(challenge.finished()){
    ImGui::Text("%s / %d of 600",challenge.score()>=400?"Goal reached":"Try again",challenge.score());
    if(accentButton("Play again",{half,0})){challenge.start();wantLive=false;submitted=false;previousRound=challenge.round();}
    ImGui::SameLine();if(ImGui::Button("Reset",{half,0})){challenge.reset();submitted=false;}
   }else{
    ImGui::Text("Round %d / 6    Score %d",challenge.round(),challenge.score());
    ImGui::Text("Set: %s / %s",weatherName(challenge.targetKind()),challenge.targetDay()?"day":"night");
    ImGui::ProgressBar(challenge.remaining()/15.f,{-1,7},"");
    ImGui::BeginDisabled(submitted);
    if(accentButton("Submit weather",{half,0})){challenge.submit(simulation.kind,simulation.isDay);submitted=true;}
    ImGui::EndDisabled();ImGui::SameLine();if(ImGui::Button("Reset",{half,0})){challenge.reset();submitted=false;}
    ImGui::TextWrapped("%s",challenge.feedback().c_str());
   }
   ImGui::Spacing();ImGui::Separator();
   if(ImGui::SmallButton("Controls [F1]"))help=true;ImGui::SameLine();if(ImGui::SmallButton("Reset view [R]"))camera.reset();
   ImGui::TextWrapped("%s / %s",team.name.c_str(),team.group.c_str());
   ImGui::End();
   // Scene information is rendered as non-interactive overlays so the camera remains draggable.
   ImGui::SetNextWindowPos({panelWidth+27,24});ImGui::SetNextWindowBgAlpha(.36f);
   ImGui::Begin("Scene heading",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoInputs|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoSavedSettings);
   ImGui::TextColored({.70f,.89f,.85f,1},"%s  /  %s",current.city.c_str(),wantLive&&liveData?"LIVE WEATHER":"SIMULATION");
   ImGui::PushFont(heroFont);ImGui::TextUnformatted(weatherName(current.kind));ImGui::PopFont();
   ImGui::Text("%s  |  PCF shadows %s",current.isDay?"Daylight":"Nightfall",settings.shadows?"on":"off");
   ImGui::Text("%s / %s",team.name.c_str(),team.group.c_str());
   if(paused)ImGui::TextColored({1,.77f,.42f,1},"ANIMATION PAUSED");
   ImGui::End();
   ImGui::SetNextWindowPos({panelWidth+27,io.DisplaySize.y-86});ImGui::SetNextWindowSize({io.DisplaySize.x-panelWidth-54,70});ImGui::SetNextWindowBgAlpha(.56f);
   ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,9);ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{15,10});
   ImGui::Begin("Scene status",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoInputs|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoScrollbar);
   ImGui::Text("%.0f FPS    %s quality    %d objects    %d particles",displayedFps,settings.highQuality?"High":"Low",stats.objects,stats.particles);
   ImGui::TextWrapped("%s%s%s",team.topic.c_str(),team.members.empty()?"":"  |  ",team.members.c_str());
   ImGui::End();ImGui::PopStyleVar(2);
   if(challenge.active()){
    ImGui::SetNextWindowPos({io.DisplaySize.x-292,26});ImGui::SetNextWindowSize({265,116});ImGui::SetNextWindowBgAlpha(.88f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,10);ImGui::Begin("Challenge status",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoInputs|ImGuiWindowFlags_NoSavedSettings);
    ImGui::Text("CHALLENGE  %d / 6    %.0fs",challenge.round(),std::ceil(challenge.remaining()));
    ImGui::Text("Target: %s / %s",weatherName(challenge.targetKind()),challenge.targetDay()?"day":"night");
    ImGui::Text("Score: %d / 600",challenge.score());
    ImGui::End();ImGui::PopStyleVar();
   }
   helpWindow(help);
   ImGui::Render();
   int width=0,height=0;glfwGetFramebufferSize(guard.window,&width,&height);
   if(width>0&&height>0){
    lastWidth=width;lastHeight=height;
    current=(wantLive&&liveData)?*liveData:simulation;
    stats=renderer.draw(current,camera,settings,width,height,simTime,paused?0.f:dt);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    bool last=glfwWindowShouldClose(guard.window)||(options.frames>0&&totalFrames+1>=options.frames)||(options.duration>0&&elapsed>=options.duration);
    if(!options.recordDir.empty()){
     std::ostringstream filename;filename<<"frame_"<<std::setw(5)<<std::setfill('0')<<totalFrames<<".png";
     if(!renderer.screenshot((options.recordDir/filename.str()).string(),width,height))throw std::runtime_error("Frame recording failed.");
    }
    if(last&&!options.screenshot.empty())if(!renderer.screenshot(options.screenshot.string(),width,height))throw std::runtime_error("Screenshot failed.");
    GLenum error=glGetError();if(error!=GL_NO_ERROR){std::cerr<<"[OpenGL] error 0x"<<std::hex<<error<<std::dec<<" on frame "<<totalFrames<<'\n';throw std::runtime_error("OpenGL rendering error.");}
    glfwSwapBuffers(guard.window);
    // Hidden benchmark windows do not wait for presentation. Synchronize so
    // timing measures completed GPU work rather than queued draw submission.
    if(options.hidden)glFinish();
    ++totalFrames;
    auto frameEnd=std::chrono::steady_clock::now();
    if(totalFrames>30){measuredSeconds+=std::chrono::duration<double>(frameEnd-measurementStart).count();++measuredFrames;}
    measurementStart=frameEnd;
    if(realDt>0)displayedFps=displayedFps==0?static_cast<float>(1./realDt):displayedFps*.93f+static_cast<float>(1./realDt)*.07f;
    if(last)break;
   }
  }
  std::cout<<"BENCHMARK {\"frames\":"<<totalFrames<<",\"sample_frames\":"<<measuredFrames<<",\"fps\":"<<std::fixed<<std::setprecision(2)<<(measuredSeconds>0?measuredFrames/measuredSeconds:0)<<",\"shadows\":"<<(settings.shadows?"true":"false")<<",\"quality\":\""<<(settings.highQuality?"high":"low")<<"\",\"recording\":"<<(!options.recordDir.empty()?"true":"false")<<",\"gpu_synchronized\":"<<(options.hidden?"true":"false")<<",\"width\":"<<lastWidth<<",\"height\":"<<lastHeight<<"}\n";
  return 0;
 }catch(const std::exception& e){std::cerr<<"Weather World: "<<e.what()<<'\n';return 1;}
}
