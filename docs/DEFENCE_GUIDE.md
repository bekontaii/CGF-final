# Weather World Defence Guide

## Five minute presentation

| Time | Show | Explain |
|---|---|---|
| 0:00–0:35 | Clear daytime scene with moving camera | Goal: a 3D weather visualization using custom OpenGL rendering. State the assigned topic after confirming it. |
| 0:35–1:20 | Pipeline diagram | Live response or simulation updates WeatherState. The light-depth pass writes a shadow map. The camera pass shades the scene and reads that depth texture. Particles and UI follow. |
| 1:20–2:10 | Clear scene, turn light, toggle T twice | Explain light-space coordinates, depth comparison, bias and 3×3 PCF. Point out the actual moving tree/house shadows. |
| 2:10–3:00 | Rain, snow, storm and night | Explain instanced particles, different speeds and sizes, sky and light changes, lightning as a transient illumination term. |
| 3:00–3:40 | Live Mode and a city, then Simulation | Explain asynchronous HTTP, WMO mapping, provider time and explicit error/fallback behavior. Simulation makes the defence independent of weather and connectivity. |
| 3:40–4:20 | Challenge start and submit | Explain a 90-second loop, six goals, 100 points per correct answer, a 400/600 target, one answer per round, timeout and reset. |
| 4:20–5:00 | Performance table and source | State machine and resolution. Compare identical scenes with/without shadows. Name limitations and actual member contributions. |

Have the app open before speaking. The first 15 seconds should visibly show the headline technique. Keep a clear daytime scene ready because shadows are easiest to see there. Use the backup video only for machine failure.

## Two minute questions

**Why render the scene twice for shadows?** The first render sees from the light and records the nearest surface depth. The camera render compares the current surface's light-space depth with that stored value. A surface farther away is occluded.

**What does PCF average?** It averages the outcomes of nearby depth comparisons. It does not average linear depth values first. Nine comparisons form a 3×3 kernel, producing a softer edge.

**Why is a bias necessary?** Finite depth precision and differences between rasterization in the two passes can make a surface shadow itself. A small normal-dependent bias reduces this acne. Excessive bias detaches the shadow, producing peter panning.

**Why is the light projection orthographic?** The main light represents distant sunlight, whose rays are approximated as parallel. A local point light would need a different shadow projection, commonly six cubemap faces.

**What happens outside the shadow map?** Coordinates outside its valid bounds must be treated as unshadowed rather than sampling invalid wrapped depths. Check the actual shader branch.

**How does the camera survive resize?** The projection uses the framebuffer width/height aspect ratio. A zero-size minimized window is skipped. Window dimensions and framebuffer dimensions differ on high-DPI screens.

**Why use a normal matrix?** Non-uniform scaling changes surface normals differently from positions. The inverse transpose of the model's upper-left 3×3 keeps the transformed normal perpendicular to the surface.

**What does instancing save?** Many particles share one base geometry and a single draw call. Per-instance attributes or a vertex-shader instance index produce different positions. The GPU still processes each visible instance.

**Does Live mean a direct local sensor?** No. The application shows the provider's current weather model values and timestamp. The API does not guarantee a new observation for each refresh.

**What happens if the API is down?** A bounded background request returns an error. The interface reports it and simulation remains usable. The graphics loop does not wait on the request.

**Is this physically accurate weather?** No. It is a stylized visual mapping of weather variables. Clouds, lightning timing and particle motion are illustrative; there is no fluid simulation, atmospheric scattering solver or physically based snow accumulation model.

**Can you prove the score is not hardcoded?** Show tests and perform a correct answer, wrong answer, timeout and reset. The score changes only when the active round accepts its first answer.

## Exercises before submission

Every member should do these changes and explain the result:

1. Reduce the PCF kernel to one sample and observe the hard shadow edge.
2. Change bias by a factor of ten in both directions and identify acne and detached shadows.
3. Rotate the light and explain which matrices change.
4. Change snow speed and explain whether animation remains independent of FPS.
5. Add a city with a known coordinate pair and validate the response.
6. Add one weather JSON test with a null field and describe the user-visible behavior.

Rebuild after each exercise. Restore the version intended for the defence before recording the backup video.

## Submission fields still requiring team input

- Actual team name, group, assigned topic number and exact title from the Topic List.
- Section 6 must-implement checklist for that exact topic, with honest done/partial/missing marks.
- Individual names and factual module contributions. Do not attribute generated modules as independent student work.
- Public or link-accessible YouTube/Google Drive backup video URL.
- A Windows 10 lab-PC build and 1280×720 performance check. Results on another machine do not establish the lab target.

Source disclosure: substantial code and documentation were generated with OpenAI Codex assistance on 5 November 2026. Follow the course's academic-integrity rule, cite the assistance and be able to modify and explain every submitted shader.
