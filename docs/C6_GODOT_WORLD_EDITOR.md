# C6 — Godot World Editor

Статус: **реализуемый текущий editor host**. Godot adapter зависит непосредственно от `clife_world`; `clife_presets` не является его зависимостью и не задаёт стартовый документ.

## 1. Пустой authoring start

При запуске `CLifeWorldEditor` создаёт пустой `WorldDefinition`. Он не вызывает `make_first_world_preset()` и не создаёт автоматически Value, Template, Function Type, bindings, runtime object или host input. `first_world` сохранён как отдельный example/test preset.

## 2. Дерево и редактирование

Текущие разделы дерева:

- Значения / Values;
- Формулы / Calculations;
- Шаблоны / Templates;
- Типы функций / Function Types;
- Правила мира / World Rules.

UI создаёт Values, Calculations, Templates и Function Types. Основной workspace остаётся экраном мира и симуляции; отдельная `Function Library` открывается как полноразмерный Control внутри того же окна и использует тот же `CLifeWorldEditor`. В ней FunctionType редактируется вкладками «Конструкция», «Процесс» и «Материалы»: независимые genome parameters, Calculation bindings, источники process/buffer/material contributions. Пользовательские имена остаются world data и не переводятся.

Value и Template можно удалить из их inspector. FunctionType удаляется через контекстное меню по правой кнопке; тип, используемый template genome, не удаляется каскадно. Function Library также даёт context delete для Calculation binding, process, process output, material contribution и buffer process. Genome parameter пока намеренно не имеет удаления: его безопасная lifecycle-semantics для уже существующих `GenomeFunctionInstance` остаётся отдельной authoring-задачей. Удаление использует обычную world validation и facade `last_error`.

## 3. Библиотека calculations

`CalculationDefinition` — самостоятельная чистая математическая библиотека. Специализированный Calculation inspector показывает inputs и последовательно вычисляемые outputs отдельными блоками, позволяет редактировать output `expression_source`, локально вычислять формулу на временных числах и удалить неиспользуемые Calculation/ports через контекстное меню. Удаление dependency-safe: input или output, требуемый выражением либо FunctionType, не удаляется. В inspector можно создать именованные inputs и outputs с `expression_source`:

```text
f(a) -> b, c
b = a * 0.8
c = a - b
```

Output видит все inputs и только ранее созданные outputs. Calculation подключается к `FunctionTypeDefinition`: каждый input связывается с независимым genome parameter, а outputs становятся вычисляемыми характеристиками phenotype. Calculation не исполняется каждый tick и пока не поддерживает chaining между разными Calculations.

Общий authoring-инвариант: у сущности, которую UI позволяет создать, должен появиться понятный путь удаления — предпочтительно через контекстное меню ПКМ. Эта итерация полностью реализует его для Calculation, input и output; остальные authoring-сущности получают свои lifecycle-операции отдельными задачами.

## 4. Expressions

Phenotype formulas и calculation outputs используют existing expression engine. Он принимает UTF-8 имена, если они реально объявлены среди доступных parameters/ports: например `Свет`, `КПД`, `ПропускнаяСпособность`, `Потери_Энергии`. Built-ins `min` и `max` сохраняют свою семантику.

## 5. EDIT / RUN и preview

EDIT изменяет только `WorldDefinition`. RUN создаёт runtime snapshot выбранного Template, инстанцирует preview object и исполняет fixed ticks; Pause, Step, Reset и Back to editor управляют только runtime lifecycle.

`ObjectTemplate` — библиотечное определение, а не размещённый объект мира. Создание или выбор Template в EDIT не показывает сферу. Сфера служит preview активного runtime; отдельного действия «Разместить клетку» и authoring-модели world instances пока нет.

Host capabilities `world.light` и `geometry.volume` принадлежат Godot host, но не создают Values или bindings сами по себе. HostBinding внутри Template остаётся данными мира.

## 6. Persistence

Godot использует один рабочий файл `user://current_world.clife.json`. Кнопка «Сохранить мир» / Save World доступна только в EDIT. Она сохраняет `WorldDefinitionSnapshot`; при следующем старте GDScript пытается загрузить этот же файл.

- отсутствующий файл означает пустой мир;
- повреждённый файл не перезаписывается автоматически и показывает ошибку;
- stable IDs и next-ID counters сохраняются;
- Calculation expressions хранятся как source text и компилируются заново;
- runtime state, Tick, buffer amounts, host input values, UI selection, locale и camera не сохраняются.

Это не система проектов: пока нет Open, Save As, списка файлов, autosave или RuntimeWorld persistence. До этапа стабилизации формата importer принимает только актуальную snapshot schema; старый тестовый файл показывает понятную ошибку и может быть создан заново.

## 7. Локализация и build

UI использует RU/EN PO resources; locale хранится отдельно в `user://settings.cfg` и не входит в мир.

```powershell
.\scripts\build_godot.ps1 -Configuration Debug
godot --editor --path .\apps\godot
```

Обычные core/world builds не требуют Godot или Unreal.
