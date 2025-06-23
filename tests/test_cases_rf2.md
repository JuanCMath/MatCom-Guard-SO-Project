# Casos de Prueba para RF2 - Monitoreo Constante del uso de Recursos para Procesos e Hilos

## CP-RF2-001: Detección de Procesos Nuevos
**Objetivo:** Verificar que el sistema detecta cuando se inicia un nuevo proceso
**Precondiciones:** Sistema de monitoreo activo
**Pasos:**
1. Iniciar el sistema de monitoreo
2. Ejecutar un nuevo proceso (ej: `sleep 60 &`)
3. Esperar un ciclo de monitoreo
**Resultado Esperado:** El nuevo proceso aparece en la lista de procesos monitoreados

## CP-RF2-002: Detección de Procesos Terminados
**Objetivo:** Verificar que el sistema detecta cuando un proceso termina
**Precondiciones:** Proceso conocido ejecutándose
**Pasos:**
1. Identificar un proceso en ejecución
2. Terminar el proceso (`kill <pid>`)
3. Esperar un ciclo de monitoreo
**Resultado Esperado:** El proceso se remueve de la lista de procesos monitoreados

## CP-RF2-003: Medición de Uso de CPU
**Objetivo:** Verificar que el sistema mide correctamente el uso de CPU por proceso
**Precondiciones:** Sistema de monitoreo activo
**Pasos:**
1. Ejecutar un proceso con alto uso de CPU (`yes > /dev/null &`)
2. Esperar varios ciclos de monitoreo
3. Verificar las métricas de CPU reportadas
**Resultado Esperado:** El proceso muestra uso de CPU elevado (>80%)

## CP-RF2-004: Medición de Uso de Memoria
**Objetivo:** Verificar que el sistema mide correctamente el uso de memoria por proceso
**Precondiciones:** Sistema de monitoreo activo
**Pasos:**
1. Ejecutar un proceso que consuma memoria significativa
2. Monitorear las métricas de memoria
3. Verificar que los valores son consistentes con herramientas del sistema
**Resultado Esperado:** Los valores de memoria son precisos (±5% vs `ps` o `top`)

## CP-RF2-005: Alertas por Alto Uso de CPU
**Objetivo:** Verificar que se generan alertas cuando un proceso excede el umbral de CPU
**Precondiciones:** Umbral de CPU configurado (ej: 90%)
**Pasos:**
1. Configurar umbral de CPU en 50%
2. Ejecutar proceso con alto uso de CPU
3. Esperar detección de umbral
**Resultado Esperado:** Se genera alerta de alto uso de CPU

## CP-RF2-006: Alertas por Alto Uso de Memoria
**Objetivo:** Verificar que se generan alertas cuando un proceso excede el umbral de memoria
**Precondiciones:** Umbral de memoria configurado
**Pasos:**
1. Configurar umbral de memoria en valor bajo
2. Ejecutar proceso que consuma memoria
3. Esperar detección de umbral
**Resultado Esperado:** Se genera alerta de alto uso de memoria

## CP-RF2-007: Duración de Alertas
**Objetivo:** Verificar que las alertas se mantienen por el tiempo configurado
**Precondiciones:** Duración de alerta configurada (ej: 10 segundos)
**Pasos:**
1. Generar una alerta
2. Reducir el uso de recursos del proceso
3. Verificar que la alerta permanece activa por el tiempo configurado
**Resultado Esperado:** La alerta se despeja después del tiempo configurado

## CP-RF2-008: Lista Blanca de Procesos
**Objetivo:** Verificar que los procesos en lista blanca no generan alertas
**Precondiciones:** Lista blanca configurada
**Pasos:**
1. Agregar un proceso a la lista blanca
2. Hacer que ese proceso exceda los umbrales
3. Verificar que no se generan alertas
**Resultado Esperado:** No se generan alertas para procesos en lista blanca

