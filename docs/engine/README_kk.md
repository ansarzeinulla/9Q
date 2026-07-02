# Қолмен жазылған бағалау қозғалтқышы

*Басқа тілдерде оқыңыз: [English](README.md) | [Қазақша](README_kk.md) | [Русский](README_ru.md) | [Кыргызча](README_ky.md)*

Бұл қалта C++ тіліндегі Тоғызқұмалақ ережелер қозғалтқышын және ойнауға болатын командалық жол қозғалтқышын қамтиды.

Зерттеу пакеті үшін негізгі қажет қозғалтқыш - қолмен жазылған minimax қозғалтқышы:

- Бот атауы: `aiN`
- Бастапқы код: `minimax_engine.cpp` және `minimax_engine.hpp`
- Іздеу: alpha-beta кесуі, жүрістерді реттеу, транспозиция кестесі және тактикалық quiescence іздеуі бар бекітілген тереңдікті minimax
- Бағалау: `HeuristicEvaluator` және `ToguzEnv::evaluate` арқылы ашылған қолмен жазылған эвристикалық бағалаушы

Бұл қалтада жаңарақ `dagN` / `filterN` іздеу іске асырылымы да бар, бірақ `aiN` негізгі baseline ретінде қолданылатын minimax/бағалау қозғалтқышы болып табылады.

## Файлдар

| Файл | Мақсаты |
| :--- | :--- |
| `togyzkumalak_rules.*` | Тақта көрінісі, заңды жүрістер, ережелер, терминалдық күйді анықтау, қайталануды бақылау және эвристикалық бағалау. |
| `minimax_engine.*` | Адам қолымен жасалған minimax/quiescence қозғалтқышы. |
| `dag_search.*` | Салыстыру үшін қосылған DAG/filter іздеу қозғалтқышы. |
| `evaluation.*` | Бағалаушы интерфейсі және статистикаға негізделген эвристикалық wrapper. |
| `position_hash.*` | Транспозиция/қайталану логикасына арналған хэштелу. |
| `togyzkumalak_engine.cpp` | Ойын, self-play, салыстыру, benchmark және UCI-ға ұқсас командаларға арналған CLI кіру нүктесі. |
| `Makefile` | `togyzkumalak_engine` бинарын құрастырады. |

## Құрастыру

```bash
cd FINAL/engine
make
```

`clang++` орнына `g++` қолдану үшін:

```bash
make CXX=g++
```

## Minimax қозғалтқышына қарсы ойнау

```bash
./togyzkumalak_engine --mode human-ai
```

Қозғалтқыш уақыт шектеуі бар iterative deepening қолданады. Әдепкі бойынша әр жартыжүріске
`0.01` секунд беріледі. Бюджетті `--move-time` арқылы өзгертіңіз:

```bash
./togyzkumalak_engine --mode human-ai --move-time 0.05
```

## Автоматтандырылған ойындарды іске қосу

```bash
./togyzkumalak_engine --mode ai-randombot --numgames 20 --noterminal
./togyzkumalak_engine --mode ai-ai --numgames 10 --noterminal --switch-color --move-time 0.01
```

## Бот атаулары

| Бот | Мағынасы |
| :--- | :--- |
| `ai` / `aiN` | Әр жүріске арналған сағатты қолданатын қолмен жазылған minimax/quiescence қозғалтқышы; `N` ескі командалармен үйлесімділік үшін қабылданады, бірақ әдеттегі ойындар уақытпен басқарылады. |
| `dag` / `dagN` | Әдеттегі ойындарда әр жүріске арналған сағатты қолданатын DAG/filter іздеуі. |
| `filter` / `filterN` | `dag` / `dagN` үшін бүркеншік атау. |
| `randombot` | Кездейсоқ заңды жүріс жасайтын бот. |
| `human` | Терминалдағы адам ойыншы. |

Режимдер `P1-P2` пішімін қолданады:

```bash
./togyzkumalak_engine --mode human-ai --move-time 0.01
./togyzkumalak_engine --mode ai-randombot --numgames 20 --noterminal --move-time 0.01
./togyzkumalak_engine --mode ai-dag --numgames 10 --noterminal --move-time 0.01
```

## Minimax пен DAG іздеуін салыстыру

```bash
./togyzkumalak_engine --compare --depth 4 --positions 100 --seed 1337 --noterminal
```

Бұл айқын салыстыру режимі әлі де бекітілген тереңдікті қолданады және бірдей рандомизацияланған теңгерілген бастапқы күйлерде қолмен жазылған minimax
`aiN` қозғалтқышын `dagN` қозғалтқышымен салыстырады.

## Minimax Self-Play бенчмаркі

```bash
./togyzkumalak_engine --benchmark --bot ai --positions 100 --threads 1 --noterminal --move-time 0.01
./togyzkumalak_engine --benchmark --bot minimax --positions 100 --threads 1 --noterminal --move-time 0.01
```

Автоматты параллелизм үшін `--threads 0` пайдаланыңыз:

```bash
./togyzkumalak_engine --benchmark --bot ai --positions 1000 --threads 0 --noterminal --move-time 0.01
```

Бенчмарк жалпы уақытты, әр ойынға кеткен секундты, әр жүріске кеткен миллисекундты, ойын ұзындығын, нәтиже санағын, қайталану теңдіктерін және max-step ойындарын хабарлайды.

## Эвристикалық бағалау

Бағалаушы `ToguzEnv::evaluate` ішінде іске асырылған және `HeuristicEvaluator` арқылы оралған. Ол мыналарды бағалайды:

- терминалдық жеңістер, жеңілістер және теңдіктер
- фазаға бейімделген жарыс салмағы бар Қазан ұпай айырмасы
- тарап материалы және бір ойнауға болатын отау санағы
- ықшам Тұздық иеленуі және орналасу бонустары
- аз материалдағы sweep pressure және жүріс кезегі үшін шағын бонус

Minimax қозғалтқышы әдепкі бойынша осы бағалаушыны пайдаланады:

```bash
./togyzkumalak_engine --mode ai-ai --numgames 10 --noterminal --move-time 0.01 --p1 heuristic --p2 heuristic
```

## Генерацияланатын файлдар

- `limit.txt` бенчмарктер кезінде жасалуы мүмкін. Онда таза іске қосу үшін `0` немесе ойын max-step safety cap шегіне жетсе trace болады.
- Dataset JSONL файлдары тек `--dataset PATH` берсеңіз ғана жасалады.

Қолмен жазылған minimax қозғалтқышы үшін модель файлы қажет емес.
