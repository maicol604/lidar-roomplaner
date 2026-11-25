// ignore: unused_import
import 'package:intl/intl.dart' as intl;

import 'app_localizations.dart';

// ignore_for_file: type=lint

/// The translations for English (`en`).
class AppLocalizationsEn extends AppLocalizations {
  AppLocalizationsEn([String locale = 'en']) : super(locale);

  @override
  String get appTitle => 'LiDAR Scanner';

  @override
  String get homeTitle => 'Scans';

  @override
  String get newScan => 'New scan';

  @override
  String get noSessions => 'No scans yet. Tap “New scan” to get started.';

  @override
  String get settings => 'Settings';

  @override
  String get diagnostics => 'LD06 diagnostics';

  @override
  String get sessionName => 'Name';

  @override
  String defaultSessionName(String date) {
    return 'Scan $date';
  }

  @override
  String get source => 'Data source';

  @override
  String get sourceMock => 'Simulator (mock)';

  @override
  String get sourceMockDesc =>
      'Walk a synthetic apartment by really moving the phone (ARCore or sensors); simulated LD06, ESP32 and odometry';

  @override
  String get sourceEsp32 => 'ESP32 + LD06 (WiFi UDP)';

  @override
  String get sourceEsp32Desc =>
      'Real hardware. Until phase 3 (ARCore) a static pose is used.';

  @override
  String get transport => 'Simulator transport';

  @override
  String get transportInProcess => 'In-process';

  @override
  String get transportLoopback => 'UDP loopback (exercises the network path)';

  @override
  String get mode => 'Scan mode';

  @override
  String get modeContinuous => 'Continuous';

  @override
  String get modeContinuousDesc =>
      'Walk sweeping the vertical plane; odometry tracks your position.';

  @override
  String get modeStation => 'Stations';

  @override
  String get modeStationDesc =>
      'Turn around on the spot at each station; only orientation is used.';

  @override
  String get record => 'Record session (MCAP and exports)';

  @override
  String get startScan => 'Start';

  @override
  String get startFailed => 'Could not start the scanner (UDP port in use?)';

  @override
  String get stop => 'Stop';

  @override
  String get pause => 'Pause';

  @override
  String get resume => 'Resume';

  @override
  String get stopConfirmTitle => 'Finish the scan?';

  @override
  String get stopConfirmBody =>
      'The session will be saved and the export files generated.';

  @override
  String get cancel => 'Cancel';

  @override
  String get finish => 'Finish';

  @override
  String get exporting => 'Saving and exporting…';

  @override
  String get cameraOrbit => 'Orbit';

  @override
  String get cameraFollow => 'Follow';

  @override
  String get cameraFirstPerson => '1st person';

  @override
  String get cameraTop => 'Top';

  @override
  String get colorHeight => 'Height';

  @override
  String get colorIntensity => 'Intensity';

  @override
  String get colorAge => 'Age';

  @override
  String get hudPoints => 'Points';

  @override
  String get hudRate => 'Points/s';

  @override
  String get hudTracking => 'Odometry';

  @override
  String get hudLoss => 'Loss';

  @override
  String get hudLatency => 'Latency';

  @override
  String get hudRpm => 'RPM';

  @override
  String get hudDuration => 'Duration';

  @override
  String get hudDistance => 'Distance';

  @override
  String get hudBattery => 'Battery';

  @override
  String get hudTemp => 'Temperature';

  @override
  String get trackingTracking => 'OK';

  @override
  String get trackingLimited => 'Limited';

  @override
  String get trackingLost => 'Lost';

  @override
  String get trackingInit => 'Starting';

  @override
  String get trackingStatic => 'Static';

  @override
  String alertSpeed(String gap) {
    return 'Move slower · $gap cm between profiles';
  }

  @override
  String alertTilt(String deg) {
    return 'Excessive tilt ($deg°): keep the scan plane vertical';
  }

  @override
  String get alertTracking =>
      'Odometry lost: point the camera at textured, well-lit areas';

  @override
  String get alertNoData => 'No LiDAR data';

  @override
  String get alertStationDisplaced =>
      'You are moving: in station mode turn without walking';

  @override
  String get alertThermal => 'The phone is getting hot';

  @override
  String stationCapturing(int n) {
    return 'Station $n · capturing';
  }

  @override
  String get stationMoving => 'Moving to the next station';

  @override
  String get stationFix => 'Fix station here';

  @override
  String get stationMove => 'Go to another station';

  @override
  String get hideHud => 'Hide data';

  @override
  String get showHud => 'Show data';

