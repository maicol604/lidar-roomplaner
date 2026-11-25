import 'dart:async';

import 'package:flutter/foundation.dart';
import 'package:flutter/widgets.dart';
import 'package:flutter_localizations/flutter_localizations.dart';
import 'package:intl/intl.dart' as intl;

import 'app_localizations_en.dart';
import 'app_localizations_es.dart';

// ignore_for_file: type=lint

/// Callers can lookup localized strings with an instance of AppLocalizations
/// returned by `AppLocalizations.of(context)`.
///
/// Applications need to include `AppLocalizations.delegate()` in their app's
/// `localizationDelegates` list, and the locales they support in the app's
/// `supportedLocales` list. For example:
///
/// ```dart
/// import 'l10n/app_localizations.dart';
///
/// return MaterialApp(
///   localizationsDelegates: AppLocalizations.localizationsDelegates,
///   supportedLocales: AppLocalizations.supportedLocales,
///   home: MyApplicationHome(),
/// );
/// ```
///
/// ## Update pubspec.yaml
///
/// Please make sure to update your pubspec.yaml to include the following
/// packages:
///
/// ```yaml
/// dependencies:
///   # Internationalization support.
///   flutter_localizations:
///     sdk: flutter
///   intl: any # Use the pinned version from flutter_localizations
///
///   # Rest of dependencies
/// ```
///
/// ## iOS Applications
///
/// iOS applications define key application metadata, including supported
/// locales, in an Info.plist file that is built into the application bundle.
/// To configure the locales supported by your app, you’ll need to edit this
/// file.
///
/// First, open your project’s ios/Runner.xcworkspace Xcode workspace file.
/// Then, in the Project Navigator, open the Info.plist file under the Runner
/// project’s Runner folder.
///
/// Next, select the Information Property List item, select Add Item from the
/// Editor menu, then select Localizations from the pop-up menu.
///
/// Select and expand the newly-created Localizations item then, for each
/// locale your application supports, add a new item and select the locale
/// you wish to add from the pop-up menu in the Value field. This list should
/// be consistent with the languages listed in the AppLocalizations.supportedLocales
/// property.
abstract class AppLocalizations {
  AppLocalizations(String locale)
    : localeName = intl.Intl.canonicalizedLocale(locale.toString());

  final String localeName;

  static AppLocalizations of(BuildContext context) {
    return Localizations.of<AppLocalizations>(context, AppLocalizations)!;
  }

  static const LocalizationsDelegate<AppLocalizations> delegate =
      _AppLocalizationsDelegate();

  /// A list of this localizations delegate along with the default localizations
  /// delegates.
  ///
  /// Returns a list of localizations delegates containing this delegate along with
  /// GlobalMaterialLocalizations.delegate, GlobalCupertinoLocalizations.delegate,
  /// and GlobalWidgetsLocalizations.delegate.
  ///
  /// Additional delegates can be added by appending to this list in
  /// MaterialApp. This list does not have to be used at all if a custom list
  /// of delegates is preferred or required.
  static const List<LocalizationsDelegate<dynamic>> localizationsDelegates =
      <LocalizationsDelegate<dynamic>>[
        delegate,
        GlobalMaterialLocalizations.delegate,
        GlobalCupertinoLocalizations.delegate,
        GlobalWidgetsLocalizations.delegate,
      ];

  /// A list of this localizations delegate's supported locales.
  static const List<Locale> supportedLocales = <Locale>[
    Locale('en'),
    Locale('es'),
  ];

  /// No description provided for @appTitle.
  ///
  /// In es, this message translates to:
  /// **'LiDAR Scanner'**
  String get appTitle;

  /// No description provided for @homeTitle.
  ///
  /// In es, this message translates to:
  /// **'Escaneos'**
  String get homeTitle;

  /// No description provided for @newScan.
  ///
  /// In es, this message translates to:
  /// **'Nuevo escaneo'**
  String get newScan;

  /// No description provided for @noSessions.
  ///
  /// In es, this message translates to:
  /// **'Aún no hay escaneos. Pulsa «Nuevo escaneo» para empezar.'**
  String get noSessions;

  /// No description provided for @settings.
  ///
  /// In es, this message translates to:
  /// **'Ajustes'**
  String get settings;

  /// No description provided for @diagnostics.
  ///
  /// In es, this message translates to:
  /// **'Diagnóstico LD06'**
  String get diagnostics;

  /// No description provided for @sessionName.
  ///
  /// In es, this message translates to:
  /// **'Nombre'**
  String get sessionName;

  /// No description provided for @defaultSessionName.
  ///
  /// In es, this message translates to:
  /// **'Escaneo {date}'**
  String defaultSessionName(String date);

  /// No description provided for @source.
  ///
  /// In es, this message translates to:
  /// **'Fuente de datos'**
  String get source;

  /// No description provided for @sourceMock.
  ///
  /// In es, this message translates to:
  /// **'Simulador (mock)'**
  String get sourceMock;

  /// No description provided for @sourceMockDesc.
  ///
  /// In es, this message translates to:
  /// **'Recorre una vivienda sintética moviendo el móvil de verdad (ARCore o sensores); LD06, ESP32 y odometría simulados'**
  String get sourceMockDesc;

  /// No description provided for @sourceEsp32.
  ///
  /// In es, this message translates to:
  /// **'ESP32 + LD06 (WiFi UDP)'**
  String get sourceEsp32;

  /// No description provided for @sourceEsp32Desc.
  ///
  /// In es, this message translates to:
  /// **'Hardware real. Hasta la fase 3 (ARCore) se usa una pose estática.'**
  String get sourceEsp32Desc;

