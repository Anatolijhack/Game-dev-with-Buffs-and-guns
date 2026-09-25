#pragma once
#include <algorithm>
#include <cmath>
#include <random>
#include <vector>
#include "System.h"

inline Entity findPlayer(const Registry& reg)
{
	Entity result;
	reg.each<Player>([&](Entity e, Player&) { result = e; });
	return result;
}

// Общий урон по врагу: и обычные пули, и лазер идут через эту функцию,
// чтобы очки начислялись один раз и не дублировалась логика смерти.
inline void damageEnemy(Registry& reg, GameData& data, Entity target, int dmg)
{
	if (!reg.alive(target)) return;
	auto* hp = reg.tryGet<Health>(target);
	if (!hp || hp->hp <= 0) return; // уже помечен на удаление в этом кадре

	hp->hp -= dmg;
	if (hp->hp <= 0)
	{
		if (reg.has<Enemy>(target)) data.score += 10;
		reg.destroyLater(target);
	}
}

// --------------------------------------------------------------------------
class InputSystem : public System
{
	Registry& reg;
	GameData& data; // нужна для мгновенного урона лазера (см. fireLaser)
	InputState input;

public:
	InputSystem(Registry& r, GameData& d) : reg(r), data(d) {}
	void SetInput(const InputState& in) override { input = in; }

	void Update(float dt, ThreadPool&, CommandBuffer& cmd) override
	{
		Entity player = findPlayer(reg);
		if (player.isNull()) return;

		float speed = 100.f;
		if (auto* ms = reg.tryGet<MoveStats>(player)) speed = ms->speed;

		if (auto* vel = reg.tryGet<Velocity>(player))
		{
			vel->x = vel->y = 0.f;
			if (input.left)  vel->x = -speed;
			if (input.right) vel->x = speed;
			if (input.up)    vel->y = -speed;
			if (input.down)  vel->y = speed;
		}

		Weapon* weapon = reg.tryGet<Weapon>(player);
		if (!weapon) return;

		// Пока переключаем клавишами напрямую. Позже то же поле input.numN
		// будет использовать меню апгрейдов, но там - между волнами, здесь - во время боя.
		if (input.num1) weapon->type = WeaponType::Pistol;
		if (input.num2) weapon->type = WeaponType::Shotgun;
		if (input.num3) weapon->type = WeaponType::Laser;

		weapon->timer -= dt;

		auto* pos = reg.tryGet<Position>(player);
		if (!input.shoot || weapon->timer > 0.f || !pos) return;

		// Центр игрока (квадрат 20x20) -> курсор
		float cx = pos->x + 10.f, cy = pos->y + 10.f;
		float dx = input.aimX - cx, dy = input.aimY - cy;
		float len = std::sqrt(dx * dx + dy * dy);
		if (len < 1.f) return; // курсор прямо на игроке: направления нет
		dx /= len;
		dy /= len;

		switch (weapon->type)
		{
		case WeaponType::Pistol:
			fireBullet(cmd, cx, cy, dx, dy, 200.f);
			break;

		case WeaponType::Shotgun:
		{
			const int pellets = 5;
			const float spread = 0.5f; // радианы, суммарный веер
			float baseAngle = std::atan2(dy, dx);
			for (int i = 0; i < pellets; ++i)
			{
				float t = (float)i / (pellets - 1) - 0.5f; // от -0.5 до 0.5
				float angle = baseAngle + t * spread;
				fireBullet(cmd, cx, cy, std::cos(angle), std::sin(angle), 220.f);
			}
			break;
		}

		case WeaponType::Laser:
			fireLaser(cx, cy, dx, dy);
			break;
		}

		weapon->timer = weapon->cooldown;
	}

private:
	void fireBullet(CommandBuffer& cmd, float cx, float cy, float dx, float dy, float bulletSpeed)
	{
		const float spawnDist = 20.f, half = 3.f;
		float bx = cx + dx * spawnDist - half, by = cy + dy * spawnDist - half;
		float vx = dx * bulletSpeed, vy = dy * bulletSpeed;

		cmd.add([=](Registry& r)
			{
				Entity b = r.create();
				r.add<Position>(b, bx, by);
				r.add<Velocity>(b, vx, vy);
				r.add<Bullet>(b, true);
			});
	}

