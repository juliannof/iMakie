# RS485 Refactoring — 2026-04-27

## Cambio realizado

Se extrajo toda la documentación relacionada con RS485 del archivo monolítico `CLAUDE.md` (originalmente 1006 líneas) a un documento dedicado `docs/RS485_PROTOCOL.md`.

### Contenido migrado

1. **Especificación exacta de paquetes**
   - MasterPacket (16 bytes)
   - SlavePacket (9 bytes)
   - Flags de automatización
   - Flags de calibración

2. **Máquina de estados**
   - Ciclo de comunicación SEND → WAIT_RESP → GAP
   - Diagramas de timing
   - Parámetros críticos (TX_ENABLE_US, RESP_TIMEOUT_US, GAP_US, etc.)

3. **CRC8 — Cálculo y validación**
   - Polinomio exacto
   - Pseudocódigo implementación
   - Ejemplos

4. **Sincronización y lectura**
   - Búsqueda de encabezado
   - Limpieza pre-TX
   - Problemas identificados

5. **Protocolo de calibración**
   - Secuencia one-shot
   - Estados del motor
   - Error handling y retries

6. **Configuración por dispositivo**
   - P4 Master (Bus A)
   - S3 Extender (Bus B)
   - S2 Slave (todos los buses)

7. **RS485Profiler**
   - Qué es y por qué es diferente de logging tradicional
   - Salida esperada
   - Interpretación de métricas
   - Patrones diagnósticos

8. **Troubleshooting RS485**
   - Síntoma: alto porcentaje de timeouts
   - Síntoma: ID Mismatch
   - Síntoma: CRC errors
   - Síntoma: Slave no responde
   - Síntoma: Master nunca recibe respuesta

9. **Historial de optimización (2026-04-27)**
   - Problema diagnosticado (25-43% timeout rates)
   - Causas raíz identificadas (3 categorías)
   - Soluciones aplicadas con código antes/después
   - Resultados esperados

10. **Referencias y notas finales**
    - Transceivers recomendados
    - Historial de cambios
    - Notas de arquitectura

### Beneficios

| Aspecto | Antes | Después |
|---|---|---|
| **CLAUDE.md** | 1006 líneas | ~300 líneas (más legible) |
| **Navigación RS485** | Scroll 437 líneas | Link directo a docs/ |
| **Mantenimiento** | Cambio RS485 → buscar en todo archivo | Editar solo RS485_PROTOCOL.md |
| **Búsqueda** | grep en CLAUDE.md | grep en RS485_PROTOCOL.md |
| **Escalabilidad** | Crecimiento monolítico | Modular, fácil de expandir |

### Referencias en CLAUDE.md

CLAUDE.md ahora contiene una sección "Protocolo RS485" que remite al nuevo documento:

```markdown
## Protocolo RS485

**→ [Ver documentación completa en `docs/RS485_PROTOCOL.md`](docs/RS485_PROTOCOL.md)**

RS485 es el bus serial que conecta master (P4 o S3) con slaves (S2):
- **Baudrate:** 500 kbaud, 8N1
- **Protocolo:** Binario custom, CRC8, topología star
- **Timing:** Ciclo ~20ms para 8 slaves, timeouts críticos en microsegundos
- **Bus A (P4):** 9 slaves, TX=GPIO50, RX=GPIO51, EN=GPIO52
- **Bus B (S3):** 8 slaves, TX=GPIO15, RX=GPIO16, EN=GPIO1
- **Slaves (S2):** TX=GPIO8, RX=GPIO9, EN=GPIO35
```

### Próximos pasos de modularización

Esta es la **primera iteración** de modularización. Recomendado continuar con:

1. **Separar hardware S2, P4, S3** en documentos individuales
2. **Crear BUILD_AND_DEPLOY.md** para compilación, cargas, OTA
3. **Crear TROUBLESHOOTING.md** con guía por síntoma transversal
4. **Crear CHANGELOG.md** para historial de todas las optimizaciones
5. **Crear ARCHITECTURE.md** con visión general del proyecto

### Notas técnicas

- RS485_PROTOCOL.md NO distingue entre P4, S2, S3 (como se pidió)
- Toda configuración específica de dispositivo está centralizada en una sección
- Troubleshooting aplicable a cualquier parte del bus
- Profiler y optimización históricas están contextualizadas

---

**Fecha:** 2026-04-27  
**Decisión:** Refactorización aprobada por usuario  
**Impacto:** CLAUDE.md reducido 70% en contenido RS485, mantenibilidad mejorada
