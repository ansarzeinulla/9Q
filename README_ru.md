# 9Q: Сложность дерева игры Тогызкумалак и анализ миллиарда партий

*Читать на других языках: [English](README.md) | [Қазақша](README_kk.md) | [Русский](README_ru.md) | [Кыргызча](README_ky.md)*

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++17](https://img.shields.io/badge/C++-17-blue.svg)](https://isocpp.org/)

Этот репозиторий содержит официальный самодостаточный исходный код на C++ для статьи:

**Комбинаторное пространство состояний и эмпирическая сложность дерева игры Тогызкумалак: анализ миллиарда партий**  
*Ansar Zeinulla (2026)* | [Читать статью на arXiv](Not uploaded yet)

## Структура репозитория

Пакет содержит три запускаемых C++ компонента, необходимых для воспроизведения вычислительных результатов статьи:

| Компонент | Назначение |
| :--- | :--- |
| `billion-game-generation/` | Высокопроизводительный C++ симулятор случайных playout-ов, использованный для генерации 1,000,000,000 партий и извлечения эмпирических метрик. |
| `shortest-game-generation/` | Исчерпывающий поисковый движок ранней игры для доказательства и проверки самой короткой возможной терминальной партии (11 полуходов). |
| `engine/` | Основной движок правил Тогызкумалак и вручную созданный оценочный агент Minimax с Alpha-Beta отсечением. |

*Примечание: Вне этого репозитория не требуется никаких зависимостей исходного кода. Симулятор миллиарда партий намеренно использует `../engine/togyzkumalak_rules.cpp` и `../engine/position_hash.cpp`, которые подключаются напрямую.*

## Требования

- Компилятор C++17 (`clang++` по умолчанию или `g++` через `CXX=g++`)
- `make`
- POSIX-совместимый терминал

*Для сборки и запуска основного C++ движка не требуются Python-пакеты, библиотеки нейронных сетей или внешние датасеты.*

## Инструкции по сборке

Чтобы скомпилировать все модули, выполните из корневого каталога:

```bash
make -C engine
make -C billion-game-generation
make -C shortest-game-generation
```

## Быстрая проверка (тестирование)

Запустите небольшой случайный пример на 1,000 партий, чтобы проверить движок симуляции:
```bash
cd billion-game-generation
./generate_billion_games --num=1000 --seed=1 --threads=1 --fresh --stat=sample_billion_game_statistics.txt
```

Проверьте доказательство самой короткой партии длиной 11 полуходов:
```bash
cd ../shortest-game-generation
./find_shortest_game --verify-only
```

Сыграйте матч против вручную созданного Minimax AI:
```bash
cd ../engine
./togyzkumalak_engine --mode human-ai4
```
*(Для машин с малым объемом памяти используйте `ai2` или `ai3` вместо `ai4`).*

## Команды полного воспроизведения

Чтобы полностью воспроизвести случайную симуляцию миллиарда партий (предупреждение: требует очень больших вычислительных ресурсов):
```bash
cd billion-game-generation
./generate_billion_games --num=1000000000 --seed=709791810521833 --threads=10 --fresh --stat=billion_game_reproduction_statistics.txt
```
*Примечание: Включенный файл `billion_game_statistics.txt` уже содержит итоговые счетчики, использованные в статье. При воспроизведении с нуля используйте новый путь вывода.*

Запустите полное математическое доказательство самой короткой партии:
```bash
cd shortest-game-generation
./find_shortest_game
```
Это заново создаст `shortest_terminal_game.txt`, `shortest_candidate_replay.tsv` и `shortest_depth_proof.tsv`.

Запустите бенчмарк self-play для движка Minimax:
```bash
cd engine
./togyzkumalak_engine --benchmark --bot ai --depth 4 --positions 100 --threads 1 --noterminal
```

## Цитирование

Если вы используете этот код, движок Тогызкумалак или датасет 1 миллиарда партий в своем исследовании, пожалуйста, процитируйте эту статью:

```bibtex
@misc{zeinulla2026togyzkumalak,
      title={Combinatorial State-Space and Empirical Game Tree Complexity of Togyzkumalak: A Billion-Game Analysis}, 
      author={Ansar Zeinulla},
      year={2026},
      eprint={Not uploaded yet},
      archivePrefix={Not uploaded yet},
      primaryClass={cs.AI}
}
```