  /// No description provided for @transport.
  ///
  /// In es, this message translates to:
  /// **'Transporte del simulador'**
  String get transport;

  /// No description provided for @transportInProcess.
  ///
  /// In es, this message translates to:
  /// **'En proceso'**
  String get transportInProcess;

  /// No description provided for @transportLoopback.
  ///
  /// In es, this message translates to:
  /// **'UDP loopback (prueba la ruta de red)'**
  String get transportLoopback;

  /// No description provided for @mode.
  ///
  /// In es, this message translates to:
  /// **'Modo de escaneo'**
  String get mode;

  /// No description provided for @modeContinuous.
  ///
  /// In es, this message translates to:
  /// **'Continuo'**
  String get modeContinuous;

  /// No description provided for @modeContinuousDesc.
  ///
  /// In es, this message translates to:
  /// **'Caminas barriendo con el plano vertical; la odometría sigue tu posición.'**
  String get modeContinuousDesc;

  /// No description provided for @modeStation.
  ///
  /// In es, this message translates to:
  /// **'Por estaciones'**
  String get modeStation;

  /// No description provided for @modeStationDesc.
  ///
  /// In es, this message translates to:
  /// **'Giras sobre ti mismo en cada punto; se usa solo la orientación.'**
  String get modeStationDesc;

  /// No description provided for @record.
  ///
  /// In es, this message translates to:
  /// **'Grabar sesión (MCAP y exportables)'**
  String get record;

  /// No description provided for @startScan.
  ///
  /// In es, this message translates to:
  /// **'Empezar'**
  String get startScan;

  /// No description provided for @startFailed.
  ///
  /// In es, this message translates to:
  /// **'No se pudo iniciar el escáner (¿puerto UDP ocupado?)'**
  String get startFailed;

  /// No description provided for @stop.
  ///
  /// In es, this message translates to:
  /// **'Detener'**
  String get stop;

  /// No description provided for @pause.
  ///
  /// In es, this message translates to:
  /// **'Pausar'**
  String get pause;

  /// No description provided for @resume.
  ///
  /// In es, this message translates to:
  /// **'Reanudar'**
  String get resume;

  /// No description provided for @stopConfirmTitle.
  ///
  /// In es, this message translates to:
  /// **'¿Terminar el escaneo?'**
  String get stopConfirmTitle;

  /// No description provided for @stopConfirmBody.
  ///
  /// In es, this message translates to:
  /// **'Se guardará la sesión y se generarán los archivos de exportación.'**
  String get stopConfirmBody;

  /// No description provided for @cancel.
  ///
  /// In es, this message translates to:
  /// **'Cancelar'**
  String get cancel;

  /// No description provided for @finish.
  ///
  /// In es, this message translates to:
  /// **'Terminar'**
  String get finish;

  /// No description provided for @exporting.
  ///
  /// In es, this message translates to:
  /// **'Guardando y exportando…'**
  String get exporting;

  /// No description provided for @cameraOrbit.
  ///
  /// In es, this message translates to:
  /// **'Orbital'**
  String get cameraOrbit;

  /// No description provided for @cameraFollow.
  ///
  /// In es, this message translates to:
  /// **'Seguir'**
  String get cameraFollow;

  /// No description provided for @cameraFirstPerson.
  ///
  /// In es, this message translates to:
  /// **'1ª persona'**
  String get cameraFirstPerson;

  /// No description provided for @cameraTop.
  ///
  /// In es, this message translates to:
  /// **'Planta'**
  String get cameraTop;

  /// No description provided for @colorHeight.
  ///
  /// In es, this message translates to:
  /// **'Altura'**
  String get colorHeight;

  /// No description provided for @colorIntensity.
  ///
  /// In es, this message translates to:
  /// **'Intensidad'**
  String get colorIntensity;

  /// No description provided for @colorAge.
  ///
  /// In es, this message translates to:
  /// **'Antigüedad'**
  String get colorAge;

  /// No description provided for @hudPoints.
  ///
  /// In es, this message translates to:
  /// **'Puntos'**
  String get hudPoints;

  /// No description provided for @hudRate.
  ///
  /// In es, this message translates to:
  /// **'Puntos/s'**
  String get hudRate;

  /// No description provided for @hudTracking.
  ///
  /// In es, this message translates to:
  /// **'Odometría'**
  String get hudTracking;

  /// No description provided for @hudLoss.
  ///
  /// In es, this message translates to:
  /// **'Pérdidas'**
  String get hudLoss;

  /// No description provided for @hudLatency.
  ///
  /// In es, this message translates to:
  /// **'Latencia'**
  String get hudLatency;

  /// No description provided for @hudRpm.
  ///
  /// In es, this message translates to:
  /// **'RPM'**
  String get hudRpm;

  /// No description provided for @hudDuration.
  ///
  /// In es, this message translates to:
  /// **'Duración'**
  String get hudDuration;

  /// No description provided for @hudDistance.
  ///
  /// In es, this message translates to:
  /// **'Distancia'**
  String get hudDistance;

  /// No description provided for @hudBattery.
  ///
  /// In es, this message translates to:
  /// **'Batería'**
  String get hudBattery;

  /// No description provided for @hudTemp.
  ///
  /// In es, this message translates to:
  /// **'Temperatura'**
  String get hudTemp;

  /// No description provided for @trackingTracking.
  ///
  /// In es, this message translates to:
  /// **'OK'**
  String get trackingTracking;

