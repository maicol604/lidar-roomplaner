// ignore: unused_import
import 'package:intl/intl.dart' as intl;

import 'app_localizations.dart';

// ignore_for_file: type=lint

/// The translations for Spanish Castilian (`es`).
class AppLocalizationsEs extends AppLocalizations {
  AppLocalizationsEs([String locale = 'es']) : super(locale);

  @override
  String get appTitle => 'LiDAR Scanner';

  @override
  String get homeTitle => 'Escaneos';

  @override
  String get newScan => 'Nuevo escaneo';

  @override
  String get noSessions =>
      'Aún no hay escaneos. Pulsa «Nuevo escaneo» para empezar.';

  @override
  String get settings => 'Ajustes';

  @override
  String get diagnostics => 'Diagnóstico LD06';

  @override
  String get sessionName => 'Nombre';

  @override
  String defaultSessionName(String date) {
    return 'Escaneo $date';
  }

  @override
  String get source => 'Fuente de datos';

  @override
  String get sourceMock => 'Simulador (mock)';

  @override
  String get sourceMockDesc =>
      'Recorre una vivienda sintética moviendo el móvil de verdad (ARCore o sensores); LD06, ESP32 y odometría simulados';

  @override
  String get sourceEsp32 => 'ESP32 + LD06 (WiFi UDP)';

  @override
  String get sourceEsp32Desc =>
      'Hardware real. Hasta la fase 3 (ARCore) se usa una pose estática.';

  @override
  String get transport => 'Transporte del simulador';

  @override
  String get transportInProcess => 'En proceso';

  @override
  String get transportLoopback => 'UDP loopback (prueba la ruta de red)';

  @override
  String get mode => 'Modo de escaneo';

  @override
  String get modeContinuous => 'Continuo';

  @override
  String get modeContinuousDesc =>
      'Caminas barriendo con el plano vertical; la odometría sigue tu posición.';

  @override
  String get modeStation => 'Por estaciones';

  @override
  String get modeStationDesc =>
      'Giras sobre ti mismo en cada punto; se usa solo la orientación.';

  @override
  String get record => 'Grabar sesión (MCAP y exportables)';

  @override
  String get startScan => 'Empezar';

  @override
  String get startFailed =>
      'No se pudo iniciar el escáner (¿puerto UDP ocupado?)';

  @override
  String get stop => 'Detener';

  @override
  String get pause => 'Pausar';

  @override
  String get resume => 'Reanudar';

  @override
  String get stopConfirmTitle => '¿Terminar el escaneo?';

  @override
  String get stopConfirmBody =>
      'Se guardará la sesión y se generarán los archivos de exportación.';

  @override
  String get cancel => 'Cancelar';

  @override
  String get finish => 'Terminar';

  @override
  String get exporting => 'Guardando y exportando…';

  @override
  String get cameraOrbit => 'Orbital';

  @override
  String get cameraFollow => 'Seguir';

  @override
  String get cameraFirstPerson => '1ª persona';

  @override
  String get cameraTop => 'Planta';

  @override
  String get colorHeight => 'Altura';

  @override
  String get colorIntensity => 'Intensidad';

  @override
  String get colorAge => 'Antigüedad';

  @override
  String get hudPoints => 'Puntos';

  @override
  String get hudRate => 'Puntos/s';

  @override
  String get hudTracking => 'Odometría';

  @override
  String get hudLoss => 'Pérdidas';

  @override
  String get hudLatency => 'Latencia';

  @override
  String get hudRpm => 'RPM';

  @override
  String get hudDuration => 'Duración';

  @override
  String get hudDistance => 'Distancia';

  @override
  String get hudBattery => 'Batería';

  @override
  String get hudTemp => 'Temperatura';

  @override
  String get trackingTracking => 'OK';

  @override
  String get trackingLimited => 'Limitada';

  @override
  String get trackingLost => 'Perdida';

  @override
  String get trackingInit => 'Iniciando';

  @override
  String get trackingStatic => 'Estática';

