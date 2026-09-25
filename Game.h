//#pragma once
//#include "Registry.h"
//#include "Components.h"
//#include "GameSystems.h" // findPlayer
//
//// Victory больше не наступает: волны бесконечные. Значение оставлено, чтобы не менять Hud.
//enum class GameState { Playing, GameOver, Victory };
//
//// Пересоздаёт мир: остаётся только игрок. Врагов приводит WaveSystem.
//inline void resetWorld(Registry& reg)
//{
//	reg.clear();
//
//	Entity player = reg.create();
//	reg.add<Player>(player);
//	reg.add<Position>(player, 390.f, 290.f); // по центру: врагов теперь не видно на старте
//	reg.add<Velocity>(player);
//	reg.add<Health>(player, 5, 5);
//	reg.add<Weapon>(player);
//	reg.add<Collider>(player);
//
//	auto addWall = [&](float x, float y, float w, float h)
//		{
//			Entity e = reg.create();
//			reg.add<Position>(e, x, y);
//			reg.add<Wall>(e, w, h);
//		};
//	// простая планировка: четыре укрытия вокруг центра, старт игрока свободен
//	addWall(250.f, 150.f, 300.f, 30.f);  // сверху
//	addWall(250.f, 420.f, 300.f, 30.f);  // снизу
//	addWall(150.f, 250.f, 30.f, 100.f);  // слева
//	addWall(620.f, 250.f, 30.f, 100.f);  // справа
//}
//
//inline GameState evaluateState(const Registry& reg)
//{
//	return findPlayer(reg).isNull() ? GameState::GameOver : GameState::Playing;
//}
#pragma once
#include <algorithm>
#include "Registry.h"
#include "Components.h"
#include "GameSystems.h" // findPlayer

// Victory больше не наступает: волны бесконечные. Значение оставлено, чтобы не менять Hud.
enum class GameState { Playing, GameOver, Victory };

// Пересоздаёт мир: остаётся только игрок. Врагов приводит WaveSystem.
inline void resetWorld(Registry& reg)
{
	reg.clear();

	Entity player = reg.create();
	reg.add<Player>(player);
	reg.add<Position>(player, 390.f, 290.f); // по центру: врагов теперь не видно на старте
	reg.add<Velocity>(player);
	reg.add<Health>(player, 5, 5);
	reg.add<Collider>(player);
	reg.add<Weapon>(player);    // по умолчанию пистолет, cooldown 0.35
	reg.add<MoveStats>(player); // по умолчанию скорость 100

	auto addWall = [&](float x, float y, float w, float h)
		{
			Entity e = reg.create();
			reg.add<Position>(e, x, y);
			reg.add<Wall>(e, w, h);
		};
	// простая планировка: четыре укрытия вокруг центра, старт игрока свободен
	addWall(250.f, 150.f, 300.f, 30.f);  // сверху
	addWall(250.f, 420.f, 300.f, 30.f);  // снизу
	addWall(150.f, 250.f, 30.f, 100.f);  // слева
	addWall(620.f, 250.f, 30.f, 100.f);  // справа
}

inline GameState evaluateState(const Registry& reg)
{
	return findPlayer(reg).isNull() ? GameState::GameOver : GameState::Playing;
}

// --------------------------------------------------------------------------
// Бафы между волнами. Каждый просто меняет один компонент игрока,
// поэтому применение - это switch без побочных структурных изменений.
enum class UpgradeKind { MaxHp, Heal, FireRate, Speed, Shotgun, Laser };

inline const char* upgradeName(UpgradeKind k)
{
	switch (k)
	{
	case UpgradeKind::MaxHp:    return "Max HP +1";
	case UpgradeKind::Heal:     return "Heal +2";
	case UpgradeKind::FireRate: return "Fire rate +20%";
	case UpgradeKind::Speed:    return "Move speed +15%";
	case UpgradeKind::Shotgun:  return "Weapon: Shotgun";
	case UpgradeKind::Laser:    return "Weapon: Laser";
	}
	return "";
}

inline void applyUpgrade(Registry& reg, Entity player, UpgradeKind kind)
{
	switch (kind)
	{
	case UpgradeKind::MaxHp:
		if (auto* h = reg.tryGet<Health>(player)) { h->max++; h->hp = std::min(h->hp + 1, h->max); }
		break;
	case UpgradeKind::Heal:
		if (auto* h = reg.tryGet<Health>(player)) h->hp = std::min(h->hp + 2, h->max);
		break;
	case UpgradeKind::FireRate:
		if (auto* w = reg.tryGet<Weapon>(player)) w->cooldown = std::max(0.1f, w->cooldown * 0.8f);
		break;
	case UpgradeKind::Speed:
		if (auto* ms = reg.tryGet<MoveStats>(player)) ms->speed *= 1.15f;
		break;
	case UpgradeKind::Shotgun:
		if (auto* w = reg.tryGet<Weapon>(player)) { w->type = WeaponType::Shotgun; w->cooldown = 0.55f; }
		break;
	case UpgradeKind::Laser:
		if (auto* w = reg.tryGet<Weapon>(player)) { w->type = WeaponType::Laser; w->cooldown = 0.45f; }
		break;
	}
}