  /// No description provided for @trackingLimited.
  ///
  /// In es, this message translates to:
  /// **'Limitada'**
  String get trackingLimited;

  /// No description provided for @trackingLost.
  ///
  /// In es, this message translates to:
  /// **'Perdida'**
  String get trackingLost;

  /// No description provided for @trackingInit.
  ///
  /// In es, this message translates to:
  /// **'Iniciando'**
  String get trackingInit;

  /// No description provided for @trackingStatic.
  ///
  /// In es, this message translates to:
  /// **'Estática'**
  String get trackingStatic;

  /// No description provided for @alertSpeed.
  ///
  /// In es, this message translates to:
  /// **'Muévete más despacio · {gap} cm entre perfiles'**
  String alertSpeed(String gap);

  /// No description provided for @alertTilt.
  ///
  /// In es, this message translates to:
  /// **'Inclinación excesiva ({deg}°): mantén el plano de escaneo vertical'**
  String alertTilt(String deg);

  /// No description provided for @alertTracking.
  ///
  /// In es, this message translates to:
  /// **'Odometría perdida: apunta la cámara a zonas con textura y luz'**
  String get alertTracking;

  /// No description provided for @alertNoData.
  ///
  /// In es, this message translates to:
  /// **'Sin datos del LiDAR'**
  String get alertNoData;

  /// No description provided for @alertStationDisplaced.
  ///
  /// In es, this message translates to:
  /// **'Te estás desplazando: en modo estación gira sin moverte'**
  String get alertStationDisplaced;

  /// No description provided for @alertThermal.
  ///
  /// In es, this message translates to:
  /// **'El móvil se está calentando'**
  String get alertThermal;

  /// No description provided for @stationCapturing.
  ///
  /// In es, this message translates to:
  /// **'Estación {n} · capturando'**
  String stationCapturing(int n);

  /// No description provided for @stationMoving.
  ///
  /// In es, this message translates to:
  /// **'Desplazándote a la siguiente estación'**
  String get stationMoving;

  /// No description provided for @stationFix.
  ///
  /// In es, this message translates to:
  /// **'Fijar estación aquí'**
  String get stationFix;

  /// No description provided for @stationMove.
  ///
  /// In es, this message translates to:
  /// **'Ir a otra estación'**
  String get stationMove;

  /// No description provided for @hideHud.
  ///
  /// In es, this message translates to:
  /// **'Ocultar datos'**
  String get hideHud;

  /// No description provided for @showHud.
  ///
  /// In es, this message translates to:
  /// **'Mostrar datos'**
  String get showHud;

  /// No description provided for @paused.
  ///
  /// In es, this message translates to:
  /// **'En pausa'**
  String get paused;

  /// No description provided for @alertMotionInit.
  ///
  /// In es, this message translates to:
  /// **'Iniciando seguimiento: mueve el móvil despacio apuntando a zonas con textura'**
  String get alertMotionInit;

  /// No description provided for @alertInsideWall.
  ///
  /// In es, this message translates to:
  /// **'Estás atravesando una pared o un mueble del piso virtual'**
  String get alertInsideWall;

  /// No description provided for @alertOutside.
  ///
  /// In es, this message translates to:
  /// **'Estás fuera del piso virtual: vuelve hacia el punto de inicio'**
  String get alertOutside;

  /// No description provided for @motionFailLight.
  ///
  /// In es, this message translates to:
  /// **'poca luz'**
  String get motionFailLight;

  /// No description provided for @motionFailMotion.
  ///
  /// In es, this message translates to:
  /// **'movimiento demasiado rápido'**
  String get motionFailMotion;

  /// No description provided for @motionFailFeatures.
  ///
  /// In es, this message translates to:
  /// **'poca textura a la vista'**
  String get motionFailFeatures;

  /// No description provided for @motionFailCamera.
  ///
  /// In es, this message translates to:
  /// **'cámara no disponible'**
  String get motionFailCamera;

  /// No description provided for @motionArcore.
  ///
  /// In es, this message translates to:
  /// **'ARCore'**
  String get motionArcore;

  /// No description provided for @motionImu.
  ///
  /// In es, this message translates to:
  /// **'Sensores'**
  String get motionImu;

  /// No description provided for @motionArcoreHint.
  ///
  /// In es, this message translates to:
  /// **'Tu movimiento real se sigue con ARCore (cámara + IMU)'**
  String get motionArcoreHint;

  /// No description provided for @motionImuHint.
  ///
  /// In es, this message translates to:
  /// **'Tu movimiento real se sigue con los sensores (giroscopio + detección de pasos)'**
  String get motionImuHint;

  /// No description provided for @motionFallbackCamera.
  ///
  /// In es, this message translates to:
  /// **'Sin permiso de cámara: se usan los sensores del móvil (giroscopio y pasos)'**
  String get motionFallbackCamera;

  /// No description provided for @motionFallbackInstall.
  ///
  /// In es, this message translates to:
  /// **'Instala «Servicios de Google Play para RA» para usar ARCore; mientras tanto se usan los sensores'**
  String get motionFallbackInstall;

  /// No description provided for @motionFallbackUnsupported.
  ///
  /// In es, this message translates to:
  /// **'Este móvil no es compatible con ARCore: se usan los sensores (giroscopio y pasos)'**
  String get motionFallbackUnsupported;

  /// No description provided for @motionFallbackError.
  ///
  /// In es, this message translates to:
  /// **'No se pudo iniciar ARCore: se usan los sensores (giroscopio y pasos)'**
  String get motionFallbackError;

