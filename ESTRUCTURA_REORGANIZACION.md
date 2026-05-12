# Reorganización de proyecto AITEC (2026-05-12 17:47)

## Resumen
Se reorganizó el repositorio iMakie de estructura descentralizada a centralizada bajo `AITEC/`.

## Estructura anterior
```
iMakie/
├── S3/                   (master P4 + extender S3)
├── track S2/             (slave S2)
└── ...
```

## Estructura nueva (actual)
```
AITEC/
├── MASTER_S3-P4/
│   ├── P4/               ← Master MCU (ESP32-P4)
│   └── S3/
│       └── iMakie-ESP32_S3_EXTENDER/  ← Extender (ESP32-S3)
├── S2/
│   └── S2_V1/            ← Slaves (ESP32-S2 ×17)
├── C3_PowerRelay/        ← Proyecto separado (power control)
├── CLAUDE.md             ← Documentación crítica
├── STATUS.md             ← Referencia técnica
├── CHANGELOG.md
├── README.md
├── platformio.ini        ← Índice de proyectos
└── .gitignore
```

## Cambios realizados

### 1. Rutas en CLAUDE.md
- **Línea 146-151:** Agregada sección "Estructura de carpetas" con diagrama
- **Línea 153-159:** Actualizada tabla "Subproyectos PlatformIO" con:
  - Rutas nuevas completas (`MASTER_S3-P4/P4/`, `MASTER_S3-P4/S3/iMakie-ESP32_S3_EXTENDER/`, `S2/S2_V1/`)
  - Columna "Compilar" con comandos correctos

### 2. Sección Repositorio en CLAUDE.md
- **Líneas 679-693:** Actualizado con:
  - URL local nueva: `/Users/julianno/Documents/PlatformIO/Projects/AITEC/`
  - Comandos `pio run` para cada subproyecto

### 3. platformio.ini raíz
- **Actualizado:** Comentarios con estructura nueva y comandos compilación

## Verificación post-reorganización

### ✅ Directorios principales
- [x] `MASTER_S3-P4/P4/` → platformio.ini + src/ + include/
- [x] `MASTER_S3-P4/S3/iMakie-ESP32_S3_EXTENDER/` → platformio.ini + src/ + include/
- [x] `S2/S2_V1/` → platformio.ini + src/ + include/

### ✅ config.h en cada proyecto
- [x] P4: `/Users/julianno/Documents/PlatformIO/Projects/AITEC/MASTER_S3-P4/P4/src/config.h`
- [x] S3: `/Users/julianno/Documents/PlatformIO/Projects/AITEC/MASTER_S3-P4/S3/iMakie-ESP32_S3_EXTENDER/src/config.h`
- [x] S2: `/Users/julianno/Documents/PlatformIO/Projects/AITEC/S2/S2_V1/src/config.h`

### ✅ Documentación actualizada
- [x] CLAUDE.md: Tabla de Subproyectos y Repositorio
- [x] platformio.ini: Comentarios de estructura nueva

## Comandos compilación (correctos tras reorganización)

```bash
# Master P4
cd /Users/julianno/Documents/PlatformIO/Projects/AITEC/MASTER_S3-P4/P4
pio run

# Extender S3
cd /Users/julianno/Documents/PlatformIO/Projects/AITEC/MASTER_S3-P4/S3/iMakie-ESP32_S3_EXTENDER
pio run

# Slave S2
cd /Users/julianno/Documents/PlatformIO/Projects/AITEC/S2/S2_V1
pio run
```

## Notas importantes

1. **Cada proyecto tiene su `config.h` independiente** — las variables de S2 no afectan P4/S3
2. **El cambio es estructural, no funcional** — código sin cambios
3. **CLAUDE.md es el source of truth** — actualizado con nueva estructura
4. **GitHub seguirá como `juliannof/iMakie`** — solo cambió ruta local

## Próximos pasos

- [ ] Compilar cada proyecto en máquina local para validar rutas include/
- [ ] Verificar que OTA/WiFi/RS485 inicialización no afectados
- [ ] Documentar en CHANGELOG.md si hay cambios funcionales
