# Changelog

Todas las fechas en formato `dd-mm-aaaa`.

## [2.1b] - 20-10-2026

Auditoría completa del código y corrección de los hallazgos críticos e importantes.

### Corregido
- **`--dry-run` ya no se ignora**: combinarlo con `--watch` o `--deshacer` se
  rechaza (código 2). Antes `--watch --dry-run` movía archivos de verdad.
- **Rutas UNC** (`\\servidor\recurso`) y `\\?\`: `createDirRecursive` conservaba mal
  el prefijo y creaba carpetas basura en la raíz de la unidad actual.
- **El historial de deshacer ya no se pierde**: si `movimientos.json` existe pero no
  se puede leer o no es JSON válido, se conserva intacto y se avisa (antes se
  sobrescribía con solo el lote nuevo).
- **Protección de archivos propios por ruta canónica** (no textual): el `.exe`,
  `reglas.json` y el log no se mueven aunque el origen use `/` o rutas no normalizadas.
- Nombres reservados (`movimientos.json`, `movimientos.json.tmp`, `reglas.json`,
  `reglas.example.json`) excluidos siempre, estén donde estén.
- **Fail-closed más estricto**: `destinoBase` vacío y origen vacío o que no es carpeta
  abortan con código 2.

### Notas
- Hallazgos no críticos documentados como pendientes (rutas largas >260, carrera
  sobre el `.tmp` entre procesos, límite de profundidad del parser JSON).

## [2.1] - 19-10-2026

Revisión de código y preparación para publicación.

### Corregido
- `--deshacer` ya no se queda atascado en un lote: descarta los movimientos cuyo
  destino ya no existe (archivo borrado o movido a mano) y conserva solo los
  reintentables (origen ocupado o archivo bloqueado).
- El aviso final «no se pudieron deshacer» ahora sí se imprime (se guardaba el
  conteo después de vaciar el vector).
- El vigilante ya no congela el menú de la bandeja: el escaneo corre en un hilo
  aparte, así la espera por archivos inestables no bloquea el bucle de mensajes.
- Lectura de números del JSON con límites (evita desbordar `int`).

### Añadido
- `-mwindows`: sin ventana de consola; se adjunta a la del padre para
  `--dry-run`/`--help`/`--selftest`.
- `--selftest` cubre ahora fail-closed y el flujo de `--deshacer`.

### Publicación
- `build.bat` portable (`MINGW_BIN` / `PATH`), `README.md`, `LICENSE` (MIT),
  `crear_atajo.ps1`, `reglas.example.json`.
- `.gitignore` para no versionar `reglas.json`, `movimientos.json`, `*.lnk`,
  `.exe`, `.o` ni `SHA256SUMS.txt`.
- El `.exe` se publica como Release junto a su `SHA256SUMS.txt`, no en el repo.

## [2.0] - 18-10-2026

Versión «más robusta».

- Log JSON de movimientos (`movimientos.json`) y `--deshacer` (rollback inverso).
- Lista de `proteccion` con comodines (`*`, `?`); reintentos con espera.
- Destino `Categoría\YYYY\MM` (config `subcarpeta`).
- Modo vigilante (`--watch`) con icono en la bandeja e instancia única.
- Icono incrustado (`organizador.rc`).

## [1.0] - 17-10-2026

- Primera versión: mueve archivos sueltos a `Categoría\`, con anticolisión `_N`,
  `MoveFileExW` y `--dry-run`.