  /// No description provided for @motionNone.
  ///
  /// In es, this message translates to:
  /// **'Este móvil no tiene sensores de orientación: el simulador no puede seguir tu movimiento'**
  String get motionNone;

  /// No description provided for @simulatorWalkHint.
  ///
  /// In es, this message translates to:
  /// **'Camina con el móvil en vertical: el escáner virtual sigue tu movimiento real por el piso sintético'**
  String get simulatorWalkHint;

  /// No description provided for @resetView.
  ///
  /// In es, this message translates to:
  /// **'Recentrar vista'**
  String get resetView;

  /// No description provided for @colorMode.
  ///
  /// In es, this message translates to:
  /// **'Color'**
  String get colorMode;

  /// No description provided for @cameraMode.
  ///
  /// In es, this message translates to:
  /// **'Cámara'**
  String get cameraMode;

  /// No description provided for @sessionDetail.
  ///
  /// In es, this message translates to:
  /// **'Sesión'**
  String get sessionDetail;

  /// No description provided for @share.
  ///
  /// In es, this message translates to:
  /// **'Compartir'**
  String get share;

  /// No description provided for @delete.
  ///
  /// In es, this message translates to:
  /// **'Eliminar'**
  String get delete;

  /// No description provided for @deleteConfirm.
  ///
  /// In es, this message translates to:
  /// **'¿Eliminar esta sesión y todos sus archivos?'**
  String get deleteConfirm;

  /// No description provided for @files.
  ///
  /// In es, this message translates to:
  /// **'Archivos'**
  String get files;

  /// No description provided for @fileMcap.
  ///
  /// In es, this message translates to:
  /// **'Sesión en bruto (MCAP, compatible ROS 2)'**
  String get fileMcap;

  /// No description provided for @filePly.
  ///
  /// In es, this message translates to:
  /// **'Nube de preview (PLY)'**
  String get filePly;

  /// No description provided for @filePcd.
  ///
  /// In es, this message translates to:
  /// **'Nube de preview (PCD)'**
  String get filePcd;

  /// No description provided for @fileCsv.
  ///
  /// In es, this message translates to:
  /// **'Trayectoria (CSV)'**
  String get fileCsv;

  /// No description provided for @fileJson.
  ///
  /// In es, this message translates to:
  /// **'Trayectoria (JSON)'**
  String get fileJson;

  /// No description provided for @summaryDuration.
  ///
  /// In es, this message translates to:
  /// **'Duración'**
  String get summaryDuration;

  /// No description provided for @summaryPoints.
  ///
  /// In es, this message translates to:
  /// **'Puntos'**
  String get summaryPoints;

  /// No description provided for @summaryDistance.
  ///
  /// In es, this message translates to:
  /// **'Distancia'**
  String get summaryDistance;

  /// No description provided for @summaryCorrections.
  ///
  /// In es, this message translates to:
  /// **'Correcciones del mapa'**
  String get summaryCorrections;

  /// No description provided for @summaryLoss.
  ///
  /// In es, this message translates to:
  /// **'Datagramas perdidos'**
  String get summaryLoss;

  /// No description provided for @summarySource.
  ///
  /// In es, this message translates to:
  /// **'Fuente'**
  String get summarySource;

  /// No description provided for @summaryMode.
  ///
  /// In es, this message translates to:
  /// **'Modo'**
  String get summaryMode;

  /// No description provided for @summaryVoxel.
  ///
  /// In es, this message translates to:
  /// **'Vóxel final'**
  String get summaryVoxel;

  /// No description provided for @incompleteSession.
  ///
  /// In es, this message translates to:
  /// **'Sesión incompleta'**
  String get incompleteSession;

  /// No description provided for @shareNothing.
  ///
  /// In es, this message translates to:
  /// **'Selecciona al menos un archivo'**
  String get shareNothing;

  /// No description provided for @settingsAppearance.
  ///
  /// In es, this message translates to:
  /// **'Apariencia'**
  String get settingsAppearance;

  /// No description provided for @theme.
  ///
  /// In es, this message translates to:
  /// **'Tema'**
  String get theme;

  /// No description provided for @themeSystem.
  ///
  /// In es, this message translates to:
  /// **'Sistema'**
  String get themeSystem;

  /// No description provided for @themeLight.
  ///
  /// In es, this message translates to:
  /// **'Claro'**
  String get themeLight;

  /// No description provided for @themeDark.
  ///
  /// In es, this message translates to:
  /// **'Oscuro'**
  String get themeDark;

  /// No description provided for @language.
  ///
  /// In es, this message translates to:
  /// **'Idioma'**
  String get language;

  /// No description provided for @languageSystem.
  ///
  /// In es, this message translates to:
  /// **'Sistema'**
  String get languageSystem;

  /// No description provided for @settingsViewer.
  ///
  /// In es, this message translates to:
  /// **'Visor 3D'**
  String get settingsViewer;

  /// No description provided for @pointSize.
  ///
  /// In es, this message translates to:
  /// **'Tamaño de punto'**
  String get pointSize;

  /// No description provided for @showTrajectory.
  ///
  /// In es, this message translates to:
  /// **'Mostrar trayectoria'**
  String get showTrajectory;

  /// No description provided for @showGrid.
  ///
  /// In es, this message translates to:
  /// **'Mostrar rejilla del suelo'**
  String get showGrid;

  /// No description provided for @ageWindow.
  ///
  /// In es, this message translates to:
  /// **'Ventana de antigüedad (s)'**
  String get ageWindow;

