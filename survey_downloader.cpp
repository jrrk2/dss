// survey_downloader.cpp - Download survey images for plate solver testing
// Integrates with existing ProperHipsClient infrastructure
#include <QApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include "ProperHipsClient.h"
#include "EnhancedMosaicCreator.h"

class SurveyDownloader : public QObject {
    Q_OBJECT

public:
    explicit SurveyDownloader(QObject *parent = nullptr) : QObject(parent) {
        m_hipsClient = new ProperHipsClient(this);
        m_mosaicCreator = new EnhancedMosaicCreator(this);
        
        // Set up output directory
        QString homeDir = QDir::homePath();
        m_outputDir = QDir(homeDir).absoluteFilePath("plate_solver_test_images");
        QDir().mkpath(m_outputDir);
        
        qDebug() << "Survey Downloader initialized";
        qDebug() << "Output directory:" << m_outputDir;
        
        connect(m_mosaicCreator, &EnhancedMosaicCreator::mosaicComplete,
                this, &SurveyDownloader::onImageReady);
    }
    
    // Download image for specific coordinates
    void downloadForCoordinates(double ra_deg, double dec_deg, 
                               const QString& name = "test_image") {
        qDebug() << QString("\n=== Downloading image for %1 ===").arg(name);
        qDebug() << QString("Coordinates: RA=%1°, Dec=%2°").arg(ra_deg).arg(dec_deg);
        
        m_currentName = name;
        m_currentRA = ra_deg;
        m_currentDec = dec_deg;
        
        // Create sky position
        SkyPosition pos;
        pos.ra_deg = ra_deg;
        pos.dec_deg = dec_deg;
        pos.name = name;
        pos.description = QString("Test image for plate solver at RA=%1°, Dec=%2°")
                         .arg(ra_deg).arg(dec_deg);
        
        // Convert to HMS/DMS for the mosaic creator
        QString raText = degToHMS(ra_deg);
        QString decText = degToDMS(dec_deg);
        
        qDebug() << "Converted coordinates:";
        qDebug() << "  RA (HMS):" << raText;
        qDebug() << "  Dec (DMS):" << decText;
        
        // Use mosaic creator to fetch the image
        m_mosaicCreator->setCustomCoordinates(raText, decText, name);
        m_mosaicCreator->createCustomMosaic(pos);
    }
    
    // Download images for a test field grid
    void downloadTestGrid(double center_ra, double center_dec, 
                         int grid_size = 3, double spacing_deg = 1.0) {
        qDebug() << QString("\n=== Downloading %1x%1 test grid ===").arg(grid_size);
        qDebug() << QString("Center: RA=%1°, Dec=%2°").arg(center_ra).arg(center_dec);
        qDebug() << QString("Spacing: %1°").arg(spacing_deg);
        
        m_testQueue.clear();
        
        // Create grid of test positions
        for (int y = 0; y < grid_size; y++) {
            for (int x = 0; x < grid_size; x++) {
                double offset_x = (x - grid_size/2) * spacing_deg;
                double offset_y = (y - grid_size/2) * spacing_deg;
                
                double ra = center_ra + offset_x / cos(center_dec * M_PI / 180.0);
                double dec = center_dec + offset_y;
                
                // Normalize RA
                while (ra < 0) ra += 360.0;
                while (ra >= 360.0) ra -= 360.0;
                
                // Clamp Dec
                if (dec > 90.0) dec = 90.0;
                if (dec < -90.0) dec = -90.0;
                
                TestPosition pos;
                pos.ra_deg = ra;
                pos.dec_deg = dec;
                pos.name = QString("grid_%1_%2").arg(x).arg(y);
                
                m_testQueue.append(pos);
                
                qDebug() << QString("  Grid[%1,%2]: RA=%3°, Dec=%4°")
                            .arg(x).arg(y).arg(ra, 0, 'f', 4).arg(dec, 0, 'f', 4);
            }
        }
        
        qDebug() << QString("Created test queue with %1 positions").arg(m_testQueue.size());
        processNextInQueue();
    }
    
