#include "app/AppSettings.h"
#include "app/AppPaths.h"

#include <QSettings>

namespace {

constexpr auto kSourceDir       = "paths/sourceDir";
constexpr auto kTargetDir       = "paths/targetDir";
constexpr auto kIncludeSubdirs  = "paths/includeSubdirs";
constexpr auto kExtFilter       = "paths/extensionFilter";

constexpr auto kSpeedFactor     = "params/speedFactor";
constexpr auto kFormat          = "params/format";
constexpr auto kSampleRate      = "params/sampleRate";
constexpr auto kBitDepth        = "params/bitDepth";
constexpr auto kBitrate         = "params/bitrate";
constexpr auto kConflict        = "params/conflict";
constexpr auto kCopyMetadata    = "params/copyMetadata";
constexpr auto kNormalize       = "params/normalize";
constexpr auto kEngine          = "params/engine";
constexpr auto kThreads         = "params/threadCount";

constexpr auto kDarkTheme       = "ui/darkTheme";
constexpr auto kWindowGeometry  = "ui/windowGeometry";
constexpr auto kWindowState     = "ui/windowState";

QSettings* settings()
{
    static QSettings s(AppPaths::configDir() + "/settings.ini", QSettings::IniFormat);
    return &s;
}

} // namespace

AppSettings::AppSettings(QObject* parent) : QObject(parent) {}

QString AppSettings::sourceDir() const
{
    return settings()->value(kSourceDir, AppPaths::defaultSourceDir()).toString();
}
void AppSettings::setSourceDir(const QString& path) { settings()->setValue(kSourceDir, path); }

QString AppSettings::targetDir() const
{
    return settings()->value(kTargetDir, AppPaths::defaultTargetDir()).toString();
}
void AppSettings::setTargetDir(const QString& path) { settings()->setValue(kTargetDir, path); }

bool AppSettings::includeSubdirs() const
{
    return settings()->value(kIncludeSubdirs, true).toBool();
}
void AppSettings::setIncludeSubdirs(bool v) { settings()->setValue(kIncludeSubdirs, v); }

QString AppSettings::extensionFilter() const
{
    return settings()->value(kExtFilter,
                             "*.wav;*.mp3;*.flac;*.ogg;*.m4a;*.aac;*.aiff").toString();
}
void AppSettings::setExtensionFilter(const QString& f) { settings()->setValue(kExtFilter, f); }

ProcessingParameters AppSettings::parameters() const
{
    ProcessingParameters p;
    p.speedFactor     = settings()->value(kSpeedFactor, 1.0).toDouble();
    p.format          = static_cast<OutputFormat>(settings()->value(kFormat,
                                                           static_cast<int>(OutputFormat::SameAsSource)).toInt());
    p.sampleRate      = settings()->value(kSampleRate, 0).toInt();
    p.bitDepth        = settings()->value(kBitDepth, 0).toInt();
    p.bitrateKbps     = settings()->value(kBitrate, 192).toInt();
    p.conflict        = static_cast<ConflictPolicy>(settings()->value(kConflict,
                                                               static_cast<int>(ConflictPolicy::Rename)).toInt());
    p.copyMetadata    = settings()->value(kCopyMetadata, true).toBool();
    p.normalizeVolume = settings()->value(kNormalize, false).toBool();
    p.engine          = static_cast<AudioEngine>(settings()->value(kEngine,
                                                          static_cast<int>(AudioEngine::SoundTouch)).toInt());
    p.threadCount     = settings()->value(kThreads, 4).toInt();
    return p;
}

void AppSettings::setParameters(const ProcessingParameters& p)
{
    settings()->setValue(kSpeedFactor, p.speedFactor);
    settings()->setValue(kFormat,      static_cast<int>(p.format));
    settings()->setValue(kSampleRate,  p.sampleRate);
    settings()->setValue(kBitDepth,    p.bitDepth);
    settings()->setValue(kBitrate,     p.bitrateKbps);
    settings()->setValue(kConflict,    static_cast<int>(p.conflict));
    settings()->setValue(kCopyMetadata,p.copyMetadata);
    settings()->setValue(kNormalize,   p.normalizeVolume);
    settings()->setValue(kEngine,      static_cast<int>(p.engine));
    settings()->setValue(kThreads,     p.threadCount);
}

bool AppSettings::darkTheme() const { return settings()->value(kDarkTheme, false).toBool(); }
void AppSettings::setDarkTheme(bool v) { settings()->setValue(kDarkTheme, v); }

QByteArray AppSettings::windowGeometry() const { return settings()->value(kWindowGeometry).toByteArray(); }
void AppSettings::setWindowGeometry(const QByteArray& g) { settings()->setValue(kWindowGeometry, g); }

QByteArray AppSettings::windowState() const { return settings()->value(kWindowState).toByteArray(); }
void AppSettings::setWindowState(const QByteArray& s) { settings()->setValue(kWindowState, s); }