  /// No description provided for @targetFps.
  ///
  /// In es, this message translates to:
  /// **'FPS objetivo'**
  String get targetFps;

  /// No description provided for @settingsHud.
  ///
  /// In es, this message translates to:
  /// **'Datos en pantalla'**
  String get settingsHud;

  /// No description provided for @hudVisibleByDefault.
  ///
  /// In es, this message translates to:
  /// **'Mostrar datos al empezar'**
  String get hudVisibleByDefault;

  /// No description provided for @settingsMapping.
  ///
  /// In es, this message translates to:
  /// **'Mapeo'**
  String get settingsMapping;

  /// No description provided for @voxel.
  ///
  /// In es, this message translates to:
  /// **'Tamaño de vóxel (cm)'**
  String get voxel;

  /// No description provided for @pointBudget.
  ///
  /// In es, this message translates to:
  /// **'Límite de puntos'**
  String get pointBudget;

  /// No description provided for @budgetAuto.
  ///
  /// In es, this message translates to:
  /// **'Automático ({n})'**
  String budgetAuto(String n);

  /// No description provided for @minRange.
  ///
  /// In es, this message translates to:
  /// **'Distancia mínima (m)'**
  String get minRange;

  /// No description provided for @maxRange.
  ///
  /// In es, this message translates to:
  /// **'Distancia máxima (m)'**
  String get maxRange;

  /// No description provided for @minIntensity.
  ///
  /// In es, this message translates to:
  /// **'Intensidad mínima'**
  String get minIntensity;

  /// No description provided for @settingsAlerts.
  ///
  /// In es, this message translates to:
  /// **'Alertas de calidad'**
  String get settingsAlerts;

  /// No description provided for @tiltThreshold.
  ///
  /// In es, this message translates to:
  /// **'Inclinación máxima (°)'**
  String get tiltThreshold;

  /// No description provided for @gapThreshold.
  ///
  /// In es, this message translates to:
  /// **'Separación máxima entre perfiles (cm)'**
  String get gapThreshold;

  /// No description provided for @settingsConnection.
  ///
  /// In es, this message translates to:
  /// **'Conexión ESP32'**
  String get settingsConnection;

  /// No description provided for @espHost.
  ///
  /// In es, this message translates to:
  /// **'IP del ESP32'**
  String get espHost;

  /// No description provided for @espPort.
  ///
  /// In es, this message translates to:
  /// **'Puerto UDP del ESP32'**
  String get espPort;

  /// No description provided for @phonePort.
  ///
  /// In es, this message translates to:
  /// **'Puerto UDP del móvil'**
  String get phonePort;

  /// No description provided for @settingsCalibration.
  ///
  /// In es, this message translates to:
  /// **'Calibración LiDAR ↔ móvil'**
  String get settingsCalibration;

  /// No description provided for @calibrationHint.
  ///
  /// In es, this message translates to:
  /// **'Por defecto: LD06 sobre el móvil con el eje de giro hacia delante (plano vertical transversal). Los ángulos se aplican sobre ese montaje.'**
  String get calibrationHint;

  /// No description provided for @extTx.
  ///
  /// In es, this message translates to:
  /// **'Desplazamiento X (cm)'**
  String get extTx;

  /// No description provided for @extTy.
  ///
  /// In es, this message translates to:
  /// **'Desplazamiento Y (cm)'**
  String get extTy;

  /// No description provided for @extTz.
  ///
  /// In es, this message translates to:
  /// **'Desplazamiento Z (cm)'**
  String get extTz;

  /// No description provided for @extRoll.
  ///
  /// In es, this message translates to:
  /// **'Alabeo (°)'**
  String get extRoll;

  /// No description provided for @extPitch.
  ///
  /// In es, this message translates to:
  /// **'Cabeceo (°)'**
  String get extPitch;

  /// No description provided for @extYaw.
  ///
  /// In es, this message translates to:
  /// **'Guiñada (°)'**
  String get extYaw;

  /// No description provided for @lidarLatency.
  ///
  /// In es, this message translates to:
  /// **'Latencia del LD06 (ms)'**
  String get lidarLatency;

  /// No description provided for @settingsMock.
  ///
  /// In es, this message translates to:
  /// **'Simulador'**
  String get settingsMock;

  /// No description provided for @mockNoise.
  ///
  /// In es, this message translates to:
  /// **'Ruido de distancia (± mm)'**
  String get mockNoise;

  /// No description provided for @mockGlass.
  ///
  /// In es, this message translates to:
  /// **'Huecos y fantasmas en cristales y espejos'**
  String get mockGlass;

  /// No description provided for @mockIntensity.
  ///
  /// In es, this message translates to:
  /// **'Intensidad según material'**
  String get mockIntensity;

  /// No description provided for @mockDrift.
  ///
  /// In es, this message translates to:
  /// **'Deriva de odometría'**
  String get mockDrift;

  /// No description provided for @mockDriftPct.
  ///
  /// In es, this message translates to:
  /// **'Error de escala (%)'**
  String get mockDriftPct;

  /// No description provided for @mockDriftYaw.
  ///
  /// In es, this message translates to:
  /// **'Deriva de rumbo (°/m)'**
  String get mockDriftYaw;

  /// No description provided for @mockLoopClosure.
  ///
  /// In es, this message translates to:
  /// **'Correcciones tipo ARCore (cierre de bucle)'**
  String get mockLoopClosure;

  /// No description provided for @mockPacketLoss.
  ///
  /// In es, this message translates to:
  /// **'Pérdida de paquetes UDP'**
  String get mockPacketLoss;

