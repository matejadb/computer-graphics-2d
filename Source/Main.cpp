#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <vector>
#include "../Header/Util.h"

// ========== KONSTANTE ==========
const float TARGET_FPS = 75.0f;
const float FRAME_TIME = 1.0f / TARGET_FPS;
const int NUM_STATIONS = 10;
const float BUS_SPEED = 0.15f;
const float STATION_WAIT_TIME = 10.0f;

// ========== STRUKTURE ==========
struct Vec2 {
    float x, y;
    Vec2(float x = 0, float y = 0) : x(x), y(y) {}
};

struct Station {
    Vec2 position;
    int number;
};

// ========== GLOBALNE PROMENLJIVE ==========
Station stations[NUM_STATIONS];
int currentStation = 0;
int nextStation = 1;
float busProgress = 0.0f;
bool busAtStation = true;
float stationTimer = 0.0f;
int passengers = 0;
bool isInspectorInBus = false;
int totalFines = 0;
int inspectorExitStation = -1;

bool leftMousePressed = false;
bool rightMousePressed = false;
bool keyKPressed = false;

unsigned int pathVAO, pathVBO;
unsigned int circleVAO, circleVBO;

// ========== CALLBACK FUNKCIJE ==========
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
    }
    if (key == GLFW_KEY_K && action == GLFW_PRESS) {
        keyKPressed = true;
    }
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        leftMousePressed = true;
    }
    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
        rightMousePressed = true;
    }
}

// ========== HELPER FUNKCIJE ==========
Vec2 lerp(Vec2 a, Vec2 b, float t) {
    return Vec2(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t);
}

Vec2 bezierQuadratic(Vec2 p0, Vec2 p1, Vec2 p2, float t) {
    float u = 1.0f - t;
    return Vec2(
        u * u * p0.x + 2 * u * t * p1.x + t * t * p2.x,
        u * u * p0.y + 2 * u * t * p1.y + t * t * p2.y
    );
}

void initStations() {
    // Creating a more natural, irregular path that resembles a city bus route
    // Stations are positioned to create varied curves and straight sections

    stations[0].position = Vec2(-0.65f, 0.55f);   // Top-left area
    stations[1].position = Vec2(-0.25f, 0.65f);   // Top-center-left
    stations[2].position = Vec2(0.35f, 0.60f);    // Top-right area
    stations[3].position = Vec2(0.70f, 0.25f);    // Right side, upper
    stations[4].position = Vec2(0.75f, -0.15f);   // Right side, lower
    stations[5].position = Vec2(0.45f, -0.55f);   // Bottom-right
    stations[6].position = Vec2(0.0f, -0.65f);    // Bottom-center
    stations[7].position = Vec2(-0.50f, -0.50f);  // Bottom-left
    stations[8].position = Vec2(-0.75f, -0.10f);  // Left side, lower
    stations[9].position = Vec2(-0.70f, 0.20f);   // Left side, upper

    for (int i = 0; i < NUM_STATIONS; i++) {
        stations[i].number = i;
    }
}

