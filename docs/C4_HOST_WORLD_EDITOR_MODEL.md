# C4 — World / Runtime / Host foundation

Статус: **реализовано / текущая модель интеграции**

C4 отделяет компактный числовой evaluator от редактируемого мира, runtime-состояния и будущих адаптеров хоста. Каноническая модель мира остаётся независимой от Godot, Unreal и конкретного UI.

## 1. Слои и зависимости

```text
Editor/UI                  Host adapter (headless / Godot / Unreal)
    |                                  |
    v                                  v
WorldDefinition  ------compile----> RuntimeWorld / RuntimeObject
                                           |
                                           v
                                      Calculator
```

В сборке это выражено направлением:

```text
clife_core <- clife_world <- host applications / future engine adapters
```

- `Calculator` — низкоуровневый числовой evaluator с плотными адресами.
- `WorldDefinition` — редактируемые имена, шаблоны, genome, world rules и host bindings.
- `RuntimeWorld` — скомпилированные программы, объекты и их состояние.
- Host adapter — собирает данные движка, подаёт их в runtime и применяет выходы.
- Editor/UI — изменяет `WorldDefinition`; виджеты и lifecycle остаются специфичными для движка.

## 2. `ValueKey` и `ValueId`

`ValueKey` — стабильная идентичность значения в `WorldDefinition`. Ключ не зависит от позиции в контейнере, а удалённый ключ не переиспользуется. Поэтому ссылки из function process definitions, правил, initial values и bindings переживают переупорядочивание данных.

`ValueId` — плотный индекс `Calculator`. При компиляции существующие `ValueKey` сортируются и детерминированно отображаются в диапазон `[0, value_count)`. Хост и редактор адресуют значения через `ValueKey`; доступ к `Calculator` наружу не требуется.

Аналогично, `TemplateId` стабильно идентифицирует шаблон, а `ObjectId` — созданный runtime-объект.

## 3. Редактируемая модель и `Program`

`WorldDefinition` — источник человекочитаемых и редактируемых данных. Он предоставляет явные операции добавления, переименования, изменения и удаления определений и сразу отвергает неизвестные ключи, пустые/дублирующиеся имена, некорректные числа и конфликтующие bindings/rules.

`Program` не является editable world model. Это компактное скомпилированное представление одного `ObjectTemplate`, передаваемое в `Calculator`. Начиная с C7 компиляция сначала строит phenotype из function types и независимых genome parameters, затем переводит процессные функции phenotype в `Function`, stable value keys — в dense IDs, initial values — в `ValueAmount`, world rules — в `EndRule`.

## 4. Genome и world rules

Genome хранится внутри `ObjectTemplate` как экземпляры world-defined function types. Он содержит только независимые наследуемые значения:

```text
FunctionTypeId
ParameterId -> independent Amount
```

Входы/выходы процесса, формулы производных параметров и их calculator-мэппинг принадлежат `FunctionTypeDefinition` на уровне мира. Производные параметры вычисляются при сборке immutable phenotype и не дублируются в genome. Подробная C7-модель описана в `C7_GENOTYPE_PHENOTYPE.md`.

World rules принадлежат миру и описывают неизбежные последствия после genome pipeline:

```text
remaining source ValueKey -> target ValueKey * target_per_source
```

Они семантически и в editable data хранятся отдельно. Текущая компиляция применяет все правила мира к каждой программе шаблона и использует существующий `Program::end_rules`; это не делает правила частью genome.

## 5. Host channels

Host channel — строковое семантическое имя, например `world.light` или `cell.temperature`, а не engine node, Unreal object или callback. Направление задаётся явно:

- `Input`: `HostChannel -> ValueKey`;
- `Output`: `ValueKey -> HostChannel`.

CLife не вызывает host/engine. Хост сам выполняет цикл:

```text
gather engine values
set_input(object, channel/key, amount)
step()
value(object, key) / output(object, channel)
apply results to engine
```

Input staging является однотактовым. На каждом `RuntimeWorld::step()` все объявленные input bindings подаются в calculator: явно staged значение либо `0`, если хост не задал его для этого tick. После шага staging очищается. Поэтому данные прошлого render frame не становятся неявным входом следующего simulation tick.

Вызов `step()` — ровно один фиксированный simulation tick. Частоту таких вызовов выбирает host независимо от rendering FPS; scheduler в CLife не вводится.

## 6. Runtime objects

`RuntimeWorld::instantiate(TemplateId)` создаёт объект с новым `ObjectId`, сохраняет source `TemplateId` и строит отдельный `Calculator`. Объекты одного шаблона разделяют только скомпилированное определение при создании, но не изменяемое состояние.

Обычный host API не возвращает mutable `Calculator&`. Значения читаются по `ObjectId + ValueKey` либо через output channel.

## 7. Следующие адаптеры

Godot и Unreal adapters должны зависеть от `clife_world`, переводить lifecycle и engine values в описанный push/read контракт и не создавать альтернативную модель мира. Смысл доменных полей и операций редактора должен быть общим; сцены, assets, widgets, inspectors и другие UI-детали будут engine-specific.

C4 намеренно не вводит renderer interface, serialization, reflection, undo framework, scheduler или общий cross-engine UI toolkit.

## 8. Зарезервированный structural lifecycle

Будущие операции структуры (`CreateObject`, `RemoveObject`, `ConnectObjects`, `DisconnectObjects`) не являются немедленными побочными эффектами calculator-функций. Зарезервирован следующий порядок:

```text
calculator phase
    -> numeric state / structural intent
end of tick
    -> runtime collects structural operations
tick boundary
    -> RuntimeWorld applies structural mutations
next tick
    -> new objects participate normally
```

Инварианты будущего расширения:

1. `Calculator` не создаёт runtime objects напрямую.
2. Объекты и topology изменяются только на границе tick.
3. Новый объект не исполняет genome в tick своего создания.
4. Host adapters обязаны поддерживать динамический набор `ObjectId`, даже если текущий demo содержит одну клетку.
5. Structural changes будут передаваться host через явные deltas/events, а не engine callbacks.
6. Godot/Unreal создают и уничтожают визуальные representations в ответ на изменения CLife.

Текущий C5 slice не реализует сами операции или `WorldDelta`; раздел фиксирует только границу для последующего reproduction/topology этапа.