  /// No description provided for @mockPacketLossPct.
  ///
  /// In es, this message translates to:
  /// **'Pérdida de paquetes (%)'**
  String get mockPacketLossPct;

  /// No description provided for @mockTrackingLoss.
  ///
  /// In es, this message translates to:
  /// **'Pérdidas de odometría'**
  String get mockTrackingLoss;

  /// No description provided for @mockTrackingLossRate.
  ///
  /// In es, this message translates to:
  /// **'Pérdidas por minuto'**
  String get mockTrackingLossRate;

  /// No description provided for @resetDefaults.
  ///
  /// In es, this message translates to:
  /// **'Restaurar valores por defecto'**
  String get resetDefaults;

  /// No description provided for @diagWaiting.
  ///
  /// In es, this message translates to:
  /// **'Esperando datos del LD06…'**
  String get diagWaiting;

  /// No description provided for @diagStart.
  ///
  /// In es, this message translates to:
  /// **'Iniciar'**
  String get diagStart;

  /// No description provided for @diagStop.
  ///
  /// In es, this message translates to:
  /// **'Parar'**
  String get diagStop;

  /// No description provided for @diagRunningSession.
  ///
  /// In es, this message translates to:
  /// **'Mostrando la sesión en curso'**
  String get diagRunningSession;

  /// No description provided for @diagDatagrams.
  ///
  /// In es, this message translates to:
  /// **'Datagramas/s'**
  String get diagDatagrams;

  /// No description provided for @diagCrc.
  ///
  /// In es, this message translates to:
  /// **'Errores CRC'**
  String get diagCrc;

  /// No description provided for @diagRtt.
  ///
  /// In es, this message translates to:
  /// **'RTT mínimo'**
  String get diagRtt;

  /// No description provided for @diagDrift.
  ///
  /// In es, this message translates to:
  /// **'Deriva de reloj'**
  String get diagDrift;

  /// No description provided for @diagSynced.
  ///
  /// In es, this message translates to:
  /// **'Reloj sincronizado'**
  String get diagSynced;

  /// No description provided for @diagPeer.
  ///
  /// In es, this message translates to:
  /// **'ESP32'**
  String get diagPeer;

  /// No description provided for @diagPoints.
  ///
  /// In es, this message translates to:
  /// **'Puntos en la vuelta'**
  String get diagPoints;

  /// No description provided for @yes.
  ///
  /// In es, this message translates to:
  /// **'Sí'**
  String get yes;

  /// No description provided for @no.
  ///
  /// In es, this message translates to:
  /// **'No'**
  String get no;

  /// No description provided for @deviceInfoLine.
  ///
  /// In es, this message translates to:
  /// **'{model} · {ram} MB RAM'**
  String deviceInfoLine(String model, String ram);

  /// No description provided for @lowEndNotice.
  ///
  /// In es, this message translates to:
  /// **'Gama baja detectada: límite de puntos y FPS reducidos'**
  String get lowEndNotice;

  /// No description provided for @advanced.
  ///
  /// In es, this message translates to:
  /// **'Opciones avanzadas'**
  String get advanced;

  /// No description provided for @stationModeHint.
  ///
  /// In es, this message translates to:
  /// **'Gira despacio sobre ti mismo. Después pulsa «Ir a otra estación», camina y fija la nueva estación.'**
  String get stationModeHint;

  /// No description provided for @loopClosures.
  ///
  /// In es, this message translates to:
  /// **'Cierres de bucle'**
  String get loopClosures;

  /// No description provided for @minimapHint.
  ///
  /// In es, this message translates to:
  /// **'Naranja: recorrido · Verde: suelo visto · Blanco: paredes y obstáculos'**
  String get minimapHint;

  /// No description provided for @viewCloud.
  ///
  /// In es, this message translates to:
  /// **'Ver nube 3D'**
  String get viewCloud;

  /// No description provided for @loadingCloud.
  ///
  /// In es, this message translates to:
  /// **'Cargando nube…'**
  String get loadingCloud;

  /// No description provided for @noCloud.
  ///
  /// In es, this message translates to:
  /// **'Esta sesión no tiene nube exportada (se interrumpió antes de terminar).'**
  String get noCloud;

  /// No description provided for @cloudLoadError.
  ///
  /// In es, this message translates to:
  /// **'No se pudo cargar la nube: {error}'**
  String cloudLoadError(String error);

  /// No description provided for @sessionInfo.
  ///
  /// In es, this message translates to:
  /// **'Información y exportación'**
  String get sessionInfo;

  /// No description provided for @cloudPoints.
  ///
  /// In es, this message translates to:
  /// **'{n} puntos'**
  String cloudPoints(String n);

  /// No description provided for @hideCeiling.
  ///
  /// In es, this message translates to:
  /// **'Ocultar techo'**
  String get hideCeiling;

  /// No description provided for @modelTitle.
  ///
  /// In es, this message translates to:
  /// **'Modelo 3D'**
  String get modelTitle;

  /// No description provided for @modelEmpty.
  ///
  /// In es, this message translates to:
  /// **'Aún no hay modelo. Recorre el espacio y aparecerá aquí.'**
  String get modelEmpty;

  /// No description provided for @modelEmptyLive.
  ///
  /// In es, this message translates to:
  /// **'Recorre el espacio: las paredes aparecerán aquí'**
  String get modelEmptyLive;