	// Мгновенный луч: находит ближайшего врага на пути и сразу бьёт по нему.
	// Стены луч пока не перекрывают - это упрощение, укрытие от лазера не защищает.
	void fireLaser(float cx, float cy, float dx, float dy)
	{
		const float range = 700.f, hitRadius = 14.f;
		float bestT = range;
		Entity best;

		reg.each<Enemy, Position>([&](Entity e, Enemy&, Position& p)
			{
				float ecx = p.x + 10.f, ecy = p.y + 10.f;
				float t = (ecx - cx) * dx + (ecy - cy) * dy; // проекция центра врага на луч
				if (t < 0.f || t > bestT) return;

				float px = cx + dx * t, py = cy + dy * t; // ближайшая точка луча к врагу
				float dist2 = (px - ecx) * (px - ecx) + (py - ecy) * (py - ecy);
				if (dist2 < hitRadius * hitRadius)
				{
					bestT = t;
					best = e;
				}
			});

		float endX = cx + dx * bestT, endY = cy + dy * bestT;
		Entity beam = reg.create(); // сразу, не через cmd: InputSystem работает в главном потоке
		reg.add<Beam>(beam, cx, cy, endX, endY, 0.08f);

		if (!best.isNull())
			damageEnemy(reg, data, best, 2); // лазер снимает сразу 2 HP
	}
};

// --------------------------------------------------------------------------
// Просто гасит луч через пару кадров - самой отрисовкой занимается RenderSystem.
class BeamSystem : public System
{
	Registry& reg;

public:
	explicit BeamSystem(Registry& r) : reg(r) {}

	void Update(float dt, ThreadPool&, CommandBuffer&) override
	{
		reg.each<Beam>([&](Entity e, Beam& b)
			{
				b.life -= dt;
				if (b.life <= 0.f) reg.destroyLater(e);
			});
	}
};

// --------------------------------------------------------------------------
class EnemySystem : public System
{
	Registry& reg;
	const float baseSpeed = 50.f;
	const float avoidStrength = 1.5f;
	const float avoidRadius2 = 1600.f;

	static float speedMul(EnemyKind k)
	{
		switch (k)
		{
		case EnemyKind::Fast: return 1.8f;
		case EnemyKind::Tank: return 0.6f;
		default:              return 1.0f;
		}
	}

public:
	explicit EnemySystem(Registry& r) : reg(r) {}

	void Update(float dt, ThreadPool&, CommandBuffer& cmd) override
	{
		Entity player = findPlayer(reg);
		auto* pp = player.isNull() ? nullptr : reg.tryGet<Position>(player);
		if (!pp) return;
		const Position target = *pp;

		auto enemies = reg.entitiesWith<Enemy, Position, Velocity>();

		for (Entity e : enemies)
		{
			const Position pos = reg.get<Position>(e);
			const float speed = baseSpeed * speedMul(reg.get<Enemy>(e).kind);

			float dx = target.x - pos.x;
			float dy = target.y - pos.y;
			float len = std::sqrt(dx * dx + dy * dy);
			bool hasDir = len > 0.f;
			if (hasDir) { dx /= len; dy /= len; }

			float avoidX = 0.f, avoidY = 0.f;
			for (Entity other : enemies)
			{
				if (other == e) continue;
				const Position& op = reg.get<Position>(other);
				float ddx = pos.x - op.x, ddy = pos.y - op.y;
				float d2 = ddx * ddx + ddy * ddy;
				if (d2 < avoidRadius2 && d2 > 0.f)
				{
					float d = std::sqrt(d2);
					avoidX += ddx / d;
					avoidY += ddy / d;
				}
			}

			Velocity& vel = reg.get<Velocity>(e);
			vel.x = dx * speed + avoidX * speed * avoidStrength;
			vel.y = dy * speed + avoidY * speed * avoidStrength;

			if (auto* sh = reg.tryGet<Shooter>(e))
			{
				sh->timer -= dt;
				if (sh->timer <= 0.f && hasDir)
				{
					sh->timer = sh->cooldown;
					const float offset = 40.f;
					float bx = pos.x + dx * offset, by = pos.y + dy * offset;
					float vx = dx * 200.f, vy = dy * 200.f; // раньше (int)dx * 200 == 0
					cmd.add([=](Registry& r)
						{
							Entity b = r.create();
							r.add<Position>(b, bx, by);
							r.add<Velocity>(b, vx, vy);
							r.add<Bullet>(b, false);
						});
				}
			}
		}
	}
};