  @override
  String alertSpeed(String gap) {
    return 'Muévete más despacio · $gap cm entre perfiles';
  }

  @override
  String alertTilt(String deg) {
    return 'Inclinación excesiva ($deg°): mantén el plano de escaneo vertical';
  }

  @override
  String get alertTracking =>
      'Odometría perdida: apunta la cámara a zonas con textura y luz';

  @override
  String get alertNoData => 'Sin datos del LiDAR';

  @override
  String get alertStationDisplaced =>
      'Te estás desplazando: en modo estación gira sin moverte';

  @override
  String get alertThermal => 'El móvil se está calentando';

  @override
  String stationCapturing(int n) {
    return 'Estación $n · capturando';
  }

  @override
  String get stationMoving => 'Desplazándote a la siguiente estación';

  @override
  String get stationFix => 'Fijar estación aquí';

  @override
  String get stationMove => 'Ir a otra estación';

  @override
  String get hideHud => 'Ocultar datos';

  @override
  String get showHud => 'Mostrar datos';

  @override
  String get paused => 'En pausa';

  @override
  String get alertMotionInit =>
      'Iniciando seguimiento: mueve el móvil despacio apuntando a zonas con textura';

  @override
  String get alertInsideWall =>
      'Estás atravesando una pared o un mueble del piso virtual';

  @override
  String get alertOutside =>
      'Estás fuera del piso virtual: vuelve hacia el punto de inicio';

  @override
  String get motionFailLight => 'poca luz';

  @override
  String get motionFailMotion => 'movimiento demasiado rápido';

  @override
  String get motionFailFeatures => 'poca textura a la vista';

  @override
  String get motionFailCamera => 'cámara no disponible';

  @override
  String get motionArcore => 'ARCore';

  @override
  String get motionImu => 'Sensores';

  @override
  String get motionArcoreHint =>
      'Tu movimiento real se sigue con ARCore (cámara + IMU)';

  @override
  String get motionImuHint =>
      'Tu movimiento real se sigue con los sensores (giroscopio + detección de pasos)';

  @override
  String get motionFallbackCamera =>
      'Sin permiso de cámara: se usan los sensores del móvil (giroscopio y pasos)';

  @override
  String get motionFallbackInstall =>
      'Instala «Servicios de Google Play para RA» para usar ARCore; mientras tanto se usan los sensores';

  @override
  String get motionFallbackUnsupported =>
      'Este móvil no es compatible con ARCore: se usan los sensores (giroscopio y pasos)';

  @override
  String get motionFallbackError =>
      'No se pudo iniciar ARCore: se usan los sensores (giroscopio y pasos)';

  @override
  String get motionNone =>
      'Este móvil no tiene sensores de orientación: el simulador no puede seguir tu movimiento';

  @override
  String get simulatorWalkHint =>
      'Camina con el móvil en vertical: el escáner virtual sigue tu movimiento real por el piso sintético';

  @override
  String get resetView => 'Recentrar vista';

  @override
  String get colorMode => 'Color';

  @override
  String get cameraMode => 'Cámara';

  @override
  String get sessionDetail => 'Sesión';

  @override
  String get share => 'Compartir';

  @override
  String get delete => 'Eliminar';

  @override
  String get deleteConfirm => '¿Eliminar esta sesión y todos sus archivos?';

  @override
  String get files => 'Archivos';

  @override
  String get fileMcap => 'Sesión en bruto (MCAP, compatible ROS 2)';

  @override
  String get filePly => 'Nube de preview (PLY)';

  @override
  String get filePcd => 'Nube de preview (PCD)';

  @override
  String get fileCsv => 'Trayectoria (CSV)';

  @override
  String get fileJson => 'Trayectoria (JSON)';

  @override
  String get summaryDuration => 'Duración';

  @override
  String get summaryPoints => 'Puntos';

  @override
  String get summaryDistance => 'Distancia';

  @override
  String get summaryCorrections => 'Correcciones del mapa';

  @override
  String get summaryLoss => 'Datagramas perdidos';

  @override
  String get summarySource => 'Fuente';

