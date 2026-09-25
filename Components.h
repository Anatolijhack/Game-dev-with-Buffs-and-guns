//#pragma once
//
//struct Position { float x = 0.f; float y = 0.f; };
//struct Velocity { float x = 0.f; float y = 0.f; };
//
//struct Player {};
//enum class EnemyKind { Grunt, Fast, Tank };
//struct Enemy { EnemyKind kind = EnemyKind::Grunt; };
//enum class WeaponKind { Pistol,Laser, Shootgun };
//struct Weapon { WeaponKind kind = WeaponKind::Pistol; };
//struct Bullet { bool fromPlayer = true; }; // владелец снаряда
//
//struct Health
//{
//	int hp = 1;
//	int max = 1;
//};
//
//struct Shooter
//{
//	float cooldown = 1.5f;
//	float timer = 0.0f;
//};
//
//struct InputState
//{
//	bool left = false;
//	bool right = false;
//	bool up = false;
//	bool down = false;
//	bool shoot = false;
//	bool restart = false;
//	float aimX = 0.f; // позиция курсора в координатах мира
//	float aimY = 0.f;
//};
//
//struct Wall { float w = 40.f; float h = 40.f; };       // статичное препятствие
//struct Collider { float w = 20.f; float h = 20.f; };   // кто не проходит сквозь стены
//
//struct GameData
//{
//	int score = 0;
//	int wave = 0; // номер текущей волны (0 - ещё не началась)
//};
#pragma once

struct Position
{
    float x = 0.f;
    float y = 0.f;
};

struct Velocity
{
    float x = 0.f;
    float y = 0.f;
};

// Скорость движения сущности
struct MoveStats
{
    float speed = 100.f;
};

struct Player {};

enum class EnemyKind
{
    Grunt,
    Fast,
    Tank
};

struct Enemy
{
    EnemyKind kind = EnemyKind::Grunt;
};

enum class WeaponType
{
    Pistol,
    Shotgun,
    Laser
};

struct Weapon
{
    WeaponType type = WeaponType::Pistol;
    float cooldown = 0.3f;
    float timer = 0.f;
};

struct Bullet
{
    bool fromPlayer = true;
};

struct Beam
{
    float x1 = 0.f;
    float y1 = 0.f;
    float x2 = 0.f;
    float y2 = 0.f;
    float life = 0.f;
};

struct Health
{
    int hp = 1;
    int max = 1;
};

struct Shooter
{
    float cooldown = 1.5f;
    float timer = 0.0f;
};

struct InputState
{
    bool left = false;
    bool right = false;
    bool up = false;
    bool down = false;
    bool shoot = false;
    bool restart = false;

    float aimX = 0.f;
    float aimY = 0.f;

    bool num1 = false;
    bool num2 = false;
    bool num3 = false;
};

struct Wall
{
    float w = 40.f;
    float h = 40.f;
};

struct Collider
{
    float w = 20.f;
    float h = 20.f;
};

struct GameData
{
    int score = 0;
    int wave = 0;
};