  @override
  String get paused => 'Paused';

  @override
  String get alertMotionInit =>
      'Starting tracking: move the phone slowly, pointing at textured areas';

  @override
  String get alertInsideWall =>
      'You are walking through a wall or furniture of the virtual apartment';

  @override
  String get alertOutside =>
      'You are outside the virtual apartment: head back towards the starting point';

  @override
  String get motionFailLight => 'too dark';

  @override
  String get motionFailMotion => 'moving too fast';

  @override
  String get motionFailFeatures => 'not enough texture in view';

  @override
  String get motionFailCamera => 'camera unavailable';

  @override
  String get motionArcore => 'ARCore';

  @override
  String get motionImu => 'Sensors';

  @override
  String get motionArcoreHint =>
      'Your real motion is tracked with ARCore (camera + IMU)';

  @override
  String get motionImuHint =>
      'Your real motion is tracked with the phone sensors (gyroscope + step detection)';

  @override
  String get motionFallbackCamera =>
      'No camera permission: using the phone sensors (gyroscope and steps)';

  @override
  String get motionFallbackInstall =>
      'Install \"Google Play Services for AR\" to use ARCore; using the phone sensors meanwhile';

  @override
  String get motionFallbackUnsupported =>
      'This phone does not support ARCore: using the phone sensors (gyroscope and steps)';

  @override
  String get motionFallbackError =>
      'ARCore could not start: using the phone sensors (gyroscope and steps)';

  @override
  String get motionNone =>
      'This phone has no orientation sensors: the simulator cannot follow your motion';

  @override
  String get simulatorWalkHint =>
      'Walk holding the phone upright: the virtual scanner follows your real motion through the synthetic apartment';

  @override
  String get resetView => 'Recenter view';

  @override
  String get colorMode => 'Color';

  @override
  String get cameraMode => 'Camera';

  @override
  String get sessionDetail => 'Session';

  @override
  String get share => 'Share';

  @override
  String get delete => 'Delete';

  @override
  String get deleteConfirm => 'Delete this session and all its files?';

  @override
  String get files => 'Files';

  @override
  String get fileMcap => 'Raw session (MCAP, ROS 2 compatible)';

  @override
  String get filePly => 'Preview cloud (PLY)';

  @override
  String get filePcd => 'Preview cloud (PCD)';

  @override
  String get fileCsv => 'Trajectory (CSV)';

  @override
  String get fileJson => 'Trajectory (JSON)';

  @override
  String get summaryDuration => 'Duration';

  @override
  String get summaryPoints => 'Points';

  @override
  String get summaryDistance => 'Distance';

  @override
  String get summaryCorrections => 'Map corrections';

  @override
  String get summaryLoss => 'Lost datagrams';

  @override
  String get summarySource => 'Source';

  @override
  String get summaryMode => 'Mode';

  @override
  String get summaryVoxel => 'Final voxel';

  @override
  String get incompleteSession => 'Incomplete session';

  @override
  String get shareNothing => 'Select at least one file';

  @override
  String get settingsAppearance => 'Appearance';

  @override
  String get theme => 'Theme';

  @override
  String get themeSystem => 'System';

  @override
  String get themeLight => 'Light';

  @override
  String get themeDark => 'Dark';

  @override
  String get language => 'Language';

  @override
  String get languageSystem => 'System';

  @override
  String get settingsViewer => '3D viewer';

  @override
  String get pointSize => 'Point size';

  @override
  String get showTrajectory => 'Show trajectory';

  @override
  String get showGrid => 'Show floor grid';

  @override
  String get ageWindow => 'Age window (s)';

  @override
  String get targetFps => 'Target FPS';

  @override
  String get settingsHud => 'On-screen data';

  @override
  String get hudVisibleByDefault => 'Show data when starting';

  @override
  String get settingsMapping => 'Mapping';

  @override
  String get voxel => 'Voxel size (cm)';

  @override
  String get pointBudget => 'Point budget';

  @override
  String budgetAuto(String n) {
    return 'Automatic ($n)';
  }

  @override
  String get minRange => 'Minimum range (m)';

  @override
  String get maxRange => 'Maximum range (m)';

  @override
  String get minIntensity => 'Minimum intensity';

  @override
  String get settingsAlerts => 'Quality alerts';

  @override
  String get tiltThreshold => 'Maximum tilt (°)';

  @override
  String get gapThreshold => 'Maximum gap between profiles (cm)';

  @override
  String get settingsConnection => 'ESP32 connection';

  @override
  String get espHost => 'ESP32 IP';

