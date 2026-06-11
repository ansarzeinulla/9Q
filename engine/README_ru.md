# Ручной оценочный движок

*Читать на других языках: [English](README.md) | [Қазақша](README_kk.md) | [Русский](README_ru.md) | [Кыргызча](README_ky.md)*

Эта папка содержит C++ движок правил Тогызкумалак и игровой движок командной строки.

Для исследовательского пакета основной необходимый движок - ручной minimax-движок:

- Имя бота: `aiN`
- Исходный код: `minimax_engine.cpp` и `minimax_engine.hpp`
- Поиск: minimax фиксированной глубины с alpha-beta отсечением, упорядочиванием ходов, таблицей транспозиций и тактическим quiescence search
- Оценка: ручной эвристический оценщик, доступный через `HeuristicEvaluator` и `ToguzEnv::evaluate`

Папка также включает более новую реализацию поиска `dagN` / `filterN`, но `aiN` является minimax/оценочным движком, используемым как основной baseline.

## Файлы

| Файл | Назначение |
| :--- | :--- |
| `togyzkumalak_rules.*` | Представление доски, легальные ходы, правила, определение терминальных состояний, отслеживание повторений и эвристическая оценка. |
| `minimax_engine.*` | Ручной minimax/quiescence движок. |
| `dag_search.*` | DAG/filter поисковый движок, включенный для сравнения. |
| `evaluation.*` | Интерфейс оценщика и статистически информированная эвристическая обертка. |
| `position_hash.*` | Хеширование для логики транспозиций и повторений. |
| `togyzkumalak_engine.cpp` | CLI-точка входа для игры, self-play, сравнения, benchmark и UCI-подобных команд. |
| `Makefile` | Собирает бинарный файл `togyzkumalak_engine`. |

## Сборка

```bash
cd FINAL/engine
make
```

Чтобы использовать `g++` вместо `clang++`:

```bash
make CXX=g++
```

## Игра против Minimax-движка

```bash
./togyzkumalak_engine --mode human-ai
```

Движок использует iterative deepening с часами. По умолчанию на каждый полуход дается
`0.01` секунды. Измените бюджет через `--move-time`:

```bash
./togyzkumalak_engine --mode human-ai --move-time 0.05
```

## Запуск автоматических игр

```bash
./togyzkumalak_engine --mode ai-randombot --numgames 20 --noterminal
./togyzkumalak_engine --mode ai-ai --numgames 10 --noterminal --switch-color --move-time 0.01
```

## Имена ботов

| Бот | Значение |
| :--- | :--- |
| `ai` / `aiN` | Ручной minimax/quiescence движок, использующий часы на каждый ход; `N` принимается для совместимости со старыми командами, но обычные игры управляются временем. |
| `dag` / `dagN` | DAG/filter поиск, использующий часы на каждый ход в обычных играх. |
| `filter` / `filterN` | Алиас для `dag` / `dagN`. |
| `randombot` | Бот случайных легальных ходов. |
| `human` | Человек-игрок в терминале. |

Режимы используют формат `P1-P2`:

```bash
./togyzkumalak_engine --mode human-ai --move-time 0.01
./togyzkumalak_engine --mode ai-randombot --numgames 20 --noterminal --move-time 0.01
./togyzkumalak_engine --mode ai-dag --numgames 10 --noterminal --move-time 0.01
```

## Сравнение Minimax и DAG-поиска

```bash
./togyzkumalak_engine --compare --depth 4 --positions 100 --seed 1337 --noterminal
```

Этот явный режим сравнения по-прежнему работает с фиксированной глубиной и тестирует ручной minimax
`aiN` против `dagN` на одинаковых рандомизированных сбалансированных стартовых позициях.

## Бенчмарк Minimax Self-Play

```bash
./togyzkumalak_engine --benchmark --bot ai --positions 100 --threads 1 --noterminal --move-time 0.01
./togyzkumalak_engine --benchmark --bot minimax --positions 100 --threads 1 --noterminal --move-time 0.01
```

Используйте `--threads 0` для автоматического параллелизма:

```bash
./togyzkumalak_engine --benchmark --bot ai --positions 1000 --threads 0 --noterminal --move-time 0.01
```

Бенчмарк сообщает общее время, секунды на игру, миллисекунды на ход, длину партии, счетчики результатов, ничьи по повторению и игры, достигшие max-step.

## Эвристическая оценка

Оценщик реализован в `ToguzEnv::evaluate` и обернут через `HeuristicEvaluator`. Он оценивает:

- терминальные победы, поражения и ничьи
- разницу счета в казанах с фазово-адаптированным весом гонки
- материал стороны и количество одной игровой лунки
- компактные бонусы владения и расположения Туздыка
- давление sweeping при малом материале и небольшой бонус стороны хода

Minimax-движок использует этот оценщик по умолчанию:

```bash
./togyzkumalak_engine --mode ai-ai --numgames 10 --noterminal --move-time 0.01 --p1 heuristic --p2 heuristic
```

## Генерируемые файлы

- `limit.txt` может создаваться во время бенчмарков. Он содержит `0` для чистого запуска или trace, если игра достигает предохранительного ограничения max-step.
- Dataset JSONL-файлы создаются только если передать `--dataset PATH`.

Для ручного minimax-движка файл модели не требуется.
