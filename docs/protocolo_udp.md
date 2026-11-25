# Protocolo UDP ESP32-S3 ↔ Android (`LDS1` v1)

El ESP32-S3 es **solo un puente**: lee el UART del LD06, pone una marca de tiempo a cada paquete y lo reenvía por UDP sin decodificarlo. Todo el procesamiento ocurre en el móvil.

## Red

| Parámetro | Valor por defecto |
|---|---|
| Modo WiFi del ESP32 | Punto de acceso (AP); el móvil se conecta a él. No se necesita Internet. |
| IP del ESP32 | `192.168.4.1` |
| Puerto UDP del ESP32 | `4211` (recibe peticiones del móvil y envía desde aquí) |
| Puerto UDP del móvil | `4210` (recibe datos) |

**Descubrimiento:** el móvil envía `SYNC_REQUEST` a `192.168.4.1:4211` cada 50–250 ms. El ESP32 toma la IP y el puerto de origen del primer mensaje recibido y empieza a enviarle `DATA` ahí. Si pasan 3 s sin recibir nada del móvil, el ESP32 deja de enviar.

## Cabecera común (12 bytes, little endian)

| Offset | Tipo | Campo |
|---|---|---|
| 0 | u32 | `magic` = `0x3153444C` (bytes `'L' 'D' 'S' '1'`) |
| 4 | u8 | `version` = 1 |
| 5 | u8 | `type`: 1 DATA, 2 SYNC_REQUEST, 3 SYNC_RESPONSE, 4 HELLO |
| 6 | u8 | `count` (solo DATA: número de paquetes LD06) |
| 7 | u8 | `flags` (reservado, 0) |
| 8 | u32 | `seq`: secuencia por tipo de mensaje; sirve para detectar pérdidas |

## DATA (ESP32 → móvil)

Cabecera seguida de `count` entradas (1 ≤ count ≤ 24) de 55 bytes:

| Offset | Tipo | Campo |
|---|---|---|
| 0 | u64 | `esp_time_us`: `esp_timer_get_time()` al recibir el **último byte** del paquete LD06 |
| 8 | u8[47] | Paquete LD06 en bruto, tal cual sale del UART (cabecera `0x54 0x2C` … CRC8) |

Recomendación de agrupado: 8 paquetes por datagrama (≈21 ms), o enviar lo acumulado si pasan 30 ms. Con 375 paquetes/s salen unos 47 datagramas/s y unos 21 KB/s.

## SYNC_REQUEST (móvil → ESP32) y SYNC_RESPONSE (ESP32 → móvil)

```
SYNC_REQUEST : cabecera + i64 phone_send_ns
SYNC_RESPONSE: cabecera + i64 phone_send_ns (eco) + u64 esp_rx_us + u64 esp_tx_us
```

El ESP32 responde al momento, copiando el mismo `seq`. Con `esp_rx_us` y `esp_tx_us`, más las horas de envío y recepción del móvil, este estima el desfase y la deriva entre relojes como en NTP. Solo usa las muestras con menor RTT y ajusta una regresión lineal (ver [clock_sync.cpp](../android/app/src/main/cpp/core/clock_sync.cpp)).

## HELLO (ESP32 → broadcast, 1 Hz, opcional)

`cabecera + u32 uptime_s + u8 fw_major + u8 fw_minor`. El móvil lo usa para descubrir el ESP32 si este no está en `192.168.4.1`.

## Base de tiempos en el móvil

- Reloj del móvil: `CLOCK_BOOTTIME` (`SystemClock.elapsedRealtimeNanos`). Es el mismo que usan las marcas de tiempo de cámara de ARCore en la mayoría de dispositivos.
- Instante del último punto de un paquete: `t_fin = espToPhone(esp_time_us) − latencia_LD06`. Por defecto la latencia es 2,5 ms; se calibra en *Ajustes → Calibración*.
- Los 12 puntos se reparten uniformemente hacia atrás en el tiempo según `speed` y el arco que cubre el paquete. Cada punto se transforma con la pose interpolada en su propio instante, lo que corrige la deformación por movimiento.