  @override
  String get espPort => 'ESP32 UDP port';

  @override
  String get phonePort => 'Phone UDP port';

  @override
  String get settingsCalibration => 'LiDAR ↔ phone calibration';

  @override
  String get calibrationHint =>
      'Default: LD06 on top of the phone with its spin axis pointing forward (vertical transversal plane). Angles are applied on top of that mount.';

  @override
  String get extTx => 'Offset X (cm)';

  @override
  String get extTy => 'Offset Y (cm)';

  @override
  String get extTz => 'Offset Z (cm)';

  @override
  String get extRoll => 'Roll (°)';

  @override
  String get extPitch => 'Pitch (°)';

  @override
  String get extYaw => 'Yaw (°)';

  @override
  String get lidarLatency => 'LD06 latency (ms)';

  @override
  String get settingsMock => 'Simulator';

  @override
  String get mockNoise => 'Range noise (± mm)';

  @override
  String get mockGlass => 'Gaps and ghosts on glass and mirrors';

  @override
  String get mockIntensity => 'Material-dependent intensity';

  @override
  String get mockDrift => 'Odometry drift';

  @override
  String get mockDriftPct => 'Scale error (%)';

  @override
  String get mockDriftYaw => 'Heading drift (°/m)';

  @override
  String get mockLoopClosure => 'ARCore-like corrections (loop closure)';

  @override
  String get mockPacketLoss => 'UDP packet loss';

  @override
  String get mockPacketLossPct => 'Packet loss (%)';

  @override
  String get mockTrackingLoss => 'Odometry dropouts';

  @override
  String get mockTrackingLossRate => 'Dropouts per minute';

  @override
  String get resetDefaults => 'Restore defaults';

  @override
  String get diagWaiting => 'Waiting for LD06 data…';

  @override
  String get diagStart => 'Start';

  @override
  String get diagStop => 'Stop';

  @override
  String get diagRunningSession => 'Showing the running session';

  @override
  String get diagDatagrams => 'Datagrams/s';

  @override
  String get diagCrc => 'CRC errors';

  @override
  String get diagRtt => 'Min RTT';

  @override
  String get diagDrift => 'Clock drift';

  @override
  String get diagSynced => 'Clock synced';

  @override
  String get diagPeer => 'ESP32';

  @override
  String get diagPoints => 'Points in revolution';

  @override
  String get yes => 'Yes';

  @override
  String get no => 'No';

  @override
  String deviceInfoLine(String model, String ram) {
    return '$model · $ram MB RAM';
  }

  @override
  String get lowEndNotice =>
      'Low-end device detected: reduced point budget and FPS';

  @override
  String get advanced => 'Advanced options';

  @override
  String get stationModeHint =>
      'Turn slowly on the spot. Then tap “Go to another station”, walk and fix the new station.';

  @override
  String get loopClosures => 'Loop closures';

  @override
  String get minimapHint =>
      'Orange: path · Green: floor seen · White: walls and obstacles';

  @override
  String get viewCloud => 'View 3D cloud';

  @override
  String get loadingCloud => 'Loading cloud…';

  @override
  String get noCloud =>
      'This session has no exported cloud (it was interrupted before finishing).';

  @override
  String cloudLoadError(String error) {
    return 'Could not load the cloud: $error';
  }

  @override
  String get sessionInfo => 'Info and export';

  @override
  String cloudPoints(String n) {
    return '$n points';
  }

  @override
  String get hideCeiling => 'Hide ceiling';

  @override
  String get modelTitle => '3D model';

  @override
  String get modelEmpty =>
      'No model yet. Walk through the space and it will appear here.';

