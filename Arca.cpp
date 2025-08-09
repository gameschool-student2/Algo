#include "framework.h"
#include "Arca.h"
#include <objidl.h>
#include <gdiplus.h>
#include <unordered_map>
#include <memory>
#include <string>
#include <algorithm> 
#include <ctime>

using namespace Gdiplus;
using namespace std;

#pragma comment(lib, "gdiplus.lib")

#define MAX_LOADSTRING 100
#define WIN32_LEAN_AND_MEAN  

// Глобальные переменные:
HINSTANCE hInst;                                // текущий экземпляр
WCHAR szTitle[MAX_LOADSTRING];                  // Текст строки заголовка
WCHAR szWindowClass[MAX_LOADSTRING];            // имя класса главного окна

bool g_keyLeftPressed = false;
bool g_keyRightPressed = false;
int lives = 3;
bool gameStarted = false;

class SpriteManager {
private:
    std::unordered_map<int, std::unique_ptr<Bitmap>> sprites;

public:
    ~SpriteManager() {
        sprites.clear();
    }

    bool LoadSprite(const std::wstring& path, int id) {
        if (sprites.find(id) != sprites.end()) {
            return false; 
        }

        auto bitmap = std::make_unique<Bitmap>(path.c_str());
        if (bitmap->GetLastStatus() != Ok) {
            return false;
        }

        sprites[id] = std::move(bitmap);
        return true;
    }

    Bitmap* GetSprite(int id) const {
        auto it = sprites.find(id);
        return it != sprites.end() ? it->second.get() : nullptr;
    }
    void Clear() {
        sprites.clear();
    }
};

SpriteManager spriteManager;

class Ball {
private:
    float newX, newY;
    float vx, vy;
    float radius;
    int spriteId;
    bool isActive;
    float prevX, prevY;

public:
    Ball(float startX, float startY, float r, int spriteID = 1, float prevX, float prevY)
        : newX(startX), newY(startY), radius(r), isActive(true), spriteId(spriteID), prevX(prevX), prevY(prevY) {
        vx = 0.0f;
        vy = 0.0f;
    }

    void Draw(Graphics& graphics) {
        if (!isActive) return;

        Bitmap* sprite = spriteManager.GetSprite(spriteId);
        if (sprite) {
            graphics.DrawImage(sprite, newX - radius, newY - radius, radius * 2, radius * 2);
        }
        else {
            SolidBrush brush(Color(255, 255, 255));
            graphics.FillEllipse(&brush, newX - radius, newY - radius, radius * 2, radius * 2);
        }
    }

    void Update() {
        if (!isActive) return;
        newX = prevX + vx;
        newY = prevY + vy;
    }

    float GetX() const { return newX; }
    float GetY() const { return newY; }
    float GetRadius() const { return radius; }
    float GetVX() const { return vx; }
    float GetVY() const { return vy; }
    void SetVX(float newVX) { vx = newVX; }
    void SetVY(float newVY) { vy = newVY; }
};

struct Cords {
    float x,y;
};

struct Line {
    Cords p1, p2;
};

class Brick {
private:
    float x, y;
    float width, height;
    int spriteId;
    bool isDestroyed;
    int hitPoints;
    Line left, top, right, bottom;

public:
    Brick(float posX, float posY, float w, float h, int spriteID = 1001, int hp = 1)
        : x(posX), y(posY), width(w), height(h), spriteId(spriteID),
        isDestroyed(false), hitPoints(hp) {
    }

    void SidesUpdate(float x, float y, float width, float height) {
        left.p1 = { x, y };
        left.p2 = { x, (y + height) };

        top.p1 = { x, y };
        top.p2 = { (x + width), y };

        right.p1 = { (x + width), y };
        right.p2 = { (x + width), (y + height) };

        bottom.p1 = { x, (y + height) };
        bottom.p2 = { (x + width), (y + height) };
    }

    void Draw(Graphics& graphics) {
        if (isDestroyed) return;

        Bitmap* sprite = spriteManager.GetSprite(spriteId);
        if (sprite) {
            graphics.DrawImage(sprite, x, y, width, height);
        }
        else {
            Color brickColor = hitPoints > 1 ? Color(255, 0, 0) : Color(0, 255, 0);
            SolidBrush brush(brickColor);
            graphics.FillRectangle(&brush, x, y, width, height);
        }
    }

    bool CheckCollision(Ball& ball) {
        if (isDestroyed) return false;

        if (ball.GetX() + ball.GetRadius() > x &&
            ball.GetX() - ball.GetRadius() < x + width &&
            ball.GetY() + ball.GetRadius() > y &&
            ball.GetY() - ball.GetRadius() < y + height) {

            hitPoints--;
            if (hitPoints <= 0) {
                Destroy();
            }
            return true;
        }
        return false;
    }

    void Destroy() {
        isDestroyed = true;
    }

    bool IsDestroyed() const { return isDestroyed; }
    float GetX() const { return x; }
    float GetY() const { return y; }
    float GetWidth() const { return width; }
    float GetHeight() const { return height; }
};