    // Download images for common test targets
    void downloadCommonTargets() {
        qDebug() << "\n=== Downloading common test targets ===";
        
        m_testQueue.clear();
        
        // Add well-known targets at various declinations
        struct Target {
            QString name;
            double ra_deg;
            double dec_deg;
        };
        
        QList<Target> targets = {
            {"M31_Andromeda", 10.6847, 41.2687},        // Northern
            {"M42_Orion", 83.8221, -5.3911},            // Equatorial
            {"M51_Whirlpool", 202.4696, 47.1952},       // Northern
            {"M81_Bodes", 148.8884, 69.0653},           // Far Northern
            {"Polaris", 37.9546, 89.2641},              // North Pole
            {"Vega", 279.2346, 38.7837},                // Summer
            {"Sirius", 101.2872, -16.7161},             // Bright star
            {"Betelgeuse", 88.7929, 7.4070}             // Another bright star
        };
        
        for (const Target& target : targets) {
            TestPosition pos;
            pos.ra_deg = target.ra_deg;
            pos.dec_deg = target.dec_deg;
            pos.name = target.name;
            m_testQueue.append(pos);
            
            qDebug() << QString("  %1: RA=%2°, Dec=%3°")
                        .arg(target.name).arg(target.ra_deg).arg(target.dec_deg);
        }
        
        processNextInQueue();
    }
    
    // Generate metadata file for plate solver testing
    void generateMetadataFile() {
        QString metadataPath = QString("%1/test_metadata.csv").arg(m_outputDir);
        QFile file(metadataPath);
        
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            qDebug() << "Failed to create metadata file:" << metadataPath;
            return;
        }
        
        QTextStream out(&file);
        out << "Filename,RA_deg,Dec_deg,RA_HMS,Dec_DMS,FOV_width,FOV_height,Pixel_scale,Image_width,Image_height,Survey\n";
        
        // Your camera specs
        double pixel_scale = 1.2; // arcsec/pixel
        int width = 3072;
        int height = 2048;
        double fov_width = (pixel_scale * width) / 3600.0;  // degrees
        double fov_height = (pixel_scale * height) / 3600.0; // degrees
        
        for (const TestPosition& pos : m_downloadedImages) {
            QString raHMS = degToHMS(pos.ra_deg);
            QString decDMS = degToDMS(pos.dec_deg);
            
            out << QString("%1.png,%2,%3,%4,%5,%6,%7,%8,%9,%10,DSS2_Color\n")
                   .arg(pos.name)
                   .arg(pos.ra_deg, 0, 'f', 6)
                   .arg(pos.dec_deg, 0, 'f', 6)
                   .arg(raHMS)
                   .arg(decDMS)
                   .arg(fov_width, 0, 'f', 4)
                   .arg(fov_height, 0, 'f', 4)
                   .arg(pixel_scale, 0, 'f', 2)
                   .arg(width)
                   .arg(height);
        }
        
        file.close();
        qDebug() << "\nMetadata file created:" << metadataPath;
    }

