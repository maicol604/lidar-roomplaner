# LiDAR Scanner — escáner 3D con LD06 + ESP32-S3 + Android

App Flutter (solo Android) que convierte un LiDAR 2D **LDROBOT LD06**, sujeto rígidamente al móvil, en un escáner 3D de mano. El usuario recorre la vivienda barriendo con el plano láser y ve crecer la nube de puntos en tiempo real **y, como en RoomPlan de Apple, una miniatura 3D de la vivienda** (paredes, puertas, ventanas y muebles como cajas) que se puede girar y ampliar. Al terminar se obtiene el modelo 3D y un **plano de planta limpio con medidas en metros**, exportable a SVG.

**Condición del proyecto:** todo se procesa y visualiza **en local en el móvil**, sin backend ni Internet. El ESP32-S3 solo hace de puente (UART → WiFi UDP). El móvil es la «pantalla del escáner»: muestra una vista previa en directo y graba la sesión en bruto para reprocesarla después en PC con otras herramientas (CloudCompare, Foxglove, ROS 2…).

## Arquitectura

```
LD06 ──UART 230400──► ESP32-S3 ──WiFi UDP (LDS1)──► Android
                       marca de tiempo µs             │
                       agrupa paquetes                ▼
┌──────────────────────────── núcleo nativo C++ (NDK) ────────────────────────────┐
│ UdpTransport / MockWorld → decodificador LD06 (CRC) → ClockSync (NTP + deriva)   │
│ → PoseBuffer (interpolación por punto = corrección de movimiento) → MappingEngine│
│   (submapas anclados, vóxeles, cobertura, suelo, límite de puntos) → MapStore    │
│ → LayoutEngine (corrección de deriva por grafo de poses + paredes, huecos,       │
│   muebles y estancias; ~1 Hz) → miniatura 3D y plano en Flutter                  │
│ → Renderer OpenGL ES 3 (hilo EGL propio, SurfaceProducer → Texture de Flutter)   │
│ → Recorder MCAP (ros2msg/CDR) + Exportador PLY/PCD/CSV/JSON                       │
└──────────────────────────────────────────────────────────────────────────────────┘
Kotlin: ARCore (simulador hoy; odometría real en F3), sensores IMU, batería/temperatura, canales de Flutter
Flutter + Riverpod: interfaz, HUD, alertas, minimapa, sesiones, ajustes (es/en, claro/oscuro)
```

Decisiones clave:

- **Plano vertical transversal:** el eje de giro del LD06 apunta hacia delante. Cada vuelta es un perfil pared–techo–pared–suelo, y al avanzar se barre la vivienda.
- **Corrección por punto:** cada uno de los 12 puntos de un paquete LD06 tiene su propio instante y se transforma con la pose interpolada (lineal + SLERP). Así se evita la deformación de las paredes al moverse.
- **Mapa corregible sin saltos:** los puntos se guardan relativos a un *anchor* (un submapa cada 2 m, 45° o 20 s). Cuando la odometría corrige la deriva, solo cambia la matriz del submapa. El renderizador anima ese cambio y la nube no salta.
- **Odometría:** continua con ARCore (F3) y, en modo por estaciones, solo con orientación. El mock reproduce ambos casos, con deriva, pérdidas de seguimiento y cierres de bucle.
- **Sesión en bruto en MCAP:** guarda los datagramas UDP tal cual, las sincronizaciones de reloj, las poses, el estado de seguimiento, los anchors y la calibración. Así la sesión se puede reprocesar con mejores algoritmos en PC.

Especificación del protocolo ESP32 ↔ móvil: [docs/protocolo_udp.md](docs/protocolo_udp.md).

## Estructura

| Ruta | Contenido |
|---|---|
| [android/app/src/main/cpp/core](android/app/src/main/cpp/core) | LD06, protocolo, sincronización de relojes, poses, mapeo, cobertura, modelo estructurado ([layout_engine](android/app/src/main/cpp/core/layout_engine.h)) |
| [android/app/src/main/cpp/mock](android/app/src/main/cpp/mock) | Vivienda sintética, recorrido automático y simulador del hardware (LD06 en bytes, ESP32, odometría tipo ARCore) |
| [android/app/src/main/cpp/render](android/app/src/main/cpp/render) | Renderizador OpenGL ES 3 de la nube (modos orbital, seguir, 1ª persona y planta) |
| [android/app/src/main/cpp/io](android/app/src/main/cpp/io) | Grabación MCAP y exportadores |
| [android/app/src/main/cpp/tests](android/app/src/main/cpp/tests) | Tests nativos (se ejecutan en el dispositivo) |
| [android/app/src/main/kotlin](android/app/src/main/kotlin/com/example/lidarscanner) | JNI, canales de Flutter, información del dispositivo |
| [lib/](lib) | App Flutter (Riverpod, i18n es/en); miniatura 3D y plano en [lib/features/layout](lib/features/layout) |