void setupPathVAO() {
    std::vector<float> pathVertices;

    for (int i = 0; i < NUM_STATIONS; i++) {
        int nextIdx = (i + 1) % NUM_STATIONS;
        Vec2 p0 = stations[i].position;
        Vec2 p2 = stations[nextIdx].position;

        // Calculate direction and normal vectors
        Vec2 dir = Vec2(p2.x - p0.x, p2.y - p0.y);
        float dist = sqrt(dir.x * dir.x + dir.y * dir.y);
        Vec2 normal = Vec2(-dir.y, dir.x);

        if (dist > 0.0001f) {
            normal.x /= dist;
            normal.y /= dist;
        }

        // Variable curvature based on station position to create more natural turns
        float curvature = 0.12f + 0.08f * sin(i * 0.7f); // Varies between 0.12 and 0.20

        // Alternate curve direction for more interesting path
        float curveDir = (i % 3 == 0) ? -1.0f : 1.0f;

        Vec2 midPoint = Vec2((p0.x + p2.x) / 2.0f, (p0.y + p2.y) / 2.0f);
        Vec2 controlPoint = Vec2(
            midPoint.x + normal.x * curvature * curveDir,
            midPoint.y + normal.y * curvature * curveDir
        );

        int segments = 30;
        for (int j = 0; j <= segments; j++) {
            float t = (float)j / (float)segments;
            Vec2 point = bezierQuadratic(p0, controlPoint, p2, t);
            pathVertices.push_back(point.x);
            pathVertices.push_back(point.y);
        }
    }

    glGenVertexArrays(1, &pathVAO);
    glGenBuffers(1, &pathVBO);

    glBindVertexArray(pathVAO);
    glBindBuffer(GL_ARRAY_BUFFER, pathVBO);
    glBufferData(GL_ARRAY_BUFFER, pathVertices.size() * sizeof(float), pathVertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

void setupCircleVAO() {
    std::vector<float> circleVertices;
    int segments = 50;

    for (int i = 0; i <= segments; i++) {
        float angle = (2.0f * 3.14159f * i) / segments;
        circleVertices.push_back(cos(angle));
        circleVertices.push_back(sin(angle));
    }

    glGenVertexArrays(1, &circleVAO);
    glGenBuffers(1, &circleVBO);

    glBindVertexArray(circleVAO);
    glBindBuffer(GL_ARRAY_BUFFER, circleVBO);
    glBufferData(GL_ARRAY_BUFFER, circleVertices.size() * sizeof(float), circleVertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

void setModelMatrix(unsigned int shaderProgram, float x, float y, float width, float height) {
    float model[16] = {
        width, 0.0f, 0.0f, 0.0f,
        0.0f, height, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        x, y, 0.0f, 1.0f
    };
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "uModel"), 1, GL_FALSE, model);
}

void renderTexture(unsigned int texture, float x, float y, float w, float h, float alpha, unsigned int shaderProgram, unsigned int VAO) {
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glUniform1f(glGetUniformLocation(shaderProgram, "uAlpha"), alpha);
    setModelMatrix(shaderProgram, x, y, w, h);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

void renderCircle(float x, float y, float radius, float r, float g, float b, unsigned int shaderProgram) {
    setModelMatrix(shaderProgram, x, y, radius, radius);
    glUniform1f(glGetUniformLocation(shaderProgram, "uAlpha"), 1.0f);
    glUniform3f(glGetUniformLocation(shaderProgram, "uColor"), r, g, b);
    glUniform1i(glGetUniformLocation(shaderProgram, "uUseColor"), 1);

    glBindVertexArray(circleVAO);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 52);

    glUniform1i(glGetUniformLocation(shaderProgram, "uUseColor"), 0);
}

// ========== MAIN ==========
int main()
{
    srand(time(NULL));

    // ========== INICIJALIZACIJA GLFW ==========
    if (!glfwInit()) {
        std::cout << "GLFW nije inicijalizovan!" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    GLFWwindow* window = glfwCreateWindow(mode->width, mode->height, "2D Autobus - Projekat", monitor, NULL);

    if (window == NULL) {
        std::cout << "Prozor nije kreiran!" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, key_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);

    // ========== INICIJALIZACIJA GLEW ==========
    if (glewInit() != GLEW_OK) {
        std::cout << "GLEW nije inicijalizovan!" << std::endl;
        return -1;
    }

    std::cout << "OpenGL verzija: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "GLSL verzija: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;

    // ========== PODESAVANJA OPENGL ==========
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glViewport(0, 0, mode->width, mode->height);
    glLineWidth(3.0f);

    // ========== UCITAVANJE SEJDERA ==========
    std::cout << "\n=== UCITAVANJE SEJDERA ===" << std::endl;
    unsigned int shaderProgram = createShader("Resource Files/Shaders/basic.vert", "Resource Files/Shaders/basic.frag");
    if (shaderProgram == 0) {
        std::cout << "GRESKA: Sejderi nisu ucitani!" << std::endl;
        return -1;
    }
    std::cout << "Sejderi uspesno ucitani!" << std::endl;

    // ========== UCITAVANJE TEKSTURA ==========
    std::cout << "\n=== UCITAVANJE TEKSTURA ===" << std::endl;

    unsigned int busTexture = loadImageToTexture("Resource Files/Textures/2d_bus.png");
    unsigned int stationTexture = loadImageToTexture("Resource Files/Textures/bus_station.png");
    unsigned int controlTexture = loadImageToTexture("Resource Files/Textures/bus_control.png");
    unsigned int doorClosedTexture = loadImageToTexture("Resource Files/Textures/closed_doors.png");
    unsigned int doorOpenTexture = loadImageToTexture("Resource Files/Textures/opened_doors.png");
    unsigned int authorTexture = loadImageToTexture("Resource Files/Textures/author_text.png");
    unsigned int passengersLabelTexture = loadImageToTexture("Resource Files/Textures/passangers_label.png");
    unsigned int finesLabelTexture = loadImageToTexture("Resource Files/Textures/fines.png");

    unsigned int numberTextures[10];
    for (int i = 0; i < 10; i++) {
        std::string path = "Resource Files/Textures/number_" + std::to_string(i) + ".png";
        numberTextures[i] = loadImageToTexture(path.c_str());
    }

    if (busTexture == 0 || stationTexture == 0 || doorClosedTexture == 0 || passengersLabelTexture == 0 || finesLabelTexture == 0) {
        std::cout << "GRESKA: Neke teksture nisu ucitane!" << std::endl;
        return -1;
    }

    std::cout << "=== SVE TEKSTURE USPESNO UCITANE ===" << std::endl;

    // ========== UCITAVANJE KURSORA ==========
    GLFWcursor* customCursor = loadImageToCursor("Resource Files/Cursors/stop_cursor.png");
    if (customCursor != NULL) {
        glfwSetCursor(window, customCursor);
        std::cout << "Kursor uspesno ucitan!" << std::endl;
    }

    // ========== VAO/VBO/EBO ZA TEKSTURE ==========
    float vertices[] = {
        -0.5f, -0.5f,   0.0f, 0.0f,
         0.5f, -0.5f,   1.0f, 0.0f,
         0.5f,  0.5f,   1.0f, 1.0f,
        -0.5f,  0.5f,   0.0f, 1.0f
    };

    unsigned int indices[] = { 0, 1, 2, 2, 3, 0 };

    unsigned int VAO, VBO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    // ========== INICIJALIZACIJA ==========
    initStations();
    setupPathVAO();
    setupCircleVAO();
    auto lastTime = std::chrono::high_resolution_clock::now();

    std::cout << "\n========================================" << std::endl;
    std::cout << "=== PROGRAM POKRENUT ===" << std::endl;
    std::cout << "Kontrole:" << std::endl;
    std::cout << "  Levi klik - dodaj putnika" << std::endl;
    std::cout << "  Desni klik - ukloni putnika" << std::endl;
    std::cout << "  K - kontrola ulazi" << std::endl;
    std::cout << "  ESC - izlaz" << std::endl;
    std::cout << "========================================\n" << std::endl;

    // ========== GLAVNA PETLJA ==========
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        auto currentTime = std::chrono::high_resolution_clock::now();
        std::chrono::duration<float> deltaTime = currentTime - lastTime;

        if (deltaTime.count() < FRAME_TIME) {
            continue;
        }
        float dt = deltaTime.count();
        lastTime = currentTime;

        // ========== LOGIKA ==========
        if (busAtStation) {
            stationTimer += dt;

            if (leftMousePressed) {
                if (passengers < 50) {
                    passengers++;
                    std::cout << "Usao putnik. Ukupno: " << passengers << std::endl;
                }
            }
            if (rightMousePressed) {
                if (passengers > 0) {
                    passengers--;
                    std::cout << "Izasao putnik. Ukupno: " << passengers << std::endl;
                }
            }

            if (keyKPressed && !isInspectorInBus) {
                isInspectorInBus = true;
                passengers++;
                inspectorExitStation = (currentStation + 1) % NUM_STATIONS;
                std::cout << ">>> KONTROLA USLA U AUTOBUS na stanici " << currentStation << " <<<" << std::endl;
            }

            if (stationTimer >= STATION_WAIT_TIME) {
                busAtStation = false;
                stationTimer = 0.0f;
                busProgress = 0.0f;
                std::cout << "Autobus krece ka stanici " << nextStation << std::endl;
            }
        }
        else {
            busProgress += BUS_SPEED * dt;
            if (busProgress >= 1.0f) {
                busProgress = 1.0f;
                busAtStation = true;
                stationTimer = 0.0f;
                currentStation = nextStation;
                nextStation = (currentStation + 1) % NUM_STATIONS;
                std::cout << "Autobus stigao na stanicu " << currentStation << std::endl;

                if (isInspectorInBus && currentStation == inspectorExitStation) {
                    passengers--;
                    int passengersWithoutInspector = passengers;
                    int maxFines = passengersWithoutInspector > 0 ? passengersWithoutInspector : 0;
                    int fines = (maxFines > 0) ? (rand() % (maxFines + 1)) : 0;
                    totalFines += fines;
                    std::cout << ">>> KONTROLA IZASLA na stanici " << currentStation << "! Naplaceno " << fines << " kazni. Ukupno kazni: " << totalFines << " <<<" << std::endl;
                    isInspectorInBus = false;
                    inspectorExitStation = -1;
                }
            }
        }

        leftMousePressed = false;
        rightMousePressed = false;
        keyKPressed = false;

        // ========== RENDEROVANJE ==========
        glClearColor(0.15f, 0.2f, 0.25f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(shaderProgram);

        // ========== PUTANJA (CRVENE KRIVE LINIJE) ==========
        glUniform1i(glGetUniformLocation(shaderProgram, "uUseColor"), 1);
        glUniform3f(glGetUniformLocation(shaderProgram, "uColor"), 0.8f, 0.1f, 0.1f);
        glUniform1f(glGetUniformLocation(shaderProgram, "uAlpha"), 1.0f);

        float identityMatrix[16] = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        };
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "uModel"), 1, GL_FALSE, identityMatrix);

        glBindVertexArray(pathVAO);
        for (int i = 0; i < NUM_STATIONS; i++) {
            glDrawArrays(GL_LINE_STRIP, i * 31, 31);
        }

        glUniform1i(glGetUniformLocation(shaderProgram, "uUseColor"), 0);

        // ========== STANICE (CRVENI KRUGOVI) ==========
        for (int i = 0; i < NUM_STATIONS; i++) {
            renderCircle(stations[i].position.x, stations[i].position.y, 0.06f, 0.8f, 0.1f, 0.1f, shaderProgram);
        }

        // ========== BROJEVI NA STANICAMA (BELI) ==========
        glBindVertexArray(VAO);
        for (int i = 0; i < NUM_STATIONS; i++) {
            renderTexture(numberTextures[i], stations[i].position.x, stations[i].position.y,
                0.05f, 0.06f, 1.0f, shaderProgram, VAO);
        }

        // ========== AUTOBUS ==========
        Vec2 busPos;
        if (busAtStation) {
            busPos = stations[currentStation].position;
        }
        else {
            int prevIdx = currentStation;
            int nextIdx = nextStation;
            Vec2 p0 = stations[prevIdx].position;
            Vec2 p2 = stations[nextIdx].position;

            Vec2 dir = Vec2(p2.x - p0.x, p2.y - p0.y);
            float dist = sqrt(dir.x * dir.x + dir.y * dir.y);
            Vec2 normal = Vec2(-dir.y, dir.x);

            if (dist > 0.0001f) {
                normal.x /= dist;
                normal.y /= dist;
            }

            float curvature = 0.12f + 0.08f * sin(prevIdx * 0.7f);
            float curveDir = (prevIdx % 3 == 0) ? -1.0f : 1.0f;

            Vec2 midPoint = Vec2((p0.x + p2.x) / 2.0f, (p0.y + p2.y) / 2.0f);
            Vec2 controlPoint = Vec2(
                midPoint.x + normal.x * curvature * curveDir,
                midPoint.y + normal.y * curvature * curveDir
            );

            busPos = bezierQuadratic(p0, controlPoint, p2, busProgress);
        }
        renderTexture(busTexture, busPos.x, busPos.y, 0.15f, 0.08f, 1.0f, shaderProgram, VAO);

        // ========== VRATA ==========
        unsigned int doorTexture = busAtStation ? doorOpenTexture : doorClosedTexture;
        renderTexture(doorTexture, -0.85f, 0.75f, 0.12f, 0.18f, 1.0f, shaderProgram, VAO);

        // ========== PUTNICI LABEL ==========
        renderTexture(passengersLabelTexture, -0.90f, -0.65f, 0.20f, 0.08f, 1.0f, shaderProgram, VAO);

        // ========== BROJ PUTNIKA ==========
        int tens = passengers / 10;
        int ones = passengers % 10;
        renderTexture(numberTextures[tens], -0.90f, -0.75f, 0.08f, 0.1f, 1.0f, shaderProgram, VAO);
        renderTexture(numberTextures[ones], -0.80f, -0.75f, 0.08f, 0.1f, 1.0f, shaderProgram, VAO);

        // ========== FINES LABEL ==========
        renderTexture(finesLabelTexture, -0.90f, -0.83f, 0.20f, 0.08f, 1.0f, shaderProgram, VAO);

        // ========== BROJ KAZNI ==========
        int finesTens = (totalFines / 10) % 10;
        int finesOnes = totalFines % 10;
        renderTexture(numberTextures[finesTens], -0.90f, -0.93f, 0.08f, 0.1f, 1.0f, shaderProgram, VAO);
        renderTexture(numberTextures[finesOnes], -0.80f, -0.93f, 0.08f, 0.1f, 1.0f, shaderProgram, VAO);

        // ========== KONTROLA ==========
        if (isInspectorInBus) {
            renderTexture(controlTexture, 0.85f, 0.75f, 0.12f, 0.12f, 1.0f, shaderProgram, VAO);
        }

        // ========== AUTHOR TEXT ==========
        renderTexture(authorTexture, 0.65f, 0.88f, 0.3f, 0.1f, 0.7f, shaderProgram, VAO);

        glfwSwapBuffers(window);
    }

    // ========== CISCENJE ==========
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteVertexArrays(1, &pathVAO);
    glDeleteBuffers(1, &pathVBO);
    glDeleteVertexArrays(1, &circleVAO);
    glDeleteBuffers(1, &circleVBO);
    glDeleteProgram(shaderProgram);

    glDeleteTextures(1, &busTexture);
    glDeleteTextures(1, &stationTexture);
    glDeleteTextures(1, &controlTexture);
    glDeleteTextures(1, &doorClosedTexture);
    glDeleteTextures(1, &doorOpenTexture);
    glDeleteTextures(1, &authorTexture);
    glDeleteTextures(1, &passengersLabelTexture);
    glDeleteTextures(1, &finesLabelTexture);
    glDeleteTextures(10, numberTextures);

    if (customCursor != NULL) {
        glfwDestroyCursor(customCursor);
    }

    glfwDestroyWindow(window);
    glfwTerminate();

    std::cout << "\n=== PROGRAM ZAVRSEN ===" << std::endl;
    return 0;
}