  @override
  String get summaryMode => 'Modo';

  @override
  String get summaryVoxel => 'Vóxel final';

  @override
  String get incompleteSession => 'Sesión incompleta';

  @override
  String get shareNothing => 'Selecciona al menos un archivo';

  @override
  String get settingsAppearance => 'Apariencia';

  @override
  String get theme => 'Tema';

  @override
  String get themeSystem => 'Sistema';

  @override
  String get themeLight => 'Claro';

  @override
  String get themeDark => 'Oscuro';

  @override
  String get language => 'Idioma';

  @override
  String get languageSystem => 'Sistema';

  @override
  String get settingsViewer => 'Visor 3D';

  @override
  String get pointSize => 'Tamaño de punto';

  @override
  String get showTrajectory => 'Mostrar trayectoria';

  @override
  String get showGrid => 'Mostrar rejilla del suelo';

  @override
  String get ageWindow => 'Ventana de antigüedad (s)';

  @override
  String get targetFps => 'FPS objetivo';

  @override
  String get settingsHud => 'Datos en pantalla';

  @override
  String get hudVisibleByDefault => 'Mostrar datos al empezar';

  @override
  String get settingsMapping => 'Mapeo';

  @override
  String get voxel => 'Tamaño de vóxel (cm)';

  @override
  String get pointBudget => 'Límite de puntos';

  @override
  String budgetAuto(String n) {
    return 'Automático ($n)';
  }

  @override
  String get minRange => 'Distancia mínima (m)';

  @override
  String get maxRange => 'Distancia máxima (m)';

  @override
  String get minIntensity => 'Intensidad mínima';

  @override
  String get settingsAlerts => 'Alertas de calidad';

  @override
  String get tiltThreshold => 'Inclinación máxima (°)';

  @override
  String get gapThreshold => 'Separación máxima entre perfiles (cm)';

  @override
  String get settingsConnection => 'Conexión ESP32';

  @override
  String get espHost => 'IP del ESP32';

  @override
  String get espPort => 'Puerto UDP del ESP32';

  @override
  String get phonePort => 'Puerto UDP del móvil';

  @override
  String get settingsCalibration => 'Calibración LiDAR ↔ móvil';

  @override
  String get calibrationHint =>
      'Por defecto: LD06 sobre el móvil con el eje de giro hacia delante (plano vertical transversal). Los ángulos se aplican sobre ese montaje.';

  @override
  String get extTx => 'Desplazamiento X (cm)';

  @override
  String get extTy => 'Desplazamiento Y (cm)';

  @override
  String get extTz => 'Desplazamiento Z (cm)';

  @override
  String get extRoll => 'Alabeo (°)';

  @override
  String get extPitch => 'Cabeceo (°)';

  @override
  String get extYaw => 'Guiñada (°)';

  @override
  String get lidarLatency => 'Latencia del LD06 (ms)';

  @override
  String get settingsMock => 'Simulador';

  @override
  String get mockNoise => 'Ruido de distancia (± mm)';

  @override
  String get mockGlass => 'Huecos y fantasmas en cristales y espejos';

  @override
  String get mockIntensity => 'Intensidad según material';

  @override
  String get mockDrift => 'Deriva de odometría';

  @override
  String get mockDriftPct => 'Error de escala (%)';

  @override
  String get mockDriftYaw => 'Deriva de rumbo (°/m)';

  @override
  String get mockLoopClosure => 'Correcciones tipo ARCore (cierre de bucle)';

  @override
  String get mockPacketLoss => 'Pérdida de paquetes UDP';

  @override
  String get mockPacketLossPct => 'Pérdida de paquetes (%)';

  @override
  String get mockTrackingLoss => 'Pérdidas de odometría';

  @override
  String get mockTrackingLossRate => 'Pérdidas por minuto';

  @override
  String get resetDefaults => 'Restaurar valores por defecto';

  @override
  String get diagWaiting => 'Esperando datos del LD06…';

  @override
  String get diagStart => 'Iniciar';

  @override
  String get diagStop => 'Parar';