class Racket {
private:
    float x, y;
    float width, height;
    int spriteId;
    bool isActive;

public:
    Racket(float startX, float startY, float w, float h, int spriteID = 2001)
        : x(startX), y(startY), width(w), height(h), spriteId(spriteID), isActive(true) {
    }

    void Draw(Graphics& graphics) {
        if (!isActive) return;

        Bitmap* sprite = spriteManager.GetSprite(spriteId);
        if (sprite) {
            graphics.DrawImage(sprite, x, y, width, height);
        }
        else {
            SolidBrush brush(Color(255, 255, 255));
            graphics.FillRectangle(&brush, x, y, width, height);
        }
    }

    void Update(float newX) {
        x = newX - width / 2;
    }

    float GetX() const { return x; }
    float GetY() const { return y; }
    float GetWidth() const { return width; }
    float GetHeight() const { return height; }
};

Racket* pRacket = nullptr;
vector<Brick> bricks;
Ball* pBall = nullptr;

ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    srand(static_cast<unsigned int>(time(nullptr)));

    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_ARCA, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    spriteManager.LoadSprite(L"assets/ball.png", 1);
    spriteManager.LoadSprite(L"assets/brick.jpg", 1001);
    spriteManager.LoadSprite(L"assets/racket.jpg", 2001);


    if (!InitInstance(hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_ARCA));

    MSG msg;
    ZeroMemory(&msg, sizeof(msg));
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
        else
        {
            Sleep(1);
        }
    }

    spriteManager.Clear();
    GdiplusShutdown(gdiplusToken);
    return (int)msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ARCA));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 3);
    wcex.lpszMenuName = nullptr;
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance;
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_POPUP | WS_VISIBLE,
        0, 0, screenWidth, screenHeight, nullptr, nullptr, hInstance, nullptr);

    if (!hWnd)
    {
        return FALSE;
    }

    SetTimer(hWnd, 1, 8, NULL); 
    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    return TRUE;
}

void ResetGameObjects(int screenWidth, int screenHeight) {
    if (pBall) {
        delete pBall;
        pBall = new Ball(screenWidth / 2, screenHeight - 100, 20, 1);
    }
    if (pRacket) {
        pRacket->Update(screenWidth / 2);
    }
}

void ResetLevel(HWND hWnd) {
    bricks.clear();

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    ResetGameObjects(screenWidth, screenHeight);

    int brickWidth = 128;
    int brickHeight = 64;
    int brickSpacing = 10;
    int numCols = 10;
    int numRows = 5;
    int gridWidth = numCols * brickWidth + (numCols - 1) * brickSpacing;
    int gridHeight = numRows * brickHeight + (numRows - 1) * brickSpacing;
    int startX = (screenWidth - gridWidth) / 2;
    int startY = (screenHeight - gridHeight) / 2.5;

    for (int row = 0; row < numRows; row++) {
        for (int col = 0; col < numCols; col++) {
            bricks.emplace_back(
                startX + col * (brickWidth + brickSpacing),
                startY + row * (brickHeight + brickSpacing),
                brickWidth, brickHeight,
                1001,
                row % 2 + 1
            );
        }
    }

    gameStarted = false;

    InvalidateRect(hWnd, NULL, TRUE);
}


LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE: {
        int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        int screenHeight = GetSystemMetrics(SM_CYSCREEN);

        pBall = new Ball(screenWidth / 2, screenHeight - 100, 20, 1);
        pRacket = new Racket(screenWidth / 2 - 50, screenHeight - 30, 100, 15, 2001);
        gameStarted = false;

        int brickWidth = 128;
        int brickHeight = 64;
        int brickSpacing = 10;
        int numCols = 10;
        int numRows = 5;
        int gridWidth = numCols * brickWidth + (numCols - 1) * brickSpacing;
        int gridHeight = numRows * brickHeight + (numRows - 1) * brickSpacing;
        int startX = (screenWidth - gridWidth) / 2;
        int startY = (screenHeight - gridHeight) / 2.5;

        for (int row = 0; row < numRows; row++) {
            for (int col = 0; col < numCols; col++) {
                bricks.emplace_back(
                    startX + col * (brickWidth + brickSpacing),
                    startY + row * (brickHeight + brickSpacing),
                    brickWidth, brickHeight,
                    1001,
                    row % 2 + 1
                );
            }
        }
        break;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        HDC hdcMem = CreateCompatibleDC(hdc);
        RECT rect;
        GetClientRect(hWnd, &rect);
        HBITMAP hbmMem = CreateCompatibleBitmap(hdc, rect.right, rect.bottom);
        HBITMAP hbmOld = (HBITMAP)SelectObject(hdcMem, hbmMem);

        Graphics graphics(hdcMem);
        SolidBrush bgBrush(Color(0, 0, 0));
        graphics.FillRectangle(&bgBrush, 0, 0, rect.right, rect.bottom);



        FontFamily fontFamily(L"Arial");
        Font font(&fontFamily, 24, FontStyleRegular, UnitPixel);
        SolidBrush whiteBrush(Color(255, 255, 255));

        wstring livesText = L"Lives: " + to_wstring(lives);
        PointF textPoint(rect.right / 2.0f - 50, 10.0f);
        graphics.DrawString(livesText.c_str(), -1, &font, textPoint, &whiteBrush);

        for (auto& brick : bricks) {
            brick.Draw(graphics);
        }
        if (pRacket) pRacket->Draw(graphics);
        if (pBall) pBall->Draw(graphics);

        BitBlt(hdc, 0, 0, rect.right, rect.bottom, hdcMem, 0, 0, SRCCOPY);

        SelectObject(hdcMem, hbmOld);
        DeleteObject(hbmMem);
        DeleteDC(hdcMem);

        EndPaint(hWnd, &ps);
        break;
    }
    case WM_KEYDOWN:
        if (wParam == VK_SPACE && !gameStarted) {
            gameStarted = true;
            if (pBall) {
                float direction = (rand() % 2) ? 1.0f : -1.0f;
                pBall->SetVX((rand() % 4 + 2.5f) * direction);
                pBall->SetVY(-4.0f);
            }
        }
        if (wParam == 'A' || wParam == VK_LEFT) {
            g_keyLeftPressed = true;
        }
        else if (wParam == 'D' || wParam == VK_RIGHT) {
            g_keyRightPressed = true;
        }
        break;

    case WM_KEYUP:
        if (wParam == 'A' || wParam == VK_LEFT) {
            g_keyLeftPressed = false;
        }
        else if (wParam == 'D' || wParam == VK_RIGHT) {
            g_keyRightPressed = false;
        }
        break;

    case WM_TIMER: {
        int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        int screenHeight = GetSystemMetrics(SM_CYSCREEN);


        if (pBall && gameStarted) {
            pBall->Update();

            if (pBall->GetX() - pBall->GetRadius() <= 0 ||
                pBall->GetX() + pBall->GetRadius() >= screenWidth) {
                pBall->SetVX(-pBall->GetVX());
            }
            if (pBall->GetY() - pBall->GetRadius() <= 0) {
                pBall->SetVY(-pBall->GetVY());
            }

            if (pBall->GetY() - pBall->GetRadius() > screenHeight) {
                lives--;
                if (lives <= 0) {
                    lives = 3;
                    ResetLevel(hWnd);
                }
                else {
                    gameStarted = false;
                    ResetGameObjects(screenWidth, screenHeight);
                }
            }


            if (pRacket && pBall->GetY() + pBall->GetRadius() >= pRacket->GetY() &&
                pBall->GetY() - pBall->GetRadius() <= pRacket->GetY() + pRacket->GetHeight() &&
                pBall->GetX() + pBall->GetRadius() >= pRacket->GetX() &&
                pBall->GetX() - pBall->GetRadius() <= pRacket->GetX() + pRacket->GetWidth()) {

                pBall->SetVY(-abs(pBall->GetVY()) * (1.02f + (rand() % 5 / 100.0f)));
                pBall->SetVX(pBall->GetVX() * (1.02f + (rand() % 5 / 100.0f)));
            }
            for (auto& brick : bricks) {
                if (brick.CheckCollision(*pBall)) {
                    float ballLeft = pBall->GetX() - pBall->GetRadius();
                    float ballRight = pBall->GetX() + pBall->GetRadius();
                    float ballTop = pBall->GetY() - pBall->GetRadius();
                    float ballBottom = pBall->GetY() + pBall->GetRadius();

                    float brickLeft = brick.GetX();
                    float brickRight = brick.GetX() + brick.GetWidth();
                    float brickTop = brick.GetY();
                    float brickBottom = brick.GetY() + brick.GetHeight();

                    float overlapLeft = ballRight - brickLeft;
                    float overlapRight = brickRight - ballLeft;
                    float overlapTop = ballBottom - brickTop;
                    float overlapBottom = brickBottom - ballTop;

                    float minOverlap = min(min(overlapLeft, overlapRight), min(overlapTop, overlapBottom));

                    if (minOverlap == overlapLeft || minOverlap == overlapRight) {
                        pBall->SetVX(-pBall->GetVX()); 
                    }
                    else {
                        pBall->SetVY(-pBall->GetVY()); 
                    }
                    break;
                }
            }

            InvalidateRect(hWnd, NULL, FALSE);
        }
        if (g_keyLeftPressed || g_keyRightPressed) {
            int screenWidth = GetSystemMetrics(SM_CXSCREEN);
            float currentCenterX = pRacket->GetX() + pRacket->GetWidth() / 2;
            float step = 15.0f;

            if (g_keyLeftPressed) {
                currentCenterX -= step;
                if (currentCenterX < pRacket->GetWidth() / 2) {
                    currentCenterX = pRacket->GetWidth() / 2;
                }
            }
            if (g_keyRightPressed) {
                currentCenterX += step;
                if (currentCenterX > screenWidth - pRacket->GetWidth() / 2) {
                    currentCenterX = screenWidth - pRacket->GetWidth() / 2;
                }
            }
            pRacket->Update(currentCenterX);
        }
        break;
    }
    case WM_DESTROY: {
        delete pBall;
        delete pRacket;
        spriteManager.Clear();
        KillTimer(hWnd, 1);
        PostQuitMessage(0);
        break;
    }
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}
