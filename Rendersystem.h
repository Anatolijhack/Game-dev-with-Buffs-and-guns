#pragma once
#include <SFML/Graphics.hpp>
#include "System.h"

class RenderSystem : public System
{
	Registry& reg;
	sf::RenderWindow& window;

	void drawBar(float x, float y, float w, float ratio)
	{
		sf::RectangleShape bg({ w, 3.f });
		bg.setPosition({ x, y });
		bg.setFillColor(sf::Color(40, 40, 40));
		window.draw(bg);

		sf::RectangleShape fg({ w * ratio, 3.f });
		fg.setPosition({ x, y });
		fg.setFillColor(sf::Color(220, 40, 40));
		window.draw(fg);
	}

public:
	RenderSystem(Registry& r, sf::RenderWindow& w) : reg(r), window(w) {}

	void Render(float) override
	{
		sf::RectangleShape rect;

		// стены рисуем отдельно: они не проходят по общей each<Position>-ветке ниже
		reg.each<Wall, Position>([&](Entity, Wall& wall, Position& pos)
			{
				sf::RectangleShape w({ wall.w, wall.h });
				w.setPosition({ pos.x, pos.y });
				w.setFillColor(sf::Color(110, 110, 120));
				window.draw(w);
			});

		// луч лазера: без поворотов, просто ряд точек вдоль линии
		reg.each<Beam>([&](Entity, Beam& b)
			{
				sf::RectangleShape dot({ 4.f, 4.f });
				dot.setFillColor(sf::Color(255, 60, 60));
				const int steps = 10;
				for (int i = 0; i <= steps; ++i)
				{
					float t = (float)i / steps;
					dot.setPosition({ b.x1 + (b.x2 - b.x1) * t - 2.f, b.y1 + (b.y2 - b.y1) * t - 2.f });
					window.draw(dot);
				}
			});

		reg.each<Position>([&](Entity e, Position& pos)
			{
				if (reg.has<Wall>(e)) return; // уже нарисована выше
				rect.setPosition(sf::Vector2f(pos.x, pos.y));

				if (reg.has<Player>(e))
				{
					rect.setSize({ 20.f, 20.f });
					rect.setFillColor(sf::Color::Green);
					window.draw(rect);
				}
				else if (auto* en = reg.tryGet<Enemy>(e))
				{
					rect.setSize({ 20.f, 20.f });
					switch (en->kind)
					{
					case EnemyKind::Fast: rect.setFillColor(sf::Color(255, 165, 0));  break; // оранжевый
					case EnemyKind::Tank: rect.setFillColor(sf::Color(140, 20, 20));  break; // тёмно-красный
					default:              rect.setFillColor(sf::Color::Red);         break;
					}
					window.draw(rect);

					// полоска здоровья над врагом
					if (auto* h = reg.tryGet<Health>(e))
						drawBar(pos.x, pos.y - 6.f, 20.f, h->max > 0 ? (float)h->hp / h->max : 0.f);
				}
				else if (auto* b = reg.tryGet<Bullet>(e))
				{
					rect.setSize({ 6.f, 6.f });
					rect.setFillColor(b->fromPlayer ? sf::Color::Yellow : sf::Color(255, 140, 0));
					window.draw(rect);
				}
			});
	}
};