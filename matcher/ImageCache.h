#ifndef IMAGECACHE_H
#define IMAGECACHE_H

#include <QObject>
#include <QString>
#include <QDir>
#include <QFile>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QStandardPaths>
#include <QDebug>

class ImageCache : public QObject {
    Q_OBJECT
public:
    QString surveyKey(DSSurvey survey) const {
	  switch (survey) {
	      case DSSurvey::POSS2UKSTU_RED:  return "poss2ukstu_red";
	      case DSSurvey::POSS2UKSTU_BLUE: return "poss2ukstu_blue";
	      case DSSurvey::POSS2UKSTU_IR:   return "poss2ukstu_ir";
	      case DSSurvey::POSS1_RED:       return "poss1_red";
	      case DSSurvey::POSS1_BLUE:      return "poss1_blue";
	      case DSSurvey::QUICKV:          return "quickv";
	      case DSSurvey::PHASE2_GSC2:     return "phase2_gsc2";
	      case DSSurvey::PHASE2_GSC1:     return "phase2_gsc1";
	      default:                        return "poss2ukstu_red";
	  }
      }
  
private:
    QString cacheDir;
    QString metadataFile;
    QJsonObject metadata;

    // Generate cache key from parameters
    QString generateCacheKey(double ra, double dec, double width, double height,
                            const QString& survey, const QString& format) const {
        QString key = QString("%1_%2_%3_%4_%5_%6")
            .arg(ra, 0, 'f', 6)
            .arg(dec, 0, 'f', 6)
            .arg(width, 0, 'f', 2)
            .arg(height, 0, 'f', 2)
            .arg(survey)
            .arg(format);
        
        // Create hash for filename
        QByteArray hash = QCryptographicHash::hash(key.toUtf8(), 
                                                   QCryptographicHash::Md5);
        return QString(hash.toHex());
    }
    
    QString getCachePath(const QString& cacheKey, const QString& format) const {
        QString ext = (format == "fits") ? "fits" : "gif";
        return cacheDir + "/" + cacheKey + "." + ext;
    }
    
    void loadMetadata() {
        QFile file(metadataFile);
        if (file.open(QIODevice::ReadOnly)) {
            QByteArray data = file.readAll();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            metadata = doc.object();
            file.close();
        }
    }
    
    void saveMetadata() {
        QFile file(metadataFile);
        if (file.open(QIODevice::WriteOnly)) {
            QJsonDocument doc(metadata);
            file.write(doc.toJson());
            file.close();
        }
    }

public:
    explicit ImageCache(QObject* parent = nullptr) : QObject(parent) {
        // Use application cache directory
        QString appCache = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
        cacheDir = appCache + "/DSS_Images";
        metadataFile = cacheDir + "/metadata.json";
        
        // Create cache directory if it doesn't exist
        QDir dir;
        if (!dir.exists(cacheDir)) {
            dir.mkpath(cacheDir);
            qDebug() << "Created cache directory:" << cacheDir;
        }
        
        loadMetadata();
    }
    
    // Check if cached version exists
    bool isCached(double ra, double dec, double width, double height,
                  const QString& survey, const QString& format) const {
        QString key = generateCacheKey(ra, dec, width, height, survey, format);
        QString path = getCachePath(key, format);
        return QFile::exists(path);
    }
    
    // Get cached image data
    QByteArray getCachedImage(double ra, double dec, double width, double height,
                             const QString& survey, const QString& format) {
        QString key = generateCacheKey(ra, dec, width, height, survey, format);
        QString path = getCachePath(key, format);
        
        QFile file(path);
        if (file.open(QIODevice::ReadOnly)) {
            QByteArray data = file.readAll();
            file.close();
            
            // Update access time in metadata
            QJsonObject entry = metadata[key].toObject();
            entry["lastAccess"] = QDateTime::currentDateTime().toString(Qt::ISODate);
            entry["accessCount"] = entry["accessCount"].toInt() + 1;
            metadata[key] = entry;
            saveMetadata();
            
            qDebug() << "Cache hit for:" << key;
            return data;
        }
        
        return QByteArray();
    }
    
    // Save image to cache
    void cacheImage(const QByteArray& data, double ra, double dec, 
                   double width, double height,
                   const QString& survey, const QString& format,
                   const QString& objectName = "") {
        QString key = generateCacheKey(ra, dec, width, height, survey, format);
        QString path = getCachePath(key, format);
        
        QFile file(path);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(data);
            file.close();
            
            // Update metadata
            QJsonObject entry;
            entry["ra"] = ra;
            entry["dec"] = dec;
            entry["width"] = width;
            entry["height"] = height;
            entry["survey"] = survey;
            entry["format"] = format;
            entry["objectName"] = objectName;
            entry["created"] = QDateTime::currentDateTime().toString(Qt::ISODate);
            entry["lastAccess"] = QDateTime::currentDateTime().toString(Qt::ISODate);
            entry["accessCount"] = 1;
            entry["size"] = data.size();
            
            metadata[key] = entry;
            saveMetadata();
            
            qDebug() << "Cached image:" << key << "Size:" << data.size();
        }
    }
    
    // Get cache statistics
    struct CacheStats {
        int totalImages;
        qint64 totalSize;
    };
    
    CacheStats getStats() const {
        CacheStats stats = {0, 0};
        
        QStringList keys = metadata.keys();
        stats.totalImages = keys.size();
        
        for (const QString& key : keys) {
            QJsonObject entry = metadata[key].toObject();
            stats.totalSize += entry["size"].toVariant().toLongLong();            
        }
        
        return stats;
    }
    
    // Clear cache
    void clearCache() {
        QDir dir(cacheDir);
        QStringList files = dir.entryList(QDir::Files | QDir::NoDotAndDotDot);
        
        for (const QString& file : files) {
            if (file != "metadata.json") {
                QFile::remove(dir.filePath(file));
            }
        }
        
        metadata = QJsonObject();
        saveMetadata();
        
        qDebug() << "Cache cleared";
    }
    
    // Remove old cached items (LRU)
    void cleanupOldEntries(int maxAgeHours = 720) {  // 30 days default
        QDateTime cutoff = QDateTime::currentDateTime().addSecs(-maxAgeHours * 3600);
        QStringList keysToRemove;
        
        QStringList keys = metadata.keys();
        for (const QString& key : keys) {
            QJsonObject entry = metadata[key].toObject();
            QDateTime lastAccess = QDateTime::fromString(
                entry["lastAccess"].toString(), Qt::ISODate);
            
            if (lastAccess < cutoff) {
                keysToRemove.append(key);
                
                // Remove file
                QString format = entry["format"].toString();
                QString path = getCachePath(key, format);
                QFile::remove(path);
            }
        }
        
        // Remove metadata entries
        for (const QString& key : keysToRemove) {
            metadata.remove(key);
        }
        
        if (!keysToRemove.isEmpty()) {
            saveMetadata();
            qDebug() << "Removed" << keysToRemove.size() << "old cache entries";
        }
    }
    
    QString getCacheDirectory() const {
        return cacheDir;
    }
};

#endif // IMAGECACHE_H
