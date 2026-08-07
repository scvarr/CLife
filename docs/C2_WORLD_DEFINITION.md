# C2 — определение мира как данных

Статус: **принято / начало реализации**

Этот документ фиксирует границу между универсальным ядром CLife и конкретным сохранённым миром.

Если более ранний `C0_CONCEPT.md` называет `volume`, `heat_capacity`, `temperature`, `Organic`, конкретную формулу эффективности или форму клетки универсальным законом, это следует читать как описание первого возможного world preset. Такие имена и формулы не являются обязательной частью `clife_core`.

## 1. Основной принцип

`clife_core` содержит:

- числовые роли `Field / Resource / State / Matter`;
- simulation tick;
- простые runtime-примитивы;
- правила конкуренции и сохранения потока;
- проверку совместимости мер;
- в будущем — интерпретатор небольшого декларативного DSL формул.

World definition содержит:

- какие конкретно `Field[n]`, `Resource[n]`, `State[n]`, `Matter[n]` существуют;
- их пользовательские имена;
- единицы и общие меры;
- произвольные числовые свойства;
- формулы производных характеристик;
- формулы стоимости/эффективности модулей;
- cell/world templates и стартовую конфигурацию.

Добавление нового типа внутри уже существующей роли не требует нового C++-кода.

## 2. Measure — общая мера

`Measure[n]` — машинный идентификатор общей числовой меры конкретного мира.

Каждый тип сообщает, сколько единиц своей меры содержит одна единица данного типа:

```text
Field[0]:
    measure = Measure[3]
    measure_per_unit = 2

Resource[1]:
    measure = Measure[3]
    measure_per_unit = 4
```

Следовательно:

```text
1 Field[0] = 2 Measure[3]
1 Resource[1] = 4 Measure[3]

1 Field[0] = 0.5 Resource[1]
```

Core не знает, называется ли `Measure[3]` энергией, массой, объёмом или условной игровой величиной.

Типы с разными `Measure[n]` нельзя соединить обычным сохраняющим преобразованием без дополнительного явно определённого закона.

## 3. Property — произвольное свойство типа

`Property[n]` — числовое свойство типа, смысл которого задаётся только world definition.

Например один мир может определить:

```text
Property[0] = volume_factor
Property[1] = heat_capacity
Property[2] = absorption
```

и затем:

```text
Matter[0].Property[0] = 1.0
Matter[0].Property[1] = 1.0
Matter[0].Property[2] = 0.6
```

Другой мир может использовать те же индексы для других понятий.

`clife_core` не содержит специальных полей `volume_per_unit`, `heat_capacity_per_unit`, `absorption` и т. п.

## 4. Производные величины

Производные характеристики задаются формулами мира, а не полями `Cell`.

Будущий DSL должен уметь выражать небольшие числовые зависимости уровня:

```text
CellMetric[0] =
    sum(cell.matter[n] * Matter[n].Property[0])

CellMetric[1] =
    sum(cell.matter[n] * Matter[n].Property[1])

CellMetric[2] =
    State[2] / CellMetric[1]
```

Конкретный preset может назвать их:

```text
CellMetric[0] = volume
CellMetric[1] = heat_capacity
CellMetric[2] = temperature
```

Но эти имена не приобретают специального значения для core.

DSL должен быть декларативным expression graph, а не универсальным языком программирования. Минимально ожидаются арифметика, `min/max`, суммы по коллекциям и чтение доступных значений/свойств.

## 5. Простые функции

### TAKE

```text
TAKE(A, throughput)
```

`throughput` — сколько единиц входного типа функция способна взять за tick.

### STORE

```text
STORE(A, capacity)
```

`capacity` — сколько единиц конкретного типа может сохраняться между tick.

### TRANSFORM

```text
TRANSFORM(A -> B, throughput)
```

`throughput` — максимум входных единиц `A` за tick.

Если `A` и `B` используют одну общую меру, идеальное преобразование вычисляется автоматически:

```text
processed_measure =
    processed_A * A.measure_per_unit

produced_B =
    processed_measure / B.measure_per_unit
```

Коэффициент `A -> B` поэтому не является отдельным ручным параметром функции.

Пример:

```text
A.measure_per_unit = 2
B.measure_per_unit = 4

processed_A = 1
produced_B = 0.5
```

Связь эффективности, потерь, structural cost и throughput не является универсальным hardcoded законом. Конкретный preset сможет задать такие зависимости формулами.

## 6. State и остаток Resource

Сохранённый `Resource[n]` остаётся рабочим ресурсом и может быть использован позже.

Непереработанный остаток, направленный в `State[n]`, перестаёт быть рабочим Resource. Если Resource и State используют одну `Measure[n]`, переход сохраняет общую меру с учётом их различных `measure_per_unit`.

Это позволяет одному preset интерпретировать State как тепловую энергию, а другому — как иной накопленный фактор.

## 7. Matter и размер клетки

`CellPhenotype` хранит структурный состав как количества `Matter[n]`.

Само наличие Matter не означает, что core обязан вычислять объём или геометрический радиус. World formula может вывести нужную метрику из состава и `Property[n]`.

Связь модулей клетки с количеством строительной Matter также является данными мира:

```text
transform structural cost = f(throughput)
store structural cost = g(capacity)
shell structural cost = ...
```

На текущем runtime этапе phenotype может получать уже агрегированный structural composition.

## 8. Renderer

Renderer получает числа и идентификаторы/метрики симуляции.

Он может интерпретировать условную метрику размера как:

- площадь круга в 2D;
- объём сферы в 3D;
- размер hex/tile;
- параметр другого визуального представления.

Выбор 2D/3D не меняет симуляционные законы и не требует менять `clife_core`.

## 9. Текущий implementation slice

Текущая реализация уже использует отдельные роли `Field`, `Resource`, `State`, `Matter`.

Следующий слой вводит:

```text
MeasureType
UnitScale
PropertyType
TypeProperty
FieldDefinition
ResourceDefinition
StateDefinition
MatterDefinition
```

Текущий `Field -> Resource` transform и `Resource -> State` remainder используют общую меру для автоматического пересчёта количества.

Полный `Resource -> Resource` внутриклеточный поток появится вместе с runtime `TAKE`/общей сетью ресурсов; вводить отдельный последовательный executor только ради этого не требуется.