// --------------------------------------------------------------------------
// Каждая сущность обрабатывается ровно одной задачей и пишется напрямую:
// ни командного буфера, ни гонок.
class MoveSystem : public System
{
	Registry& reg;

public:
	explicit MoveSystem(Registry& r) : reg(r) {}

	void Update(float dt, ThreadPool& pool, CommandBuffer&) override
	{
		auto entities = reg.entitiesWith<Position, Velocity>();

		parallel_for_chunks(pool, entities.size(), 256, [&](size_t, size_t begin, size_t end)
			{
				for (size_t j = begin; j < end; ++j)
				{
					Entity e = entities[j];
					const Velocity& v = reg.get<Velocity>(e);
					Position& p = reg.get<Position>(e);
					p.x += v.x * dt;
					p.y += v.y * dt;
				}
			});
	}
};

// --------------------------------------------------------------------------
// Параллельно ТОЛЬКО ищем попадания (чтение). Каждая задача пишет в свой
// results[chunk]. Изменения применяются потом в главном потоке.
class CollisionSystem : public System
{
	Registry& reg;
	GameData& data;
	struct Hit { Entity bullet, target; };

public:
	CollisionSystem(Registry& r, GameData& d) : reg(r), data(d) {}

	void Update(float, ThreadPool& pool, CommandBuffer&) override
	{
		auto bullets = reg.entitiesWith<Bullet, Position>();
		if (bullets.empty()) return;
		auto enemies = reg.entitiesWith<Enemy, Position, Health>();

		Entity player = findPlayer(reg);
		Position playerPos;
		bool hasPlayer = false;
		if (auto* pp = player.isNull() ? nullptr : reg.tryGet<Position>(player))
		{
			playerPos = *pp;
			hasPlayer = reg.has<Health>(player);
		}

		const size_t chunk = 64;
		std::vector<std::vector<Hit>> results(chunk_count(bullets.size(), chunk));
		const float r2 = 400.f;

		parallel_for_chunks(pool, bullets.size(), chunk, [&](size_t c, size_t begin, size_t end)
			{
				for (size_t j = begin; j < end; ++j)
				{
					Entity b = bullets[j];
					const Position& bp = reg.get<Position>(b);
					const Bullet& info = reg.get<Bullet>(b);

					if (info.fromPlayer)
					{
						for (Entity en : enemies)
						{
							const Position& ep = reg.get<Position>(en);
							float dx = bp.x - ep.x, dy = bp.y - ep.y;
							if (dx * dx + dy * dy < r2)
							{
								results[c].push_back({ b, en });
								break;
							}
						}
					}
					else if (hasPlayer)
					{
						float dx = bp.x - playerPos.x, dy = bp.y - playerPos.y;
						if (dx * dx + dy * dy < r2)
							results[c].push_back({ b, player });
					}
				}
			});

		for (auto& list : results)
			for (auto& h : list)
			{
				if (!reg.alive(h.bullet) || !reg.alive(h.target)) continue;

				if (reg.has<Enemy>(h.target))
				{
					damageEnemy(reg, data, h.target, 1);
				}
				else if (auto* hp = reg.tryGet<Health>(h.target)) // цель - игрок, очки не начисляем
				{
					hp->hp--;
					if (hp->hp <= 0) reg.destroyLater(h.target);
				}
				reg.destroyLater(h.bullet);
			}
	}
};

// --------------------------------------------------------------------------
// Пули за пределами экрана удаляются (раньше копились бесконечно).
class BoundsSystem : public System
{
	Registry& reg;
	float w, h, margin = 50.f;
	const float playerSize = 20.f;

public:
	BoundsSystem(Registry& r, float width, float height) : reg(r), w(width), h(height) {}

