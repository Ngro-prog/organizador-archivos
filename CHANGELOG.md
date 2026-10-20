# Changelog

Todas las fechas en formato `dd-mm-aaaa`.

## [1.0.0] - 20-10-2026

Primera versión pública.

### Características
- Organiza archivos sueltos en `Categoría\Año\Mes` (configurable) usando la fecha
  de **creación** local (configurable a modificación).
- Clasificación por extensión y, como respaldo, por el contenido real del archivo
  (magic bytes).
- **Anticolisión**: si el nombre ya existe, añade `_1`, `_2`, ...
- **Simulación** con `--dry-run`: muestra qué haría sin mover nada.
- **Deshacer** con `--deshacer`: revierte el último lote desde `movimientos.json`.
- **Modo vigilante** `--watch`: icono en la bandeja del sistema, instancia única,
  escaneo en hilo aparte para no congelar el menú.
- **Protección** con comodines (`*`, `?`) y exclusiones de extensiones incompletas
  (`.part`, `.crdownload`, ...).
- No toca archivos en curso de escritura (tamaño/edad estable) ni ocultos/sistema.
- **Fail-closed**: si `reglas.json` falta o es inválido, aborta (código 2) en vez
  de mover archivos sin reglas.
- Rutas **UNC** (`\\servidor\recurso`) y `\\?\` soportadas.
- **`--selftest`**: pruebas internas (patrones, JSON, anticolisión, fail-closed,
  flujo de deshacer).
- Binario estático, sin dependencias.

### Notas
- Hallazgos no críticos conocidos y documentados: rutas largas >260, carrera sobre
  el `.tmp` entre procesos y límite de profundidad del parser JSON.
- El `.exe` se publica como Release junto a su `SHA256SUMS.txt`, no en el repositorio.