  /// No description provided for @modelSummary.
  ///
  /// In es, this message translates to:
  /// **'{walls, plural, =1{1 pared} other{{walls} paredes}}, {doors, plural, =1{1 puerta} other{{doors} puertas}}, {windows, plural, =1{1 ventana} other{{windows} ventanas}}, {objects, plural, =1{1 mueble} other{{objects} muebles}} · {area}'**
  String modelSummary(
    int walls,
    int doors,
    int windows,
    int objects,
    String area,
  );

  /// No description provided for @modelGestureHint.
  ///
  /// In es, this message translates to:
  /// **'Arrastra para girar, pellizca para ampliar y toca dos veces para recentrar'**
  String get modelGestureHint;

  /// No description provided for @modelLiveTitle.
  ///
  /// In es, this message translates to:
  /// **'Modelo en directo'**
  String get modelLiveTitle;

  /// No description provided for @modelLiveShort.
  ///
  /// In es, this message translates to:
  /// **'En directo'**
  String get modelLiveShort;

  /// No description provided for @expandModel.
  ///
  /// In es, this message translates to:
  /// **'Ampliar modelo 3D'**
  String get expandModel;

  /// No description provided for @collapseModel.
  ///
  /// In es, this message translates to:
  /// **'Reducir modelo 3D'**
  String get collapseModel;

  /// No description provided for @rotateLeft.
  ///
  /// In es, this message translates to:
  /// **'Girar a la izquierda'**
  String get rotateLeft;

  /// No description provided for @rotateRight.
  ///
  /// In es, this message translates to:
  /// **'Girar a la derecha'**
  String get rotateRight;

  /// No description provided for @tiltUp.
  ///
  /// In es, this message translates to:
  /// **'Ver más desde arriba'**
  String get tiltUp;

  /// No description provided for @tiltDown.
  ///
  /// In es, this message translates to:
  /// **'Ver más de lado'**
  String get tiltDown;

  /// No description provided for @zoomIn.
  ///
  /// In es, this message translates to:
  /// **'Acercar'**
  String get zoomIn;

  /// No description provided for @zoomOut.
  ///
  /// In es, this message translates to:
  /// **'Alejar'**
  String get zoomOut;

  /// No description provided for @topView.
  ///
  /// In es, this message translates to:
  /// **'Vista cenital'**
  String get topView;

  /// No description provided for @fitPlan.
  ///
  /// In es, this message translates to:
  /// **'Encajar el plano'**
  String get fitPlan;

  /// No description provided for @scaleBar.
  ///
  /// In es, this message translates to:
  /// **'Escala: {label}'**
  String scaleBar(String label);

  /// No description provided for @roomCorridor.
  ///
  /// In es, this message translates to:
  /// **'Pasillo'**
  String get roomCorridor;

  /// No description provided for @roomNumbered.
  ///
  /// In es, this message translates to:
  /// **'Estancia {n}'**
  String roomNumbered(int n);

  /// No description provided for @legendDoor.
  ///
  /// In es, this message translates to:
  /// **'Puerta'**
  String get legendDoor;

  /// No description provided for @legendWindow.
  ///
  /// In es, this message translates to:
  /// **'Ventana'**
  String get legendWindow;

  /// No description provided for @legendWall.
  ///
  /// In es, this message translates to:
  /// **'Pared'**
  String get legendWall;

  /// No description provided for @minimapSemantics.
  ///
  /// In es, this message translates to:
  /// **'Mapa de cobertura: recorrido, suelo visto y paredes'**
  String get minimapSemantics;

  /// No description provided for @expand.
  ///
  /// In es, this message translates to:
  /// **'Toca para ampliar'**
  String get expand;

  /// No description provided for @collapse.
  ///
  /// In es, this message translates to:
  /// **'Toca para reducir'**
  String get collapse;

  /// No description provided for @cameraPreview.
  ///
  /// In es, this message translates to:
  /// **'Vista de la cámara'**
  String get cameraPreview;

  /// No description provided for @closeScan.
  ///
  /// In es, this message translates to:
  /// **'Terminar escaneo'**
  String get closeScan;

  /// No description provided for @recording.
  ///
  /// In es, this message translates to:
  /// **'Grabando'**
  String get recording;

  /// No description provided for @cloudViewSemantics.
  ///
  /// In es, this message translates to:
  /// **'Vista 3D de la nube de puntos'**
  String get cloudViewSemantics;

  /// No description provided for @cloudViewHint.
  ///
  /// In es, this message translates to:
  /// **'Arrastra con un dedo para girar y con dos para desplazar y ampliar; toca dos veces para recentrar'**
  String get cloudViewHint;

  /// No description provided for @tabModel.
  ///
  /// In es, this message translates to:
  /// **'Modelo 3D'**
  String get tabModel;

  /// No description provided for @tabPlan.
  ///
  /// In es, this message translates to:
  /// **'Plano'**
  String get tabPlan;

  /// No description provided for @tabCloud.
  ///
  /// In es, this message translates to:
  /// **'Nube'**
  String get tabCloud;

  /// No description provided for @planSubtitle.
  ///
  /// In es, this message translates to:
  /// **'{date} · {area} útiles'**
  String planSubtitle(String date, String area);

  /// No description provided for @planDisclaimer.
  ///
  /// In es, this message translates to:
  /// **'Medidas aproximadas obtenidas con el escáner LiDAR. Verifícalas antes de usarlas en obra.'**
  String get planDisclaimer;

  /// No description provided for @planExport.
  ///
  /// In es, this message translates to:
  /// **'Exportar SVG'**
  String get planExport;