## Compilar y probar

```powershell
flutter pub get
flutter test                                   # tests Dart
.\tool\run_native_tests.ps1 -Device <serial>   # tests C++ en el dispositivo (NDK + adb)
flutter run -d <serial>
```

Requisitos: Flutter 3.47+, Android SDK con NDK 28.2 y CMake 3.22.1, y un dispositivo con OpenGL ES 3.0 (Android 7+).

## Uso del simulador

*Nuevo escaneo → Simulador*. No hay joysticks: el simulador sigue el **movimiento real del móvil**, como en el producto final. Coge el móvil en vertical, con la cámara trasera hacia delante, y camina. El escáner virtual recorre un piso sintético de 10 × 8 m y la nube de puntos crece en tiempo real.

- **Seguimiento:** se usa ARCore (cámara + IMU, 6 grados de libertad) y aparece una miniatura de la cámara. Se pide permiso de cámara la primera vez. Si el móvil no admite ARCore, si falta *Servicios de Google Play para RA* o si se deniega el permiso, se usan los sensores: el vector de rotación para la orientación y la detección de pasos (zancada de 0,65 m) para el avance. Un chip indica qué fuente está activa.
- **Anclaje:** la primera pose con seguimiento se coloca en el punto de inicio del piso virtual (salón, mirando al norte) a escala 1:1. A partir de ahí cada metro real es un metro virtual, y la altura a la que sostienes el móvil al empezar equivale a 1,4 m.
- **Paredes virtuales:** tu espacio real no coincide con el piso sintético. Puedes atravesar paredes y muebles virtuales; aparece un aviso si estás dentro de uno o fuera del piso.
- **Odometría:** encima de la pose real se sigue simulando la deriva, los cierres de bucle y las pérdidas de seguimiento configurados en *Ajustes → Simulador*. Las pérdidas reales de ARCore (poca luz, poca textura o movimiento brusco) también se reflejan y se muestra el motivo.
- La simulación va 120 ms por detrás del tiempo real para tener siempre la pose del móvil al generar cada punto del LD06.
- **Recorrido automático** (*Nuevo escaneo → Simulador → Recorrido automático*): el escáner virtual recorre solo todo el piso (unos 4 minutos, con un giro completo en cada estancia y vuelta al inicio para cerrar el bucle). Sirve para probar la miniatura y el plano sin un espacio libre de 10 × 8 m. Como nadie puede seguir los consejos de movimiento, en este modo se ocultan las alertas de velocidad e inclinación.

Sobre la vista 3D: arrastrar con un dedo rota la cámara, con dos dedos desplaza y hace zoom, y un doble toque recentra. En *Ajustes → Simulador* se configuran el ruido, los cristales y espejos, la deriva, los cierres de bucle y las pérdidas de paquetes o de seguimiento. La pantalla de diagnóstico usa el simulador con el escáner quieto.

Las alertas de calidad, que siempre están visibles, avisan de:

- separación entre perfiles a 3 m por encima de 5 cm (moverse más despacio);
- inclinación del plano de escaneo por encima de 15°;
- inicialización o pérdida de odometría;
- falta de datos;
- desplazamiento en el modo por estaciones;
- simulador: estar dentro de una pared o un mueble virtual, o fuera del piso;
- temperatura alta del móvil.

## Modelo 3D y plano (estilo RoomPlan)

Mientras se escanea, una **miniatura 3D en directo** (abajo a la derecha) va levantando las paredes, puertas, ventanas y muebles a medida que se detectan, con un punto que marca la posición del móvil. Se puede girar arrastrando, ampliar con dos dedos y recentrar con doble toque. El botón ⤢ la amplía y muestra controles con botones (girar, inclinar, zoom, vista cenital y recentrar).

Al terminar, el visor de la sesión tiene tres pestañas:

- **Modelo 3D:** el modelo final, con interruptor de muebles y acceso a las medidas.
- **Plano:** plano de planta limpio, sin muebles. Muestra paredes con su grosor, puertas con su barrido, ventanas, estancias con nombre, superficie y dimensiones, cotas exteriores en metros (separador decimal del idioma), barra de escala y leyenda. Tiene una **lista de medidas** (estancias, paredes, huecos, altura libre y deriva corregida) y **Exportar SVG** (`floorplan.svg`, en centímetros, listo para CAD o impresión).
- **Nube:** la nube de puntos (el visor anterior).

### Cómo se genera (`LayoutEngine`, C++)

Se ejecuta en un hilo propio aproximadamente una vez por segundo. Al terminar se repite con todas las alineaciones y tres pasadas, y el resultado se guarda en `layout.json`.

1. **Rejilla de alturas:** cada submapa guarda celdas de 5 cm con una máscara de alturas cada 10 cm (y otra solo con retornos de intensidad alta para distinguir el cristal).
2. **Corrección de deriva (ir y volver):** los submapas consecutivos se agrupan en nodos de un grafo de poses 2D (x, z, rumbo). Las aristas de odometría unen nodos consecutivos con una confianza que baja con la distancia. Las aristas de cierre de bucle salen de un ICP punto-a-recta entre nodos no consecutivos que ven la misma estructura. El ICP solo usa superficies verticales entre 0,6 y 2 m, no empareja las dos caras de un tabique y no se mueve en direcciones degeneradas como el eje de un pasillo. Cada arista se valida con una puerta de deriva máxima según la distancia recorrida y lleva la matriz de información del ICP. El sistema se resuelve por mínimos cuadrados con gradiente conjugado y se descartan por χ² las aristas inconsistentes. La corrección se aplica también a la nube (render, PLY/PCD y trayectoria exportados), así que nube, modelo y plano coinciden.
3. **Fantasmas:** los espejos (y algunos cristales) devuelven puntos *detrás* de la pared. Una celda que el sensor vio mayoritariamente *a través* de una pared maciza (no de una puerta o ventana) se descarta.
4. **Paredes:** son las celdas con puntos en la franja alta (por encima de dinteles y de casi todos los muebles). La orientación dominante (Manhattan) se calcula y se refina con líneas completas. Las caras se extraen por picos y, para paredes oblicuas, por RANSAC. Después se regularizan: las dos caras de un tabique visto desde ambos lados forman una pared con su grosor medido, los duplicados paralelos se promedian, los tramos colineales se unen y las esquinas se cierran (hasta 1,6 m si no se ve suelo en el hueco, porque las esquinas suelen quedar tapadas por muebles).
5. **Huecos:** una **puerta** no tiene nada por debajo del dintel y se confirma porque la trayectoria la cruza o porque hay suelo a ambos lados. Una **ventana** tiene antepecho abajo y vacío o cristal arriba.
6. **Estancias:** suelo observado delimitado por las paredes (con las puertas cerradas). Los huecos bajo los muebles se rellenan. Se calculan superficie, dimensiones y el punto más interior para la etiqueta. Las estancias largas y estrechas se llaman *Pasillo*.
7. **Muebles:** agrupaciones compactas fuera de las paredes, como cajas alineadas a los ejes del plano. Se clasifican en bajo, mesa (hueco por debajo) y alto. Se descartan los que caen fuera de las estancias.

Precisión en el piso sintético (tests nativos): sin deriva, 12 paredes con un error de 1,4 cm RMS, 6/6 puertas y 6 estancias que suman 72,6 m² (≈ 75 m² reales a cara de pared). Con deriva de 0,12°/m y 0,8 % de escala, el error baja de 12 cm a 4 cm tras corregir, y la superficie, de 62,6 a 72,4 m².

Limitaciones actuales: las medidas son aproximadas (se avisa en el plano). Los armarios que llegan al techo pueden confundirse con paredes. La puerta se dibuja con un barrido convencional porque el sentido real de apertura no se conoce.

### Accesibilidad (WCAG 2.2 AA)

