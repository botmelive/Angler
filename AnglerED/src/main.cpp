#include "AnglerED.h"

enum MODE { NOGUI, GUI };
class AnglerED {
  private:
    // Display settings
    GLFWwindow *window;
    uint16_t HEIGHT, WIDTH;
    // Renderer settings
    Render *mRenderer;
    Camera &mCamera;
    Options &options;
    ImGuiIO &io;
    int cameraFOV;
    float aspect_ratio;
    // ImGUI

    void Init();

  public:
    AnglerED(uint16_t, uint16_t, Render *, Camera &, Options &);
    ~AnglerED();

    void Loop();
};

AnglerED ::AnglerED(uint16_t Width, uint16_t Height, Render *Renderer, Camera &camera, Options &opt)
    : io(ImGui::GetIO()), mCamera(camera), options(opt) {
    WIDTH = Width;
    HEIGHT = Height;

    mRenderer = Renderer;
    mCamera = camera;

    cameraFOV = 35.0f;
    aspect_ratio = 16.0f / 9.0f;

    const char *glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    if (!glfwInit()) {
        spdlog::error("OpenGL Error! (GLFW INIT FAILED!)");
        return;
    }

    // Create a windowed mode window and its OpenGL context
    window = glfwCreateWindow(WIDTH, HEIGHT, "AnglerED", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        spdlog::error("OpenGL Error! (WINDOW CREATION FAILED!)");
        return;
    }

    // Make the window's context current
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    bool err = gladLoadGL() == 0;
    if (err) {
        spdlog::error("OpenGL Error!");
        return;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    (void)io;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);
}

AnglerED ::~AnglerED() {
    delete mRenderer;
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwTerminate();
}

void AnglerED ::Loop() {

    bool show_demo_window = true;
    bool needToBindTexture = false;

    GLuint renderedImageTexture = 0;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // #ifndef NDEBUG
        // ImGui::ShowDemoWindow(&show_demo_window);
        // #endif

        // Dont modify renderer setting while the renderer is active.
        if (!options.isRenderActive) {
            ImGui::Begin("AnglerRT settings");
            ImGui::Text("Renderer Settings");
            ImGui::InputInt("Max Depth", &options.MAX_DEPTH);
            ImGui::InputInt("Samples", &options.SAMPLES_PER_PIXEL);
            ImGui::InputFloat("Aspect Ratio ", &aspect_ratio);
            ImGui::InputInt("Image Width", &options.WIDTH);
            options.HEIGHT = static_cast<int>(options.WIDTH / aspect_ratio);
            ImGui::Text("Resolution (%d X %d)", options.WIDTH, options.HEIGHT);
            ImGui::Separator();
            ImGui::Text("Camera Settings");
            ImGui::InputInt("Camera FOV", &cameraFOV);
            static float p[4] = { 0.f, 3.f, 10.f, 0.44f };
            static float q[4] = { 0.0f, 0.0f, 0.0f, 0.44f };
            if (ImGui::InputFloat3("Look From", p) || ImGui::InputFloat3("Look At", q))
                mCamera =
                    Camera(cameraFOV, aspect_ratio, Point(p[0], p[1], p[2]), Point(q[0], q[1], q[2]), Vec3f(0, 1, 0));
            ImGui::End();
        }

        {
            ImGui::Begin("Scene Settings");
            ImGui::Text("Resolution (%d X %d)", options.WIDTH, options.HEIGHT);
            ImGui::Text("Max Depth: %d", options.MAX_DEPTH);
            ImGui::Text("Samples: %d", options.SAMPLES_PER_PIXEL);
            ImGui::Text("Aspect Ratio: %.2f", aspect_ratio);
            ImGui::Separator();
            ImGui::Text("Camera FOV: %d", cameraFOV);
            ImGui::End();
        }

        {
            ImGui::Begin("AnglerRT");
            ImGui::Text("Render Window");

            if (ImGui::Button("Render Image") && !options.isRenderActive) {
                options.image = nullptr;

                std::thread render(&Render::StartRender, *mRenderer);
                render.detach();

                // renderer.StartRender();
                needToBindTexture = true;
            } else {
                ImGui::Text("Progress : %.2f %%", options.progress * 100);
            }
            ImGui::End();
        }

        if (!options.isRenderActive && options.image) {
            if (needToBindTexture) {
                spdlog::info("AnglerED : Binding texture data");
                // options.image->GammaCorrect();

                std::shared_ptr<uint8_t[]> mBuffer = options.image->getBufferCopy();
                auto *buffer = new unsigned char[options.WIDTH * options.HEIGHT * 4];
                for (int i = 0; i < options.WIDTH * options.HEIGHT * 4; i++) {
                    buffer[i] = mBuffer[i];
                }

                if (!BindImageTexture(buffer, &renderedImageTexture, options.WIDTH, options.HEIGHT))
                    spdlog::warn("AnglerED : Binding texture failed");
                needToBindTexture = false;
                delete[] buffer;
            }

            ImGui::Begin("Rendered Image");
            ImGui::Text("Image Size = %d x %d", std::min(options.WIDTH, WIDTH - 100),
                        std::min(options.HEIGHT, HEIGHT - 100));

            ImGui::Image((void *)(intptr_t)renderedImageTexture, ImVec2(options.WIDTH, options.HEIGHT));
            ImGui::End();
        }
        ImGui::Render();

        // Render here
        // glClearColor(0.12f, 0.14f, 0.17f, 1.0f);
        glClearColor(0.117f, 0.117f, 0.117f, 1.0f);

        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        // Swap front and back buffers
        glfwSwapBuffers(window);
    }
}

