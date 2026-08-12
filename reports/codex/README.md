# Codex local verification

`LATEST.md` — информационный отчёт о последней локальной проверке Codex, а не source of truth состояния repository.

После каждой implementation-задачи Codex перезаписывает `LATEST.md`; история предыдущих отчётов остаётся в Git.

В отчёт включаются только реально выполненные команды. Нельзя отмечать проверку как `PASS`, если команда не запускалась; при `FAIL` нужно кратко указать причину.

Базовая проверка для C++-изменений:

```powershell
cmake --build --preset vs2022-debug
ctest --preset vs2022-debug
git diff --check
```

Если затронут native/Godot adapter, дополнительно выполняется:

```powershell
.\scripts\build_godot.ps1 -Configuration Debug
```

Release build/test не является обязательной проверкой каждой итерации: его выполняют, когда это оправдано scope задачи или запрошено отдельно.