	void Update(float, ThreadPool&, CommandBuffer&) override
	{
		// игрок не выходит за окно
		reg.each<Player, Position>([&](Entity, Player&, Position& p)
			{
				p.x = std::clamp(p.x, 0.f, w - playerSize);
				p.y = std::clamp(p.y, 0.f, h - playerSize);
			});

		// пули за экраном удаляются
		reg.each<Bullet, Position>([&](Entity e, Bullet&, Position& p)
			{
				if (p.x < -margin || p.x > w + margin || p.y < -margin || p.y > h + margin)
					reg.destroyLater(e);
			});
	}
};

// --------------------------------------------------------------------------
// Враг вплотную к игроку: -1 HP, затем секунда неуязвимости к касаниям
// (иначе 5 врагов снимут всё здоровье за пару кадров).
class ContactDamageSystem : public System
{
	Registry& reg;
	const float size = 20.f;
	const float cooldown = 1.0f;
	float timer = 0.f;

public:
	explicit ContactDamageSystem(Registry& r) : reg(r) {}

	void Update(float dt, ThreadPool&, CommandBuffer&) override
	{
		timer -= dt;
		if (timer > 0.f) return;

		Entity player = findPlayer(reg);
		auto* pp = player.isNull() ? nullptr : reg.tryGet<Position>(player);
		auto* hp = player.isNull() ? nullptr : reg.tryGet<Health>(player);
		if (!pp || !hp) return;

		bool touched = false;
		reg.each<Enemy, Position>([&](Entity, Enemy&, Position& ep)
			{
				if (std::abs(ep.x - pp->x) < size && std::abs(ep.y - pp->y) < size)
					touched = true;
			});

		if (touched)
		{
			hp->hp--;
			timer = cooldown;
			if (hp->hp <= 0) reg.destroyLater(player);
		}
	}
};

// --------------------------------------------------------------------------
// Бесконечные волны. Когда врагов не осталось, через `delay` секунд приходит
// следующая: врагов больше (3 + 2*N, максимум 20), а каждые 3 волны они
// получают +1 HP (максимум 6). В начале каждой новой волны игрок лечится на 1.
// Враги появляются за краем экрана, не рядом с игроком.
class WaveSystem : public System
{
	Registry& reg;
	GameData& data;
	float screenW, screenH;
	std::mt19937 rng{ std::random_device{}() };
	const float delay = 2.0f;
	float timer = 1.0f; // пауза перед первой волной

	struct Spawn { float x, y, shootTimer; EnemyKind kind; };

	float rnd(float a, float b) { return std::uniform_real_distribution<float>(a, b)(rng); }

	// Grunt всегда доступен; Fast подмешивается с волны 3, Tank - с волны 5.
	// Так сложность растёт не только числом врагов, но и их разнообразием.
	EnemyKind randomKind(int wave)
	{
		float r = rnd(0.f, 1.f);
		if (wave >= 5 && r < 0.2f) return EnemyKind::Tank;
		if (wave >= 3 && r < 0.45f) return EnemyKind::Fast;
		return EnemyKind::Grunt;
	}

	Spawn randomSpawn(const Position& playerPos, int wave)
	{
		Spawn s{};
		s.kind = randomKind(wave);
		for (int attempt = 0; attempt < 10; ++attempt)
		{
			switch (std::uniform_int_distribution<int>(0, 3)(rng))
			{
			case 0:  s.x = rnd(0.f, screenW - 20.f); s.y = -20.f;     break; // сверху
			case 1:  s.x = rnd(0.f, screenW - 20.f); s.y = screenH;   break; // снизу
			case 2:  s.x = -20.f;    s.y = rnd(0.f, screenH - 20.f);  break; // слева
			default: s.x = screenW;  s.y = rnd(0.f, screenH - 20.f);  break; // справа
			}
			float dx = s.x - playerPos.x, dy = s.y - playerPos.y;
			if (dx * dx + dy * dy > 150.f * 150.f) break; // не спавним вплотную к игроку
		}
		// стреляют не сразу, чтобы успеть выйти на экран
		s.shootTimer = rnd(1.0f, 2.5f);
		return s;
	}

public:
	WaveSystem(Registry& r, GameData& d, float w, float h) : reg(r), data(d), screenW(w), screenH(h) {}