int main(int argc, char *argv[]) {

    spdlog::info("CPU Arch : {} bit", sizeof(void *) * 8);

    // AnglerRT initialization.
    float aspect_ratio = 16.0 / 9.0;

    Options options;
    options.MAX_DEPTH = 4;
    options.SAMPLES_PER_PIXEL = 60;
    options.WIDTH = 512;
    options.HEIGHT = static_cast<int>(options.WIDTH / aspect_ratio);
    options.image = nullptr;

    int cameraFOV = 35;

    // Camera camera(cameraFOV, aspect_ratio, Point(14, 2, 3), Point(0, 0, 0), Vec3f(0, 1, 0));
    Camera camera = Camera(cameraFOV, aspect_ratio, Point(0, 3, 10), Point(0, 0, 0), Vec3f(0, 1, 0));

    Scene world = random_scene();

    const char *TextureFilePath = "D:/Documents/C++/Angler/Angler_ED_RT/Angler/Textures/UV_Debug.png";

    std ::shared_ptr<EnvironmentTexture> envTex = std ::make_shared<EnvironmentTexture>(TextureFilePath);
    world.SetEnvironmentTexture(nullptr);
    options.isRenderActive = false;

    Render *renderer = new Render(world, camera, options);

#ifdef __WIN32
    AnglerED anglerED(500, 400, renderer, camera, options);
    anglerED.Loop();
#endif

#ifdef __unix__
    renderer->StartRender();
#endif

    return 0;
}

#ifdef __WIN32
bool BindImageTexture(unsigned char *buffer, GLuint *out_texture, int image_width, int image_height) {

    // Load from buffer
    unsigned char *image_data = buffer;
    if (image_data == nullptr)
        return false;

    // Create a OpenGL texture identifier
    GLuint image_texture;
    glGenTextures(1, &image_texture);
    glBindTexture(GL_TEXTURE_2D, image_texture);

    // Setup filtering parameters for display
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                    GL_CLAMP_TO_EDGE); // This is required on WebGL for non power-of-two textures
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); // Same

    // Upload pixels into texture
#if defined(GL_UNPACK_ROW_LENGTH) && !defined(__EMSCRIPTEN__)
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image_width, image_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data);

    *out_texture = image_texture;

    return true;
}

#endif