# 9Q: Тоғызқұмалақтың ойын ағашы күрделілігі және миллиард ойындық талдау

*Басқа тілдерде оқыңыз: [English](README.md) | [Қазақша](README_kk.md) | [Русский](README_ru.md) | [Кыргызча](README_ky.md)*

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++17](https://img.shields.io/badge/C++-17-blue.svg)](https://isocpp.org/)

Бұл репозиторий мына мақалаға арналған ресми, дербес C++ бастапқы кодын қамтиды:

**Тоғызқұмалақтың комбинаторлық күй кеңістігі және эмпирикалық ойын ағашы күрделілігі: миллиард ойындық талдау**  
*Ansar Zeinulla (2026)* | [Мақаланы arXiv-тен оқу](Not uploaded yet)

## Репозиторий құрылымы

Бұл пакет мақаладағы есептеу нәтижелерін қайта өндіруге қажет үш іске қосылатын C++ компонентін қамтиды:

| Компонент | Мақсаты |
| :--- | :--- |
| `billion-game-generation/` | 1,000,000,000 ойын генерациялау және эмпирикалық метрикаларды шығару үшін қолданылған жоғары өнімді C++ кездейсоқ playout симуляторы. |
| `shortest-game-generation/` | Мүмкін болатын ең қысқа аяқталатын ойынды (11 жартыжүріс) дәлелдеу және тексеру үшін толық ерте ойын іздеу қозғалтқышы. |
| `engine/` | Тоғызқұмалақтың негізгі ережелер қозғалтқышы және Alpha-Beta кесуі бар қолмен жасалған Minimax бағалау агенті. |

*Ескертпе: Бұл репозиторийден тыс ешқандай бастапқы код тәуелділігі қажет емес. Миллиард ойындық симулятор әдейі `../engine/togyzkumalak_rules.cpp` және `../engine/position_hash.cpp` файлдарын пайдаланады; олар тікелей құрамға енгізіледі.*

## Талаптар

- C++17 компиляторы (`clang++` әдепкі бойынша немесе `CXX=g++` арқылы `g++`)
- `make`
- POSIX стиліндегі терминал

*Негізгі C++ қозғалтқышын құрастыру және іске қосу үшін Python пакеттері, нейрондық желі кітапханалары немесе сыртқы датасеттер қажет емес.*

## Құрастыру нұсқаулары

Барлық модульдерді құрастыру үшін түбір каталогтан мына командаларды орындаңыз:

```bash
make -C engine
make -C billion-game-generation
make -C shortest-game-generation
```

## WebAssembly құрастыру

Егер C++ өзегіне өзгеріс енгізіп, web қолданбасы үшін WebAssembly бумасын қайта жинағыңыз келсе, алдымен Emscripten SDK орнату қажет:

1. Emscripten репозиторийін көшіріп алып, ішіне кіріңіз:

   ```bash
   git clone https://github.com/emscripten-core/emsdk.git
   cd emsdk
   ```

2. Соңғы SDK-ны орнатып, белсендіріңіз:

   ```bash
   ./emsdk install latest
   ./emsdk activate latest
   source ./emsdk_env.sh
   ```

3. Осы жобаның түбір каталогынан автоматты құрастыру скриптін іске қосыңыз:

   ```bash
   npm run build:wasm
   ```

Бұл C++ кодын `-O3` жалаушасымен жинайды және `web/public/wasm/` ішіне `togyz_engine.js` пен `togyz_engine.wasm` шығарады.

## Жылдам тексеру (тестілеу)

Симуляция қозғалтқышын тексеру үшін кездейсоқ ойынның шағын 1,000 ойындық үлгісін іске қосыңыз:
```bash
cd billion-game-generation
./generate_billion_games --num=1000 --seed=1 --threads=1 --fresh --stat=sample_billion_game_statistics.txt
```

Қозғалыс генераторының математикалық дұрыстығын Perft жиыны арқылы тексеріңіз (4-ші тереңдікке дейін node сандарын рекурсивті түрде валидациялайды):
```bash
make test -C engine
```
Бұл exhaustive leaf-node тексеру қозғалтқышын іске қосады. Ол 1-ден 4-ке дейінгі тереңдіктер үшін расталған нәтижелерді (тиісінше 9, 73, 613 және 5,199 node) миллисекундтар ішінде шығарып, күй ауысу ережелерінің математикалық тұрғыдан дұрыс екенін көрсетеді.

11 жартыжүрістік ең қысқа ойын дәлелін тексеріңіз:
```bash
cd ../shortest-game-generation
./find_shortest_game --verify-only
```

Қолмен жасалған Minimax AI-ға қарсы матч ойнаңыз:
```bash
cd ../engine
./togyzkumalak_engine --mode human-ai4
```
*(Жады аз машиналарда `ai4` орнына `ai2` немесе `ai3` пайдаланыңыз).*

## Толық қайта өндіру командалары

Миллиард ойындық кездейсоқ симуляцияны толық қайта өндіру үшін (ескерту: есептеу ресурсын өте көп қажет етеді):
```bash
cd billion-game-generation
./generate_billion_games --num=1000000000 --seed=709791810521833 --threads=10 --fresh --stat=billion_game_reproduction_statistics.txt
```
*Ескертпе: Қоса берілген `billion_game_statistics.txt` файлында мақалада қолданылған соңғы санағыштар бар. Нөлден қайта өндіру кезінде жаңа шығару жолын пайдаланыңыз.*

Ең қысқа ойынның толық математикалық дәлелін іске қосыңыз:
```bash
cd shortest-game-generation
./find_shortest_game
```
Бұл `shortest_terminal_game.txt`, `shortest_candidate_replay.tsv` және `shortest_depth_proof.tsv` файлдарын қайта жасайды.

Minimax қозғалтқышының self-play бенчмаркін іске қосыңыз:
```bash
cd engine
./togyzkumalak_engine --benchmark --bot ai --depth 4 --positions 100 --threads 1 --noterminal
```

## Дәйексөз

Егер бұл кодты, Тоғызқұмалақ қозғалтқышын немесе 1 миллиард ойындық датасетті зерттеуіңізде пайдалансаңыз, мына мақалаға сілтеме жасаңыз:

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
