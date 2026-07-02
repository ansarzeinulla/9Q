# Кол менен жазылган баалоо кыймылдаткычы

*Башка тилдерде окуу: [English](README.md) | [Қазақша](README_kk.md) | [Русский](README_ru.md) | [Кыргызча](README_ky.md)*

Бул папка C++ тилиндеги Тогуз коргоол эрежелер кыймылдаткычын жана ойноого боло турган буйрук сабы кыймылдаткычын камтыйт.

Изилдөө пакети үчүн негизги керектүү кыймылдаткыч - кол менен жазылган minimax кыймылдаткычы:

- Боттун аты: `aiN`
- Баштапкы код: `minimax_engine.cpp` жана `minimax_engine.hpp`
- Издөө: alpha-beta кыркуусу, жүрүштөрдү иреттөө, транспозиция таблицасы жана тактикалык quiescence search бар туруктуу тереңдиктеги minimax
- Баалоо: `HeuristicEvaluator` жана `ToguzEnv::evaluate` аркылуу жеткиликтүү кол менен жазылган эвристикалык баалоочу

Папкада жаңыраак `dagN` / `filterN` издөө ишке ашыруусу да бар, бирок `aiN` негизги baseline катары колдонулган minimax/баалоо кыймылдаткычы болуп саналат.

## Файлдар

| Файл | Максаты |
| :--- | :--- |
| `togyzkumalak_rules.*` | Такта көрүнүшү, мыйзамдуу жүрүштөр, эрежелер, терминалдык абалды аныктоо, кайталоону көзөмөлдөө жана эвристикалык баалоо. |
| `minimax_engine.*` | Кол менен түзүлгөн minimax/quiescence кыймылдаткычы. |
| `dag_search.*` | Салыштыруу үчүн кошулган DAG/filter издөө кыймылдаткычы. |
| `evaluation.*` | Баалоочу интерфейси жана статистикага негизделген эвристикалык wrapper. |
| `position_hash.*` | Транспозиция/кайталоо логикасы үчүн хештөө. |
| `togyzkumalak_engine.cpp` | Оюн, self-play, салыштыруу, benchmark жана UCI сыяктуу буйруктар үчүн CLI кирүү чекити. |
| `Makefile` | `togyzkumalak_engine` бинарын жыйнайт. |

## Жыйноо

```bash
cd FINAL/engine
make
```

`clang++` ордуна `g++` колдонуу үчүн:

```bash
make CXX=g++
```

## Minimax кыймылдаткычына каршы ойноо

```bash
./togyzkumalak_engine --mode human-ai
```

Кыймылдаткыч убакыт чектөөсү бар iterative deepening колдонот. Демейки боюнча ар бир жарым жүрүшкө
`0.01` секунд берилет. Бюджетти `--move-time` менен өзгөртүңүз:

```bash
./togyzkumalak_engine --mode human-ai --move-time 0.05
```

## Автоматташтырылган оюндарды иштетүү

```bash
./togyzkumalak_engine --mode ai-randombot --numgames 20 --noterminal
./togyzkumalak_engine --mode ai-ai --numgames 10 --noterminal --switch-color --move-time 0.01
```

## Бот аттары

| Бот | Мааниси |
| :--- | :--- |
| `ai` / `aiN` | Ар бир жүрүшкө бөлүнгөн убакытты колдонгон кол менен жазылган minimax/quiescence кыймылдаткычы; `N` эски буйруктар менен шайкештик үчүн кабыл алынат, бирок кадимки оюндар убакыт менен башкарылат. |
| `dag` / `dagN` | Кадимки оюндарда ар бир жүрүшкө бөлүнгөн убакытты колдонгон DAG/filter издөөсү. |
| `filter` / `filterN` | `dag` / `dagN` үчүн алиас. |
| `randombot` | Кокустук мыйзамдуу жүрүш жасаган бот. |
| `human` | Терминалдагы адам оюнчу. |

Режимдер `P1-P2` форматын колдонот:

```bash
./togyzkumalak_engine --mode human-ai --move-time 0.01
./togyzkumalak_engine --mode ai-randombot --numgames 20 --noterminal --move-time 0.01
./togyzkumalak_engine --mode ai-dag --numgames 10 --noterminal --move-time 0.01
```

## Minimax жана DAG издөөсүн салыштыруу

```bash
./togyzkumalak_engine --compare --depth 4 --positions 100 --seed 1337 --noterminal
```

Бул ачык салыштыруу режими дагы эле туруктуу тереңдикти колдонот жана бирдей рандомизацияланган тең салмактуу баштапкы абалдарда кол менен жазылган minimax
`aiN` кыймылдаткычын `dagN` менен салыштырат.

## Minimax Self-Play бенчмарки

```bash
./togyzkumalak_engine --benchmark --bot ai --positions 100 --threads 1 --noterminal --move-time 0.01
./togyzkumalak_engine --benchmark --bot minimax --positions 100 --threads 1 --noterminal --move-time 0.01
```

Автоматтык параллелизм үчүн `--threads 0` колдонуңуз:

```bash
./togyzkumalak_engine --benchmark --bot ai --positions 1000 --threads 0 --noterminal --move-time 0.01
```

Бенчмарк жалпы убакытты, ар бир оюнга секунддарды, ар бир жүрүшкө миллисекунддарды, оюн узундугун, натыйжа эсептерин, кайталоодон тең чыгууларды жана max-step оюндарын билдирет.

## Эвристикалык баалоо

Баалоочу `ToguzEnv::evaluate` ичинде ишке ашырылган жана `HeuristicEvaluator` аркылуу оролгон. Ал төмөнкүлөрдү баалайт:

- терминалдык жеңиштер, жеңилүүлөр жана тең чыгуулар
- фазага ылайыкташкан жарыш салмагы бар Казан упай айырмасы
- тарап материалы жана бир ойноого боло турган чуңкур саны
- компакттуу Туздук ээлиги жана жайгашуу бонустары
- аз материалдагы sweep pressure жана жүрүүчү тарап үчүн чакан бонус

Minimax кыймылдаткычы бул баалоочуну демейки боюнча колдонот:

```bash
./togyzkumalak_engine --mode ai-ai --numgames 10 --noterminal --move-time 0.01 --p1 heuristic --p2 heuristic
```

## Генерацияланган файлдар

- `limit.txt` бенчмарктер учурунда түзүлүшү мүмкүн. Анда таза иштетүү үчүн `0` же оюн max-step safety cap чегине жетсе trace болот.
- Dataset JSONL файлдары `--dataset PATH` берилсе гана түзүлөт.

Кол менен жазылган minimax кыймылдаткычы үчүн модель файлы талап кылынбайт.
