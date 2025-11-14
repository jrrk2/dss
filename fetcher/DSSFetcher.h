#ifndef DSSFETCHER_H
#define DSSFETCHER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrl>
#include <QUrlQuery>
#include <QImage>
#include <QPixmap>
#include <QFile>
#include <QDebug>
#include <functional>

// DSS Survey types
enum class DSSurvey {
    POSS2UKSTU_RED,      // POSS2/UKSTU Red
    POSS2UKSTU_BLUE,     // POSS2/UKSTU Blue
    POSS2UKSTU_IR,       // POSS2/UKSTU Infrared
    POSS1_RED,           // POSS1 Red
    POSS1_BLUE,          // POSS1 Blue
    QUICKV,              // Quick-V Survey
    PHASE2_GSC2,         // Phase 2 GSC2
    PHASE2_GSC1          // Phase 2 GSC1
};

// Image format options
enum class ImageFormat {
    FITS,
    GIF
};

class DSSImageFetcher : public QObject {
    Q_OBJECT

private:
    QNetworkAccessManager* networkManager;
    QString baseUrl;

    QString surveyToString(DSSurvey survey) const {
        switch (survey) {
            case DSSurvey::POSS2UKSTU_RED: return "poss2ukstu_red";
            case DSSurvey::POSS2UKSTU_BLUE: return "poss2ukstu_blue";
            case DSSurvey::POSS2UKSTU_IR: return "poss2ukstu_ir";
            case DSSurvey::POSS1_RED: return "poss1_red";
            case DSSurvey::POSS1_BLUE: return "poss1_blue";
            case DSSurvey::QUICKV: return "quickv";
            case DSSurvey::PHASE2_GSC2: return "phase2_gsc2";
            case DSSurvey::PHASE2_GSC1: return "phase2_gsc1";
            default: return "poss2ukstu_red";
        }
    }

    QString formatToString(ImageFormat format) const {
        return (format == ImageFormat::FITS) ? "fits" : "gif";
    }

public:
    explicit DSSImageFetcher(QObject* parent = nullptr) 
        : QObject(parent), 
          baseUrl("https://archive.stsci.edu/cgi-bin/dss_search") {
        networkManager = new QNetworkAccessManager(this);
    }

    ~DSSImageFetcher() {
        delete networkManager;
    }

    // Fetch DSS image by coordinates (RA/Dec in decimal degrees)
    void fetchByCoordinates(double ra, double dec, 
                           double widthArcmin = 15.0, 
                           double heightArcmin = 15.0,
                           DSSurvey survey = DSSurvey::POSS2UKSTU_RED,
                           ImageFormat format = ImageFormat::GIF) {
        
        QUrl url(baseUrl);
        QUrlQuery query;
        
        query.addQueryItem("r", QString::number(ra, 'f', 6));
        query.addQueryItem("d", QString::number(dec, 'f', 6));
        query.addQueryItem("e", "J2000");  // Equinox
        query.addQueryItem("h", QString::number(heightArcmin, 'f', 2));
        query.addQueryItem("w", QString::number(widthArcmin, 'f', 2));
        query.addQueryItem("f", formatToString(format));
        query.addQueryItem("v", surveyToString(survey));
        query.addQueryItem("s", "on");  // Save to file
        
        url.setQuery(query);
        
        qDebug() << "Fetching DSS image from:" << url.toString();
        
        QNetworkRequest request(url);
        QNetworkReply* reply = networkManager->get(request);
        
        connect(reply, &QNetworkReply::finished, this, [this, reply, format]() {
            handleReply(reply, format);
        });
    }

    // Fetch DSS image by object name (uses SIMBAD/NED resolution)
    void fetchByObjectName(const QString& objectName,
                          double widthArcmin = 15.0,
                          double heightArcmin = 15.0,
                          DSSurvey survey = DSSurvey::POSS2UKSTU_RED,
                          ImageFormat format = ImageFormat::GIF) {
        
        QUrl url(baseUrl);
        QUrlQuery query;
        
        query.addQueryItem("v", objectName);
        query.addQueryItem("e", "J2000");
        query.addQueryItem("h", QString::number(heightArcmin, 'f', 2));
        query.addQueryItem("w", QString::number(widthArcmin, 'f', 2));
        query.addQueryItem("f", formatToString(format));
        query.addQueryItem("v", surveyToString(survey));
        query.addQueryItem("s", "on");
        
        url.setQuery(query);
        
        qDebug() << "Fetching DSS image for object:" << objectName;
        qDebug() << "URL:" << url.toString();
        
        QNetworkRequest request(url);
        QNetworkReply* reply = networkManager->get(request);
        
        connect(reply, &QNetworkReply::finished, this, [this, reply, format]() {
            handleReply(reply, format);
        });
    }

    // Save image to file
    bool saveImage(const QByteArray& data, const QString& filename) {
        QFile file(filename);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(data);
            file.close();
            return true;
        }
        return false;
    }

private slots:
    void handleReply(QNetworkReply* reply, ImageFormat format) {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            
            if (format == ImageFormat::GIF) {
                // Load as QImage for display
                QImage image;
                if (image.loadFromData(data, "GIF")) {
                    emit imageReceived(image, data);
                    qDebug() << "Image received successfully. Size:" 
                             << image.width() << "x" << image.height();
                } else {
                    emit errorOccurred("Failed to load image data");
                }
            } else {
                // FITS format - return raw data
                emit fitsDataReceived(data);
                qDebug() << "FITS data received. Size:" << data.size() << "bytes";
            }
        } else {
            QString errorMsg = QString("Network error: %1").arg(reply->errorString());
            qDebug() << errorMsg;
            emit errorOccurred(errorMsg);
        }
        
        reply->deleteLater();
    }

signals:
    void imageReceived(const QImage& image, const QByteArray& rawData);
    void fitsDataReceived(const QByteArray& fitsData);
    void errorOccurred(const QString& error);
};

#endif // DSSFETCHER_H

// Example usage in your main.cpp or widget:
/*
#include "DSSFetcher.h"
#include <QApplication>
#include <QLabel>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    // Create fetcher
    DSSImageFetcher* fetcher = new DSSImageFetcher();
    
    // Create label to display image
    QLabel* imageLabel = new QLabel();
    imageLabel->setMinimumSize(600, 600);
    imageLabel->show();
    
    // Connect signal to display image
    QObject::connect(fetcher, &DSSImageFetcher::imageReceived, 
                    [imageLabel](const QImage& img, const QByteArray& raw) {
        imageLabel->setPixmap(QPixmap::fromImage(img));
        qDebug() << "Image displayed!";
    });
    
    QObject::connect(fetcher, &DSSImageFetcher::errorOccurred,
                    [](const QString& error) {
        qDebug() << "Error:" << error;
    });
    
    // Fetch M51 (Whirlpool Galaxy)
    fetcher->fetchByObjectName("M51", 20.0, 20.0);
    
    // Or fetch by coordinates (M42 - Orion Nebula)
    // fetcher->fetchByCoordinates(83.8221, -5.3911, 30.0, 30.0);
    
    return app.exec();
}
*/