  @override
  String get modelEmptyLive => 'Walk through the space: walls will appear here';

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
      other: '$walls walls',
      one: '1 wall',
    );
    String _temp1 = intl.Intl.pluralLogic(
      doors,
      locale: localeName,
      other: '$doors doors',
      one: '1 door',
    );
    String _temp2 = intl.Intl.pluralLogic(
      windows,
      locale: localeName,
      other: '$windows windows',
      one: '1 window',
    );
    String _temp3 = intl.Intl.pluralLogic(
      objects,
      locale: localeName,
      other: '$objects pieces of furniture',
      one: '1 piece of furniture',
    );
    return '$_temp0, $_temp1, $_temp2, $_temp3 · $area';
  }

  @override
  String get modelGestureHint =>
      'Drag to rotate, pinch to zoom, double tap to recenter';

  @override
  String get modelLiveTitle => 'Live model';

  @override
  String get modelLiveShort => 'Live';

  @override
  String get expandModel => 'Expand 3D model';

  @override
  String get collapseModel => 'Shrink 3D model';

  @override
  String get rotateLeft => 'Rotate left';

  @override
  String get rotateRight => 'Rotate right';

  @override
  String get tiltUp => 'View more from above';

  @override
  String get tiltDown => 'View more from the side';

  @override
  String get zoomIn => 'Zoom in';

  @override
  String get zoomOut => 'Zoom out';

  @override
  String get topView => 'Top view';

  @override
  String get fitPlan => 'Fit plan';

  @override
  String scaleBar(String label) {
    return 'Scale: $label';
  }

  @override
  String get roomCorridor => 'Hallway';

  @override
  String roomNumbered(int n) {
    return 'Room $n';
  }

  @override
  String get legendDoor => 'Door';

  @override
  String get legendWindow => 'Window';

  @override
  String get legendWall => 'Wall';

  @override
  String get minimapSemantics => 'Coverage map: path, floor seen and walls';

  @override
  String get expand => 'Tap to enlarge';

  @override
  String get collapse => 'Tap to shrink';

  @override
  String get cameraPreview => 'Camera view';

  @override
  String get closeScan => 'Finish scan';

  @override
  String get recording => 'Recording';

  @override
  String get cloudViewSemantics => '3D view of the point cloud';

  @override
  String get cloudViewHint =>
      'Drag with one finger to rotate and with two to pan and zoom; double tap to recenter';

  @override
  String get tabModel => '3D model';

  @override
  String get tabPlan => 'Plan';

  @override
  String get tabCloud => 'Cloud';

  @override
  String planSubtitle(String date, String area) {
    return '$date · $area usable';
  }

  @override
  String get planDisclaimer =>
      'Approximate measurements from the LiDAR scanner. Verify them before construction work.';

  @override
  String get planExport => 'Export SVG';

  @override
  String planExportError(String error) {
    return 'Could not export the plan: $error';
  }

  @override
  String get layoutComputing => 'Building the model and the plan…';

  @override
  String get layoutUnavailable =>
      'Not enough data to build the plan. Walk through the rooms sweeping the walls.';

  @override
  String roomCount(int n) {
    String _temp0 = intl.Intl.pluralLogic(
      n,
      locale: localeName,
      other: '$n rooms',
      one: '1 room',
    );
    return '$_temp0';
  }

  @override
  String get furniture => 'Furniture';

  @override
  String get measuresTitle => 'Measurements';

  @override
  String get measuresTotalArea => 'Total usable area';

  @override
  String get measuresCeiling => 'Ceiling height';

  @override
  String get measuresCeilingEstimated => 'Estimated: the ceiling was not seen';

  @override
  String get measuresOpenings => 'Openings';

  @override
  String doorsWindows(int doors, int windows) {
    String _temp0 = intl.Intl.pluralLogic(
      doors,
      locale: localeName,
      other: '$doors doors',
      one: '1 door',
    );
    String _temp1 = intl.Intl.pluralLogic(
      windows,
      locale: localeName,
      other: '$windows windows',
      one: '1 window',
    );
    return '$_temp0 · $_temp1';
  }

  @override
  String get driftCorrected => 'Drift corrected';

  @override
  String driftCorrectedDetail(int loops, String cm, String deg) {
    String _temp0 = intl.Intl.pluralLogic(
      loops,
      locale: localeName,
      other: '$loops loop closures',
      one: '1 loop closure',
    );
    return '$_temp0 · largest correction $cm cm / $deg°';
  }

  @override
  String get measuresRooms => 'Rooms';

  @override
  String get measuresWalls => 'Walls';

  @override
  String wallItem(int n, String length) {
    return 'Wall $n: $length';
  }

  @override
  String wallThickness(String value) {
    return '$value thick';
  }

  @override
  String doorWidth(String value) {
    return '$value door';
  }

  @override
  String windowWidth(String value) {
    return '$value window';
  }

  @override
  String get fileLayout =>
      'Structured model (JSON: walls, openings, furniture, rooms)';

  @override
  String get fileFloorPlan => 'Floor plan (SVG)';

  @override
  String get summaryArea => 'Usable area';

  @override
  String get summaryRooms => 'Rooms';

  @override
  String get viewResult => 'View model and plan';

  @override
  String get autoTour => 'Automatic tour';

  @override
  String get autoTourDesc =>
      'The virtual scanner walks the whole apartment by itself (about 4 minutes). Handy to try the model and the plan without moving.';
}