	void Update(float dt, ThreadPool&, CommandBuffer& cmd) override
	{
		Entity player = findPlayer(reg);
		auto* pp = player.isNull() ? nullptr : reg.tryGet<Position>(player);
		if (!pp) return;

		bool anyEnemy = false;
		reg.each<Enemy>([&](Entity, Enemy&) { anyEnemy = true; });
		if (anyEnemy) { timer = delay; return; } // волна идёт

		timer -= dt;
		if (timer > 0.f) return;

		data.wave++;
		timer = delay;

		if (data.wave > 1)
			if (auto* hp = reg.tryGet<Health>(player))
				hp->hp = std::min(hp->hp + 1, hp->max);

		int count = std::min(3 + 2 * data.wave, 20);
		int baseHp = std::min(3 + (data.wave - 1) / 3, 6);

		std::vector<Spawn> spawns;
		for (int i = 0; i < count; ++i)
			spawns.push_back(randomSpawn(*pp, data.wave));

		cmd.add([spawns, baseHp](Registry& r)
			{
				for (const Spawn& s : spawns)
				{
					// Fast слабее на 1 HP, Tank сильнее на 2, но не меньше 1
					int hpBonus = s.kind == EnemyKind::Fast ? -1 : s.kind == EnemyKind::Tank ? 2 : 0;
					int hp = std::max(1, baseHp + hpBonus);

					Entity e = r.create();
					r.add<Position>(e, s.x, s.y);
					r.add<Velocity>(e);
					r.add<Enemy>(e, s.kind);
					r.add<Shooter>(e, 1.5f, s.shootTimer);
					r.add<Health>(e, hp, hp);
					r.add<Collider>(e);
				}
			});
	}
};

// --------------------------------------------------------------------------
// Толкает игрока и врагов (у кого есть Collider) наружу из стен по оси
// с наименьшим перекрытием (MTV). Стены статичны, поэтому их не двигаем.
class WallCollisionSystem : public System
{
	Registry& reg;

public:
	explicit WallCollisionSystem(Registry& r) : reg(r) {}

	void Update(float, ThreadPool&, CommandBuffer&) override
	{
		auto walls = reg.entitiesWith<Wall, Position>();
		if (walls.empty()) return;

		reg.each<Collider, Position>([&](Entity, Collider& col, Position& pos)
			{
				for (Entity w : walls)
				{
					const Position& wp = reg.get<Position>(w);
					const Wall& wall = reg.get<Wall>(w);

					float overlapX = std::min(pos.x + col.w, wp.x + wall.w) - std::max(pos.x, wp.x);
					float overlapY = std::min(pos.y + col.h, wp.y + wall.h) - std::max(pos.y, wp.y);
					if (overlapX <= 0.f || overlapY <= 0.f) continue;

					float ecx = pos.x + col.w / 2.f, ecy = pos.y + col.h / 2.f;
					float wcx = wp.x + wall.w / 2.f, wcy = wp.y + wall.h / 2.f;

					if (overlapX < overlapY)
						pos.x += (ecx < wcx) ? -overlapX : overlapX;
					else
						pos.y += (ecy < wcy) ? -overlapY : overlapY;
				}
			});
	}
};

// --------------------------------------------------------------------------
// Пули (свои и вражеские) уничтожаются при попадании в стену - укрытие
// от них защищает.
class BulletWallSystem : public System
{
	Registry& reg;
	const float bulletSize = 6.f;

public:
	explicit BulletWallSystem(Registry& r) : reg(r) {}

	void Update(float, ThreadPool&, CommandBuffer&) override
	{
		auto walls = reg.entitiesWith<Wall, Position>();
		if (walls.empty()) return;

		reg.each<Bullet, Position>([&](Entity e, Bullet&, Position& p)
			{
				for (Entity w : walls)
				{
					const Position& wp = reg.get<Position>(w);
					const Wall& wall = reg.get<Wall>(w);
					if (p.x < wp.x + wall.w && p.x + bulletSize > wp.x &&
						p.y < wp.y + wall.h && p.y + bulletSize > wp.y)
					{
						reg.destroyLater(e);
						break;
					}
				}
			});
	}
};