- Todo gesto tiene alternativa con un toque: botones de girar, inclinar, zoom, vista cenital y recentrar en el modelo, y zoom/encajar en el plano (2.5.1, 2.5.7). Los objetivos táctiles miden 48 dp (2.5.8).
- Alternativas textuales: el modelo anuncia un resumen ("12 paredes, 6 puertas… · 72,4 m²") y ofrece acciones para lectores de pantalla (girar, ampliar, recentrar). El plano tiene la **lista de medidas** completa, que es su alternativa en texto (1.1.1).
- Las alertas de calidad son regiones *live*, así que se anuncian sin mover el foco (4.1.3). Sus fondos son opacos y garantizan ≥ 4,5:1 con texto blanco (1.4.3).
- Plano y modelo usan paletas con contraste AA en claro y oscuro (1.4.3, 1.4.11). Los estados no dependen solo del color, porque el texto los nombra y la leyenda identifica puertas y ventanas (1.4.1).
- Se respeta la preferencia de reducir animaciones y el tamaño de texto del sistema; el texto del plano se dibuja en coordenadas de pantalla y siempre es legible.
- Los botones solo con icono tienen nombre accesible (tooltip) y los interruptores van asociados a su etiqueta.

## Galería y visor 3D

Al tocar un escaneo en la galería se abre el **visor de resultados**, y también al terminar un escaneo. Arranca en la pestaña *Modelo 3D* (ver arriba). La pestaña *Nube* carga `cloud.ply` y la trayectoria en el mismo renderizador OpenGL que se usa durante el escaneo:

- **Vistas:** orbital y planta. La cámara encuadra la nube automáticamente según el tamaño de la pantalla.
- **Ocultar techo:** vista tipo casa de muñecas, activada por defecto.
- **Visualización:** color por altura, intensidad o antigüedad; tamaño de punto ajustable; minimapa de cobertura y recorrido.
- **Gestos:** los mismos que durante el escaneo (rotar, desplazar, zoom y doble toque para recentrar).
- **Información y exportación:** desde el botón ⓘ (compartir o eliminar la sesión).

## Exportación

Cada sesión se guarda en `<documentos de la app>/sessions/<fecha>/` y se comparte desde el menú Compartir de Android:

| Archivo | Contenido |
|---|---|
| `raw.mcap` | Sesión en bruto (perfil ros2, CDR). Se abre con Foxglove o `ros2 bag`. |
| `cloud.ply` / `cloud.pcd` | Nube de preview ya corregida (Z arriba, metros; campos de intensidad y tiempo) |
| `trajectory.csv` / `.json` | Trayectoria corregida (Z arriba) |
| `layout.json` | Modelo estructurado: paredes (eje, grosor, altura, puertas y ventanas), muebles, estancias (superficie, rectángulos en el marco del plano) y diagnóstico de la corrección. Coordenadas de planta `(x, z)` del marco interno Y arriba (= `(x, -y)` de los exportables Z arriba), en metros |
| `floorplan.svg` | Plano de planta limpio con cotas (se genera con *Exportar SVG*) |
| `session.json` | Resumen de la sesión (incluye superficie, estancias, paredes y huecos) |

Las sesiones antiguas, sin `layout.json`, lo calculan al abrirse a partir de `cloud.ply`. En ese caso no se puede corregir la deriva, porque la nube ya está fusionada.

## Plan por fases

| Fase | Estado | Contenido |
|---|---|---|
| F0 | ✅ | Proyecto Flutter Android, núcleo C++ (NDK), JNI, canales y estructura |
| F1 | ✅ | Interfaz completa con mock: visor 3D en tiempo real, HUD, alertas, minimapa de cobertura, simulador guiado por el movimiento real del móvil (ARCore con respaldo IMU, miniatura de cámara), modo por estaciones, sesiones, MCAP y exportación, ajustes, diagnóstico polar, galería con visor 3D de sesiones guardadas |
| F1.5 | ✅ | Modelo estructurado estilo RoomPlan: miniatura 3D en directo, corrección de deriva por grafo de poses, filtro de fantasmas, plano de planta con cotas y exportación SVG, recorrido automático del simulador y repaso de accesibilidad WCAG 2.2 AA |
| F2 | ⏳ | Firmware del ESP32-S3 (UART → UDP `LDS1`, sincronización de reloj, AP WiFi) y validación con el LD06 real en la pantalla de diagnóstico |
| F3 | ⏳ | Plugin ARCore (Kotlin): poses de cámara en `CLOCK_BOOTTIME`, anchors, estado de seguimiento, cámara en miniatura, IMU e imágenes clave en el MCAP |
| F4 | ⏳ | Fusión real: verificación de la base de tiempos, calibración de la latencia del LD06 y de los extrínsecos LiDAR↔cámara, y modo por estaciones con giroscopio y ICP entre estaciones |
| F5 | ⏳ | Pruebas de campo y rendimiento en gama media y baja (límite de puntos, FPS, temperatura) |
