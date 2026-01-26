import 'package:logging/logging.dart';
import 'package:ansicolor/ansicolor.dart';
import 'package:intl/intl.dart';

void initLogging() {
  ansiColorDisabled = false;
  // Logger.level = Level.all;
  final DateFormat formatter = DateFormat('HH:mm:ss.SSS');

  recordStackTraceAtLevel = Level.ALL;
  Logger.root.level = Level.ALL; // defaults to Level.INFO

  /// Global app logger configured for easy debugging
  Logger.root.onRecord.listen((record) {
    // final time = record.time.toIso8601String();
    // final level = record.level.name;
    // final message = record.message;
    // final error = record.error != null ? ' Error: ${record.error}' : '';
    // final stackTrace = record.stackTrace != null ? '\nStackTrace: ${record.stackTrace}' : '';
    AnsiPen pen = AnsiPen();

    switch (record.level) {
      case Level.SHOUT:
      case Level.SEVERE:
        pen = pen..red(bold: true);
        break;

      case Level.WARNING:
        pen = pen..yellow(bold: true);
        break;

      case Level.INFO:
        pen = pen..green();
        break;

      case Level.CONFIG:
        pen = pen..blue();
        break;

      case Level.FINE:
        pen = pen..gray(level: 0.2);
        break;

      case Level.FINER:
        pen = pen..gray(level: 0.2);
        break;

      case Level.FINEST:
        pen = pen..gray(level: 0.2);
        break;
    }

    // ignore: avoid_print
    print(pen('${formatter.format(record.time)} ${record.level.name.substring(0, 4)}: [${record.loggerName}] ${record.message}'));
  });
}

Logger getLogger(Object any) {
  if (any is String) {
    return Logger(any);
  } else {
    return Logger(any.runtimeType.toString());
  }
}

final Logger logger = getLogger('AppLogger');