## CP-RF2-009: Intervalo de Monitoreo
**Objetivo:** Verificar que el sistema respeta el intervalo de monitoreo configurado
**Precondiciones:** Intervalo configurado (ej: 5 segundos)
**Pasos:**
1. Configurar intervalo de 5 segundos
2. Monitorear timestamps de las mediciones
3. Verificar la frecuencia de actualizaciones
**Resultado Esperado:** Las mediciones se realizan cada 5 segundos (±1 segundo)

## CP-RF2-010: Thread Safety
**Objetivo:** Verificar que el sistema es thread-safe en acceso concurrente
**Precondiciones:** Sistema multi-hilo activo
**Pasos:**
1. Iniciar monitoreo en un hilo
2. Acceder a datos desde otro hilo (GUI)
3. Realizar operaciones concurrentes por 1 minuto
**Resultado Esperado:** No hay corrupción de datos ni crashes

## CP-RF2-011: Monitoreo de Hilos (si implementado)
**Objetivo:** Verificar que el sistema puede monitorear hilos individuales
**Precondiciones:** Proceso multi-hilo ejecutándose
**Pasos:**
1. Ejecutar aplicación multi-hilo
2. Verificar detección de hilos individuales
3. Monitorear recursos por hilo
**Resultado Esperado:** Se detectan y monitorizan hilos individuales

## CP-RF2-012: Persistencia de Configuración
**Objetivo:** Verificar que la configuración se mantiene entre reinicios
**Precondiciones:** Archivo de configuración modificado
**Pasos:**
1. Modificar configuración (umbrales, intervalos)
2. Reiniciar el sistema de monitoreo
3. Verificar que la configuración se mantiene
**Resultado Esperado:** La configuración persiste correctamente

## CP-RF2-013: Manejo de Errores
**Objetivo:** Verificar el manejo correcto de errores del sistema
**Precondiciones:** Sistema de monitoreo activo
**Pasos:**
1. Simular condiciones de error (proceso inexistente, permisos insuficientes)
2. Verificar que el sistema continúa funcionando
3. Verificar logs de error apropiados
**Resultado Esperado:** El sistema maneja errores gracefully sin crash

## CP-RF2-014: Rendimiento del Sistema
**Objetivo:** Verificar que el monitoreo no impacta significativamente el rendimiento
**Precondiciones:** Sistema bajo carga normal
**Pasos:**
1. Medir rendimiento del sistema sin monitoreo
2. Activar monitoreo y medir nuevamente
3. Comparar métricas de rendimiento
**Resultado Esperado:** Impacto en rendimiento < 5% de CPU y memoria

## CP-RF2-015: Callbacks de Eventos
**Objetivo:** Verificar que los callbacks se ejecutan correctamente
**Precondiciones:** Callbacks configurados
**Pasos:**
1. Registrar callbacks para todos los eventos
2. Generar eventos (nuevo proceso, alerta, etc.)
3. Verificar que los callbacks se ejecutan
**Resultado Esperado:** Todos los callbacks se ejecutan en el momento correcto

## Matriz de Trazabilidad
| Caso de Prueba | Requerimiento | Prioridad | Estado |
|---------------|---------------|-----------|---------|
| CP-RF2-001    | Detección de procesos nuevos | Alta | Implementado |
| CP-RF2-002    | Detección de procesos terminados | Alta | Implementado |
| CP-RF2-003    | Medición CPU | Alta | Implementado |
| CP-RF2-004    | Medición Memoria | Alta | Implementado |
| CP-RF2-005    | Alertas CPU | Alta | Implementado |
| CP-RF2-006    | Alertas Memoria | Alta | Implementado |
| CP-RF2-007    | Duración alertas | Media | Implementado |
| CP-RF2-008    | Lista blanca | Media | Implementado |
| CP-RF2-009    | Intervalo monitoreo | Media | Implementado |
| CP-RF2-010    | Thread safety | Alta | Implementado |
| CP-RF2-011    | Monitoreo hilos | Baja | No implementado |
| CP-RF2-012    | Persistencia config | Media | Implementado |
| CP-RF2-013    | Manejo errores | Alta | Implementado |
| CP-RF2-014    | Rendimiento | Media | Pendiente verificar |
| CP-RF2-015    | Callbacks | Alta | Implementado |