  @override
  String get diagRunningSession => 'Mostrando la sesión en curso';

  @override
  String get diagDatagrams => 'Datagramas/s';

  @override
  String get diagCrc => 'Errores CRC';

  @override
  String get diagRtt => 'RTT mínimo';

  @override
  String get diagDrift => 'Deriva de reloj';

  @override
  String get diagSynced => 'Reloj sincronizado';

  @override
  String get diagPeer => 'ESP32';

  @override
  String get diagPoints => 'Puntos en la vuelta';

  @override
  String get yes => 'Sí';

  @override
  String get no => 'No';

  @override
  String deviceInfoLine(String model, String ram) {
    return '$model · $ram MB RAM';
  }

  @override
  String get lowEndNotice =>
      'Gama baja detectada: límite de puntos y FPS reducidos';

  @override
  String get advanced => 'Opciones avanzadas';

  @override
  String get stationModeHint =>
      'Gira despacio sobre ti mismo. Después pulsa «Ir a otra estación», camina y fija la nueva estación.';

  @override
  String get loopClosures => 'Cierres de bucle';

  @override
  String get minimapHint =>
      'Naranja: recorrido · Verde: suelo visto · Blanco: paredes y obstáculos';

  @override
  String get viewCloud => 'Ver nube 3D';

  @override
  String get loadingCloud => 'Cargando nube…';

  @override
  String get noCloud =>
      'Esta sesión no tiene nube exportada (se interrumpió antes de terminar).';

  @override
  String cloudLoadError(String error) {
    return 'No se pudo cargar la nube: $error';
  }

  @override
  String get sessionInfo => 'Información y exportación';

  @override
  String cloudPoints(String n) {
    return '$n puntos';
  }

  @override
  String get hideCeiling => 'Ocultar techo';

  @override
  String get modelTitle => 'Modelo 3D';

  @override
  String get modelEmpty =>
      'Aún no hay modelo. Recorre el espacio y aparecerá aquí.';

  @override
  String get modelEmptyLive =>
      'Recorre el espacio: las paredes aparecerán aquí';

  @override
  String modelSummary(
    int walls,
    int doors,
    int windows,
    int objects,
    String area,
  ) {
    String _temp0 = intl.Intl.pluralLogic(
      walls,
      locale: localeName,
      other: '$walls paredes',
      one: '1 pared',
    );
    String _temp1 = intl.Intl.pluralLogic(
      doors,
      locale: localeName,
      other: '$doors puertas',
      one: '1 puerta',
    );
    String _temp2 = intl.Intl.pluralLogic(
      windows,
      locale: localeName,
      other: '$windows ventanas',
      one: '1 ventana',
    );
    String _temp3 = intl.Intl.pluralLogic(
      objects,
      locale: localeName,
      other: '$objects muebles',
      one: '1 mueble',
    );
    return '$_temp0, $_temp1, $_temp2, $_temp3 · $area';
  }

  @override
  String get modelGestureHint =>
      'Arrastra para girar, pellizca para ampliar y toca dos veces para recentrar';

  @override
  String get modelLiveTitle => 'Modelo en directo';

  @override
  String get modelLiveShort => 'En directo';

  @override
  String get expandModel => 'Ampliar modelo 3D';

  @override
  String get collapseModel => 'Reducir modelo 3D';

  @override
  String get rotateLeft => 'Girar a la izquierda';

  @override
  String get rotateRight => 'Girar a la derecha';

  @override
  String get tiltUp => 'Ver más desde arriba';

  @override
  String get tiltDown => 'Ver más de lado';

  @override
  String get zoomIn => 'Acercar';

  @override
  String get zoomOut => 'Alejar';

  @override
  String get topView => 'Vista cenital';

  @override
  String get fitPlan => 'Encajar el plano';

  @override
  String scaleBar(String label) {
    return 'Escala: $label';
  }

  @override
  String get roomCorridor => 'Pasillo';

  @override
  String roomNumbered(int n) {
    return 'Estancia $n';
  }

  @override
  String get legendDoor => 'Puerta';

