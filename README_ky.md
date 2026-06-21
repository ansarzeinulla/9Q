# 9Q: Тогуз коргоолдун оюн дарагынын татаалдыгы жана миллиард оюндук талдоо

*Башка тилдерде окуу: [English](README.md) | [Қазақша](README_kk.md) | [Русский](README_ru.md) | [Кыргызча](README_ky.md)*

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++17](https://img.shields.io/badge/C++-17-blue.svg)](https://isocpp.org/)

Бул репозиторий төмөнкү макала үчүн расмий, өз алдынча C++ баштапкы кодун камтыйт:

**Тогуз коргоолдун комбинатордук абал мейкиндиги жана эмпирикалык оюн дарагынын татаалдыгы: миллиард оюндук талдоо**  
*Ansar Zeinulla (2026)* | [Макаланы arXiv'ден окуу](Not uploaded yet)

## Репозиторийдин түзүмү

Бул пакет макаладагы эсептөө натыйжаларын кайра чыгаруу үчүн керектүү үч иштетилүүчү C++ компонентин камтыйт:

| Компонент | Максаты |
| :--- | :--- |
| `billion-game-generation/` | 1,000,000,000 оюн түзүү жана эмпирикалык метрикаларды алуу үчүн колдонулган жогорку өндүрүмдүү C++ кокустук playout симулятору. |
| `shortest-game-generation/` | Мүмкүн болгон эң кыска терминалдык оюнду (11 жарым жүрүш) далилдөө жана текшерүү үчүн толук эрте оюн издөө кыймылдаткычы. |
| `engine/` | Тогуз коргоолдун негизги эрежелер кыймылдаткычы жана Alpha-Beta кыркуусу бар кол менен түзүлгөн Minimax баалоо агенти. |

*Эскертүү: Бул репозиторийден тышкары эч кандай баштапкы код көз карандылыгы талап кылынбайт. Миллиард оюндук симулятор атайын `../engine/togyzkumalak_rules.cpp` жана `../engine/position_hash.cpp` файлдарын колдонот; алар түздөн-түз кошулат.*

## Талаптар

- C++17 компилятору (`clang++` демейки боюнча же `CXX=g++` аркылуу `g++`)
- `make`
- POSIX стилиндеги терминал

*Негизги C++ кыймылдаткычын жыйноо жана иштетүү үчүн Python пакеттери, нейрон тармак китепканалары же тышкы датасеттер талап кылынбайт.*

## Жыйноо нускамалары

Бардык модулдарды жыйноо үчүн түпкү каталогдон төмөнкүлөрдү иштетиңиз:

```bash
make -C engine
make -C billion-game-generation
make -C shortest-game-generation
```

## WebAssembly жыйноо

Эгер C++ өзөгүнө өзгөртүү киргизип, web колдонмосу үчүн WebAssembly бумасын кайра жыйноо керек болсо, алгач Emscripten SDK орнотуу зарыл:

1. Emscripten репозиторийин көчүрүп алып, ичине кириңиз:

   ```bash
   git clone https://github.com/emscripten-core/emsdk.git
   cd emsdk
   ```

2. Акыркы SDKны орнотуп, активдештириңиз:

   ```bash
   ./emsdk install latest
   ./emsdk activate latest
   source ./emsdk_env.sh
   ```

3. Бул долбоордун түпкү каталогунан автоматтык жыйноо скриптин иштетиңиз:

   ```bash
   npm run build:wasm
   ```

Бул C++ кодун `-O3` менен жыйнап, `web/public/wasm/` ичине `togyz_engine.js` жана `togyz_engine.wasm` чыгарат.

## Ыкчам текшерүү (тестирлөө)

Симуляция кыймылдаткычын текшерүү үчүн 1,000 кокустук оюндан турган чакан үлгүнү иштетиңиз:
```bash
cd billion-game-generation
./generate_billion_games --num=1000 --seed=1 --threads=1 --fresh --stat=sample_billion_game_statistics.txt
```

Жүрүш генераторунун математикалык тууралыгын Perft суитасы аркылуу текшериңиз (4-тереңдикке чейин node сандарын рекурсивдүү валидациялайт):
```bash
make test -C engine
```
Бул exhaustive leaf-node текшерүү кыймылдаткычын иштетет. Ал 1ден 4кө чейинки тереңдиктер үчүн тастыкталган абалды (тиешелүүлүгүнө жараша 9, 73, 613 жана 5,199 node) миллисекунддар ичинде чыгарып, абал өтүү эрежелери математикалык жактан так экенин көрсөтөт.

11 жарым жүрүштүк эң кыска оюн далилин текшериңиз:
```bash
cd ../shortest-game-generation
./find_shortest_game --verify-only
```

Кол менен түзүлгөн Minimax AI'га каршы матч ойноңуз:
```bash
cd ../engine
./togyzkumalak_engine --mode human-ai4
```
*(Эс тутуму аз машиналарда `ai4` ордуна `ai2` же `ai3` колдонуңуз).*

## Толук кайра чыгаруу командалары

Миллиард оюндук кокустук симуляцияны толук кайра чыгаруу үчүн (эскертүү: эсептөө ресурстарын абдан көп талап кылат):
```bash
cd billion-game-generation
./generate_billion_games --num=1000000000 --seed=709791810521833 --threads=10 --fresh --stat=billion_game_reproduction_statistics.txt
```
*Эскертүү: Кошулган `billion_game_statistics.txt` файлында макалада колдонулган акыркы эсептегичтер бар. Нөлдөн кайра чыгарууда жаңы чыгаруу жолун колдонуңуз.*

Эң кыска оюндун толук математикалык далилин иштетиңиз:
```bash
cd shortest-game-generation
./find_shortest_game
```
Бул `shortest_terminal_game.txt`, `shortest_candidate_replay.tsv` жана `shortest_depth_proof.tsv` файлдарын кайра түзөт.

Minimax кыймылдаткычынын self-play бенчмаркин иштетиңиз:
```bash
cd engine
./togyzkumalak_engine --benchmark --bot ai --depth 4 --positions 100 --threads 1 --noterminal
```

## Цитата келтирүү

Эгер бул кодду, Тогуз коргоол кыймылдаткычын же 1 миллиард оюндук датасетти изилдөөңүздө колдонсоңуз, бул макалага шилтеме бериңиз:

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