private slots:
    void onImageReady(const QImage& image) {
        if (image.isNull()) {
            qDebug() << "❌ Failed to generate image for" << m_currentName;
            processNextInQueue();
            return;
        }
        
        qDebug() << "✅ Image generated:" << image.width() << "x" << image.height();
        
        // Resize to match your camera resolution
        QImage resized = image.scaled(3072, 2048, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        
        // If aspect ratio doesn't match, create black letterbox
        if (resized.width() != 3072 || resized.height() != 2048) {
            QImage final(3072, 2048, QImage::Format_RGB888);
            final.fill(Qt::black);
            
            int x = (3072 - resized.width()) / 2;
            int y = (2048 - resized.height()) / 2;
            
            QPainter painter(&final);
            painter.drawImage(x, y, resized);
            painter.end();
            
            resized = final;
        }
        
        // Save the image
        QString filename = QString("%1/%2.png").arg(m_outputDir).arg(m_currentName);
        bool saved = resized.save(filename);
        
        if (saved) {
            qDebug() << "✅ Saved:" << filename;
            qDebug() << "   Size:" << resized.width() << "x" << resized.height();
            
            // Record this download with current position info
            TestPosition pos;
            pos.name = m_currentName;
            pos.ra_deg = m_currentRA;
            pos.dec_deg = m_currentDec;
            m_downloadedImages.append(pos);
        } else {
            qDebug() << "❌ Failed to save:" << filename;
        }
        
        // Process next in queue
        QTimer::singleShot(1000, this, &SurveyDownloader::processNextInQueue);
    }
    
    void processNextInQueue() {
        if (m_testQueue.isEmpty()) {
            qDebug() << "\n=== All downloads complete ===";
            qDebug() << "Total images:" << m_downloadedImages.size();
            qDebug() << "Location:" << m_outputDir;
            generateMetadataFile();
            QTimer::singleShot(1000, qApp, &QApplication::quit);
            return;
        }
        
        TestPosition pos = m_testQueue.takeFirst();
        qDebug() << QString("\n[%1/%2] Processing: %3")
                    .arg(m_downloadedImages.size() + 1)
                    .arg(m_downloadedImages.size() + m_testQueue.size() + 1)
                    .arg(pos.name);
        
        downloadForCoordinates(pos.ra_deg, pos.dec_deg, pos.name);
    }

private:
    ProperHipsClient* m_hipsClient;
    EnhancedMosaicCreator* m_mosaicCreator;
    QString m_outputDir;
    QString m_currentName;
    double m_currentRA;
    double m_currentDec;
    
    struct TestPosition {
        QString name;
        double ra_deg;
        double dec_deg;
    };
    
    QList<TestPosition> m_testQueue;
    QList<TestPosition> m_downloadedImages;
    
    // Coordinate conversion helpers
    QString degToHMS(double deg) const {
        double hours = deg / 15.0;
        int h = static_cast<int>(hours);
        double min_decimal = (hours - h) * 60.0;
        int m = static_cast<int>(min_decimal);
        double s = (min_decimal - m) * 60.0;
        
        return QString("%1h%2m%3s")
               .arg(h, 2, 10, QChar('0'))
               .arg(m, 2, 10, QChar('0'))
               .arg(s, 0, 'f', 1);
    }
    
    QString degToDMS(double deg) const {
        bool negative = deg < 0;
        double abs_deg = std::abs(deg);
        int d = static_cast<int>(abs_deg);
        double min_decimal = (abs_deg - d) * 60.0;
        int m = static_cast<int>(min_decimal);
        double s = (min_decimal - m) * 60.0;
        
        return QString("%1%2d%3m%4s")
               .arg(negative ? "-" : "+")
               .arg(d, 2, 10, QChar('0'))
               .arg(m, 2, 10, QChar('0'))
               .arg(s, 0, 'f', 1);
    }
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("Survey Image Downloader");
    app.setApplicationVersion("1.0");
    
    QCommandLineParser parser;
    parser.setApplicationDescription("Download survey images for plate solver testing");
    parser.addHelpOption();
    parser.addVersionOption();
    
    // Add options
    QCommandLineOption modeOption(QStringList() << "m" << "mode",
        "Download mode: single, grid, or targets", "mode", "targets");
    parser.addOption(modeOption);
    
    QCommandLineOption raOption(QStringList() << "r" << "ra",
        "Right Ascension in degrees", "ra");
    parser.addOption(raOption);
    
    QCommandLineOption decOption(QStringList() << "d" << "dec",
        "Declination in degrees", "dec");
    parser.addOption(decOption);
    
    QCommandLineOption nameOption(QStringList() << "n" << "name",
        "Image name", "name", "test_image");
    parser.addOption(nameOption);
    
    QCommandLineOption gridSizeOption(QStringList() << "g" << "grid-size",
        "Grid size (NxN)", "size", "3");
    parser.addOption(gridSizeOption);
    
    QCommandLineOption spacingOption(QStringList() << "s" << "spacing",
        "Grid spacing in degrees", "spacing", "1.0");
    parser.addOption(spacingOption);
    
    parser.process(app);
    
    // Create downloader
    SurveyDownloader downloader;
    
    QString mode = parser.value(modeOption);
    
    if (mode == "single") {
        if (!parser.isSet(raOption) || !parser.isSet(decOption)) {
            qDebug() << "Error: RA and Dec required for single mode";
            qDebug() << "Example: -m single -r 202.47 -d 47.20 -n M51";
            return 1;
        }
        
        double ra = parser.value(raOption).toDouble();
        double dec = parser.value(decOption).toDouble();
        QString name = parser.value(nameOption);
        
        downloader.downloadForCoordinates(ra, dec, name);
        
    } else if (mode == "grid") {
        if (!parser.isSet(raOption) || !parser.isSet(decOption)) {
            qDebug() << "Error: RA and Dec required for grid center";
            qDebug() << "Example: -m grid -r 202.47 -d 47.20 -g 5 -s 0.5";
            return 1;
        }
        
        double ra = parser.value(raOption).toDouble();
        double dec = parser.value(decOption).toDouble();
        int gridSize = parser.value(gridSizeOption).toInt();
        double spacing = parser.value(spacingOption).toDouble();
        
        downloader.downloadTestGrid(ra, dec, gridSize, spacing);
        
    } else if (mode == "targets") {
        qDebug() << "Downloading common astronomical targets...";
        downloader.downloadCommonTargets();
        
    } else {
        qDebug() << "Error: Unknown mode:" << mode;
        qDebug() << "Valid modes: single, grid, targets";
        parser.showHelp(1);
    }
    
    return app.exec();
}

#include "survey_downloader.moc"