  @override
  String get legendWindow => 'Ventana';

  @override
  String get legendWall => 'Pared';

  @override
  String get minimapSemantics =>
      'Mapa de cobertura: recorrido, suelo visto y paredes';

  @override
  String get expand => 'Toca para ampliar';

  @override
  String get collapse => 'Toca para reducir';

  @override
  String get cameraPreview => 'Vista de la cámara';

  @override
  String get closeScan => 'Terminar escaneo';

  @override
  String get recording => 'Grabando';

  @override
  String get cloudViewSemantics => 'Vista 3D de la nube de puntos';

  @override
  String get cloudViewHint =>
      'Arrastra con un dedo para girar y con dos para desplazar y ampliar; toca dos veces para recentrar';

  @override
  String get tabModel => 'Modelo 3D';

  @override
  String get tabPlan => 'Plano';

  @override
  String get tabCloud => 'Nube';

  @override
  String planSubtitle(String date, String area) {
    return '$date · $area útiles';
  }

  @override
  String get planDisclaimer =>
      'Medidas aproximadas obtenidas con el escáner LiDAR. Verifícalas antes de usarlas en obra.';

  @override
  String get planExport => 'Exportar SVG';

  @override
  String planExportError(String error) {
    return 'No se pudo exportar el plano: $error';
  }

  @override
  String get layoutComputing => 'Generando el modelo y el plano…';

  @override
  String get layoutUnavailable =>
      'No hay datos suficientes para generar el plano. Recorre las estancias barriendo las paredes.';

  @override
  String roomCount(int n) {
    String _temp0 = intl.Intl.pluralLogic(
      n,
      locale: localeName,
      other: '$n estancias',
      one: '1 estancia',
    );
    return '$_temp0';
  }

  @override
  String get furniture => 'Muebles';

  @override
  String get measuresTitle => 'Medidas';

  @override
  String get measuresTotalArea => 'Superficie útil total';

  @override
  String get measuresCeiling => 'Altura libre';

  @override
  String get measuresCeilingEstimated => 'Estimada: no se llegó a ver el techo';

  @override
  String get measuresOpenings => 'Huecos';

  @override
  String doorsWindows(int doors, int windows) {
    String _temp0 = intl.Intl.pluralLogic(
      doors,
      locale: localeName,
      other: '$doors puertas',
      one: '1 puerta',
    );
    String _temp1 = intl.Intl.pluralLogic(
      windows,
      locale: localeName,
      other: '$windows ventanas',
      one: '1 ventana',
    );
    return '$_temp0 · $_temp1';
  }

  @override
  String get driftCorrected => 'Deriva corregida';

  @override
  String driftCorrectedDetail(int loops, String cm, String deg) {
    String _temp0 = intl.Intl.pluralLogic(
      loops,
      locale: localeName,
      other: '$loops cierres de bucle',
      one: '1 cierre de bucle',
    );
    return '$_temp0 · corrección máxima $cm cm / $deg°';
  }

  @override
  String get measuresRooms => 'Estancias';

  @override
  String get measuresWalls => 'Paredes';

  @override
  String wallItem(int n, String length) {
    return 'Pared $n: $length';
  }

  @override
  String wallThickness(String value) {
    return 'grosor $value';
  }

  @override
  String doorWidth(String value) {
    return 'puerta de $value';
  }

  @override
  String windowWidth(String value) {
    return 'ventana de $value';
  }

  @override
  String get fileLayout =>
      'Modelo estructurado (JSON: paredes, huecos, muebles, estancias)';

  @override
  String get fileFloorPlan => 'Plano de planta (SVG)';

  @override
  String get summaryArea => 'Superficie útil';

  @override
  String get summaryRooms => 'Estancias';

  @override
  String get viewResult => 'Ver modelo y plano';

  @override
  String get autoTour => 'Recorrido automático';

  @override
  String get autoTourDesc =>
      'El escáner virtual recorre solo todo el piso (unos 4 minutos). Útil para probar el modelo y el plano sin moverte.';
}