  /// No description provided for @planExportError.
  ///
  /// In es, this message translates to:
  /// **'No se pudo exportar el plano: {error}'**
  String planExportError(String error);

  /// No description provided for @layoutComputing.
  ///
  /// In es, this message translates to:
  /// **'Generando el modelo y el plano…'**
  String get layoutComputing;

  /// No description provided for @layoutUnavailable.
  ///
  /// In es, this message translates to:
  /// **'No hay datos suficientes para generar el plano. Recorre las estancias barriendo las paredes.'**
  String get layoutUnavailable;

  /// No description provided for @roomCount.
  ///
  /// In es, this message translates to:
  /// **'{n, plural, =1{1 estancia} other{{n} estancias}}'**
  String roomCount(int n);

  /// No description provided for @furniture.
  ///
  /// In es, this message translates to:
  /// **'Muebles'**
  String get furniture;

  /// No description provided for @measuresTitle.
  ///
  /// In es, this message translates to:
  /// **'Medidas'**
  String get measuresTitle;

  /// No description provided for @measuresTotalArea.
  ///
  /// In es, this message translates to:
  /// **'Superficie útil total'**
  String get measuresTotalArea;

  /// No description provided for @measuresCeiling.
  ///
  /// In es, this message translates to:
  /// **'Altura libre'**
  String get measuresCeiling;

  /// No description provided for @measuresCeilingEstimated.
  ///
  /// In es, this message translates to:
  /// **'Estimada: no se llegó a ver el techo'**
  String get measuresCeilingEstimated;

  /// No description provided for @measuresOpenings.
  ///
  /// In es, this message translates to:
  /// **'Huecos'**
  String get measuresOpenings;

  /// No description provided for @doorsWindows.
  ///
  /// In es, this message translates to:
  /// **'{doors, plural, =1{1 puerta} other{{doors} puertas}} · {windows, plural, =1{1 ventana} other{{windows} ventanas}}'**
  String doorsWindows(int doors, int windows);

  /// No description provided for @driftCorrected.
  ///
  /// In es, this message translates to:
  /// **'Deriva corregida'**
  String get driftCorrected;

  /// No description provided for @driftCorrectedDetail.
  ///
  /// In es, this message translates to:
  /// **'{loops, plural, =1{1 cierre de bucle} other{{loops} cierres de bucle}} · corrección máxima {cm} cm / {deg}°'**
  String driftCorrectedDetail(int loops, String cm, String deg);

  /// No description provided for @measuresRooms.
  ///
  /// In es, this message translates to:
  /// **'Estancias'**
  String get measuresRooms;

  /// No description provided for @measuresWalls.
  ///
  /// In es, this message translates to:
  /// **'Paredes'**
  String get measuresWalls;

  /// No description provided for @wallItem.
  ///
  /// In es, this message translates to:
  /// **'Pared {n}: {length}'**
  String wallItem(int n, String length);

  /// No description provided for @wallThickness.
  ///
  /// In es, this message translates to:
  /// **'grosor {value}'**
  String wallThickness(String value);

  /// No description provided for @doorWidth.
  ///
  /// In es, this message translates to:
  /// **'puerta de {value}'**
  String doorWidth(String value);

  /// No description provided for @windowWidth.
  ///
  /// In es, this message translates to:
  /// **'ventana de {value}'**
  String windowWidth(String value);

  /// No description provided for @fileLayout.
  ///
  /// In es, this message translates to:
  /// **'Modelo estructurado (JSON: paredes, huecos, muebles, estancias)'**
  String get fileLayout;

  /// No description provided for @fileFloorPlan.
  ///
  /// In es, this message translates to:
  /// **'Plano de planta (SVG)'**
  String get fileFloorPlan;

  /// No description provided for @summaryArea.
  ///
  /// In es, this message translates to:
  /// **'Superficie útil'**
  String get summaryArea;

  /// No description provided for @summaryRooms.
  ///
  /// In es, this message translates to:
  /// **'Estancias'**
  String get summaryRooms;

  /// No description provided for @viewResult.
  ///
  /// In es, this message translates to:
  /// **'Ver modelo y plano'**
  String get viewResult;

  /// No description provided for @autoTour.
  ///
  /// In es, this message translates to:
  /// **'Recorrido automático'**
  String get autoTour;

  /// No description provided for @autoTourDesc.
  ///
  /// In es, this message translates to:
  /// **'El escáner virtual recorre solo todo el piso (unos 4 minutos). Útil para probar el modelo y el plano sin moverte.'**
  String get autoTourDesc;
}

class _AppLocalizationsDelegate
    extends LocalizationsDelegate<AppLocalizations> {
  const _AppLocalizationsDelegate();

  @override
  Future<AppLocalizations> load(Locale locale) {
    return SynchronousFuture<AppLocalizations>(lookupAppLocalizations(locale));
  }

  @override
  bool isSupported(Locale locale) =>
      <String>['en', 'es'].contains(locale.languageCode);

  @override
  bool shouldReload(_AppLocalizationsDelegate old) => false;
}

AppLocalizations lookupAppLocalizations(Locale locale) {
  // Lookup logic when only language code is specified.
  switch (locale.languageCode) {
    case 'en':
      return AppLocalizationsEn();
    case 'es':
      return AppLocalizationsEs();
  }

  throw FlutterError(
    'AppLocalizations.delegate failed to load unsupported locale "$locale". This is likely '
    'an issue with the localizations generation tool. Please file an issue '
    'on GitHub with a reproducible sample app and the gen-l10n configuration '
    'that was used.',
  );
}
