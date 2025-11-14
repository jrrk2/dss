// main.cpp - DSS Image Fetcher using Messier Catalog
#include "DSSFetcher.h"
#include "MessierCatalog.h"
#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QBuffer>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QMessageBox>
#include <QFileDialog>
#include <QProgressBar>
#include <QCheckBox>
#include <QListWidget>

class DSSViewerWindow : public QWidget {
    Q_OBJECT

private:
    DSSImageFetcher* fetcher;
    QLabel* imageLabel;
    QLabel* statusLabel;
    QLabel* objectInfoLabel;
    QProgressBar* progressBar;
    
    // Input controls
    QComboBox* messierObjectCombo;
    QListWidget* messierObjectList;
    QCheckBox* imagedOnlyCheckbox;
    QComboBox* filterTypeCombo;
    QDoubleSpinBox* widthSpinBox;
    QDoubleSpinBox* heightSpinBox;
    QComboBox* surveyCombo;
    QComboBox* formatCombo;
    
    QPushButton* fetchObjectBtn;
    QPushButton* fetchCompositeBtn;
    QPushButton* saveImageBtn;
    QPushButton* autoFetchAllBtn;
    
    QByteArray currentImageData;
    QImage currentImage;
    MessierObject currentObject;
    
    // For composite image fetching
    QImage irImage, redImage, blueImage;
    int compositeFetchCount;
    bool fetchingComposite;

public:
    DSSViewerWindow(QWidget* parent = nullptr) : QWidget(parent), 
        compositeFetchCount(0), fetchingComposite(false) {
        setWindowTitle("DSS Image Fetcher - Messier Catalog");
        resize(1200, 900);
        
        // Create fetcher
        fetcher = new DSSImageFetcher(this);
        
        // Setup UI
        setupUI();
        
        // Connect signals
        connectSignals();
        
        // Populate Messier objects
        populateMessierObjects();
        
        // Set default values
        setDefaults();
    }

private:
    void setupUI() {
        QHBoxLayout* mainLayout = new QHBoxLayout(this);
        
        // Left panel - Controls
        QVBoxLayout* leftPanel = new QVBoxLayout();
        leftPanel->setSpacing(10);
        
        // Messier object selection
        QGroupBox* objectGroup = new QGroupBox("Select Messier Object");
        QVBoxLayout* objectLayout = new QVBoxLayout(objectGroup);
        
        // Combo box for quick selection
        QHBoxLayout* comboLayout = new QHBoxLayout();
        comboLayout->addWidget(new QLabel("Quick Select:"));
        messierObjectCombo = new QComboBox();
        messierObjectCombo->setMaxVisibleItems(20);
        comboLayout->addWidget(messierObjectCombo);
        objectLayout->addLayout(comboLayout);
        
        // Filter options
        QHBoxLayout* filterLayout = new QHBoxLayout();
        imagedOnlyCheckbox = new QCheckBox("Show only imaged objects");
        filterLayout->addWidget(imagedOnlyCheckbox);
        
        filterTypeCombo = new QComboBox();
        filterTypeCombo->addItem("All Types", -1);
        filterTypeCombo->addItem("Galaxies", (int)MessierObjectType::GALAXY);
        filterTypeCombo->addItem("Nebulae", (int)MessierObjectType::NEBULA);
        filterTypeCombo->addItem("Globular Clusters", (int)MessierObjectType::GLOBULAR_CLUSTER);
        filterTypeCombo->addItem("Open Clusters", (int)MessierObjectType::OPEN_CLUSTER);
        filterTypeCombo->addItem("Planetary Nebulae", (int)MessierObjectType::PLANETARY_NEBULA);
        filterLayout->addWidget(new QLabel("Type:"));
        filterLayout->addWidget(filterTypeCombo);
        objectLayout->addLayout(filterLayout);
        
        // List widget for browsing
        messierObjectList = new QListWidget();
        messierObjectList->setMaximumHeight(300);
        objectLayout->addWidget(messierObjectList);
        
        leftPanel->addWidget(objectGroup);
        
        // Object info display
        objectInfoLabel = new QLabel("Select an object to view details");
        objectInfoLabel->setWordWrap(true);
        objectInfoLabel->setStyleSheet("QLabel { padding: 10px; background-color: #f9f9f9; border: 1px solid #ddd; border-radius: 4px; }");
        objectInfoLabel->setMinimumHeight(150);
        leftPanel->addWidget(objectInfoLabel);
        
        // Fetch parameters
        QGroupBox* paramGroup = new QGroupBox("Fetch Parameters");
        QVBoxLayout* paramLayout = new QVBoxLayout(paramGroup);
        
        // Field of view
        QHBoxLayout* fovLayout = new QHBoxLayout();
        fovLayout->addWidget(new QLabel("Width (arcmin):"));
        widthSpinBox = new QDoubleSpinBox();
        widthSpinBox->setRange(1.0, 60.0);
        widthSpinBox->setValue(20.0);
        widthSpinBox->setDecimals(1);
        fovLayout->addWidget(widthSpinBox);
        
        fovLayout->addWidget(new QLabel("Height (arcmin):"));
        heightSpinBox = new QDoubleSpinBox();
        heightSpinBox->setRange(1.0, 60.0);
        heightSpinBox->setValue(20.0);
        heightSpinBox->setDecimals(1);
        fovLayout->addWidget(heightSpinBox);
        paramLayout->addLayout(fovLayout);
        
        // Survey and format selection
        QHBoxLayout* surveyLayout = new QHBoxLayout();
        surveyLayout->addWidget(new QLabel("Survey:"));
        surveyCombo = new QComboBox();
        surveyCombo->addItem("POSS2/UKSTU Red", (int)DSSurvey::POSS2UKSTU_RED);
        surveyCombo->addItem("POSS2/UKSTU Blue", (int)DSSurvey::POSS2UKSTU_BLUE);
        surveyCombo->addItem("POSS2/UKSTU IR", (int)DSSurvey::POSS2UKSTU_IR);
        surveyCombo->addItem("POSS1 Red", (int)DSSurvey::POSS1_RED);
        surveyCombo->addItem("POSS1 Blue", (int)DSSurvey::POSS1_BLUE);
        surveyCombo->addItem("Quick-V", (int)DSSurvey::QUICKV);
        surveyLayout->addWidget(surveyCombo);
        paramLayout->addLayout(surveyLayout);
        
        QHBoxLayout* formatLayout = new QHBoxLayout();
        formatLayout->addWidget(new QLabel("Format:"));
        formatCombo = new QComboBox();
        formatCombo->addItem("GIF (Display)", (int)ImageFormat::GIF);
        formatCombo->addItem("FITS (Science)", (int)ImageFormat::FITS);
        formatLayout->addWidget(formatCombo);
        paramLayout->addLayout(formatLayout);
        
        leftPanel->addWidget(paramGroup);
        
        // Action buttons
        fetchObjectBtn = new QPushButton("Fetch Selected Survey");
        fetchObjectBtn->setStyleSheet("QPushButton { padding: 8px; font-weight: bold; }");
        leftPanel->addWidget(fetchObjectBtn);
        
        fetchCompositeBtn = new QPushButton("Fetch False Color Composite (IR/Red/Blue)");
        fetchCompositeBtn->setStyleSheet("QPushButton { padding: 8px; font-weight: bold; background-color: #4CAF50; color: white; }");
        leftPanel->addWidget(fetchCompositeBtn);
        
        autoFetchAllBtn = new QPushButton("Auto-Fetch All Imaged Objects");
        autoFetchAllBtn->setEnabled(false); // Enable this feature later if needed
        leftPanel->addWidget(autoFetchAllBtn);
        
        leftPanel->addStretch();
        
        // Right panel - Image display
        QVBoxLayout* rightPanel = new QVBoxLayout();
        
        // Progress bar
        progressBar = new QProgressBar();
        progressBar->setRange(0, 0); // Indeterminate
        progressBar->hide();
        rightPanel->addWidget(progressBar);
        
        // Status label
        statusLabel = new QLabel("Ready to fetch DSS images from Messier Catalog");
        statusLabel->setStyleSheet("QLabel { padding: 5px; background-color: #f0f0f0; }");
        rightPanel->addWidget(statusLabel);
        
        // Image display
        QGroupBox* imageGroup = new QGroupBox("Image Display");
        QVBoxLayout* imageLayout = new QVBoxLayout(imageGroup);
        
        imageLabel = new QLabel();
        imageLabel->setMinimumSize(700, 700);
        imageLabel->setAlignment(Qt::AlignCenter);
        imageLabel->setStyleSheet("QLabel { background-color: black; color: white; }");
        imageLabel->setText("No image loaded\nSelect a Messier object and click 'Fetch'");
        imageLayout->addWidget(imageLabel);
        
        // Save button
        QHBoxLayout* saveLayout = new QHBoxLayout();
        saveImageBtn = new QPushButton("Save Image");
        saveImageBtn->setEnabled(false);
        saveLayout->addStretch();
        saveLayout->addWidget(saveImageBtn);
        imageLayout->addLayout(saveLayout);
        
        rightPanel->addWidget(imageGroup);
        
        // Add panels to main layout
        mainLayout->addLayout(leftPanel, 1);
        mainLayout->addLayout(rightPanel, 2);
        
        setLayout(mainLayout);
    }
    
    void connectSignals() {
        // Fetch buttons
        connect(fetchObjectBtn, &QPushButton::clicked, this, &DSSViewerWindow::onFetchObject);
        connect(fetchCompositeBtn, &QPushButton::clicked, this, &DSSViewerWindow::onFetchComposite);
        connect(saveImageBtn, &QPushButton::clicked, this, &DSSViewerWindow::onSaveImage);
        
        // Object selection
        connect(messierObjectCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
                this, &DSSViewerWindow::onObjectSelected);
        connect(messierObjectList, &QListWidget::currentRowChanged,
                this, &DSSViewerWindow::onListObjectSelected);
        
        // Filters
        connect(imagedOnlyCheckbox, &QCheckBox::stateChanged, 
                this, &DSSViewerWindow::updateObjectList);
        connect(filterTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &DSSViewerWindow::updateObjectList);
        
        // Fetcher signals
        connect(fetcher, &DSSImageFetcher::imageReceived, this, &DSSViewerWindow::onImageReceived);
        connect(fetcher, &DSSImageFetcher::fitsDataReceived, this, &DSSViewerWindow::onFitsReceived);
        connect(fetcher, &DSSImageFetcher::errorOccurred, this, &DSSViewerWindow::onError);
    }
    
    void setDefaults() {
        widthSpinBox->setValue(20.0);
        heightSpinBox->setValue(20.0);
    }
    
    void populateMessierObjects() {
        updateObjectList();
    }
    
    void updateObjectList() {
        messierObjectCombo->clear();
        messierObjectList->clear();
        
        bool imagedOnly = imagedOnlyCheckbox->isChecked();
        int typeFilter = filterTypeCombo->currentData().toInt();
        
        QList<MessierObject> objects = MessierCatalog::getAllObjects();
        
        for (const auto& obj : objects) {
            // Apply filters
            if (imagedOnly && !obj.has_been_imaged) continue;
            if (typeFilter >= 0 && (int)obj.object_type != typeFilter) continue;
            
            QString displayText = obj.name;
            if (!obj.common_name.isEmpty()) {
                displayText += " - " + obj.common_name;
            }
            displayText += " (" + MessierCatalog::objectTypeToString(obj.object_type) + ")";
            
            messierObjectCombo->addItem(displayText, obj.id);
            messierObjectList->addItem(displayText);
        }
        
        if (messierObjectCombo->count() > 0) {
            messierObjectCombo->setCurrentIndex(0);
        }
    }

private slots:
    void onObjectSelected(int index) {
        if (index < 0) return;
        
        int objectId = messierObjectCombo->itemData(index).toInt();
        currentObject = MessierCatalog::getObjectById(objectId);
        
        displayObjectInfo(currentObject);
        
        // Sync list selection
        messierObjectList->blockSignals(true);
        messierObjectList->setCurrentRow(index);
        messierObjectList->blockSignals(false);
        
        // Auto-adjust FOV based on object size
        autoAdjustFOV(currentObject);
    }
    
    void onListObjectSelected(int row) {
        if (row < 0) return;
        
        messierObjectCombo->blockSignals(true);
        messierObjectCombo->setCurrentIndex(row);
        messierObjectCombo->blockSignals(false);
        
        onObjectSelected(row);
    }
    
    void displayObjectInfo(const MessierObject& obj) {
        QString info;
        info += QString("<b>%1</b>").arg(obj.name);
        if (!obj.common_name.isEmpty()) {
            info += QString(" - %1").arg(obj.common_name);
        }
        info += "<br><br>";
        
        info += QString("<b>Type:</b> %1<br>").arg(MessierCatalog::objectTypeToString(obj.object_type));
        info += QString("<b>Constellation:</b> %1<br>").arg(MessierCatalog::constellationToString(obj.constellation));
        info += QString("<b>Coordinates:</b><br>");
        info += QString("  RA: %1° (J2000)<br>").arg(obj.sky_position.ra_deg, 0, 'f', 4);
        info += QString("  Dec: %1° (J2000)<br>").arg(obj.sky_position.dec_deg, 0, 'f', 4);
        info += QString("<b>Magnitude:</b> %1<br>").arg(obj.magnitude, 0, 'f', 1);
        info += QString("<b>Distance:</b> %1 kly<br>").arg(obj.distance_kly, 0, 'f', 1);
        info += QString("<b>Size:</b> %1' × %2'<br>")
                .arg(obj.size_arcmin.width(), 0, 'f', 1)
                .arg(obj.size_arcmin.height(), 0, 'f', 1);
        info += QString("<b>Best Viewed:</b> %1<br>").arg(obj.best_viewed);
        info += QString("<b>Imaged:</b> %1<br>").arg(obj.has_been_imaged ? "Yes ✓" : "No");
        info += QString("<br><i>%1</i>").arg(obj.description);
        
        objectInfoLabel->setText(info);
    }
    
    void autoAdjustFOV(const MessierObject& obj) {
        // Set FOV to 1.5x the object size for good framing
        double width = qMax(obj.size_arcmin.width() * 1.5, 10.0);
        double height = qMax(obj.size_arcmin.height() * 1.5, 10.0);
        
        // Cap at reasonable maximums
        width = qMin(width, 60.0);
        height = qMin(height, 60.0);
        
        widthSpinBox->setValue(width);
        heightSpinBox->setValue(height);
    }
    
    void onFetchObject() {
        if (currentObject.name.isEmpty()) {
            QMessageBox::warning(this, "No Selection", "Please select a Messier object first!");
            return;
        }
        
        fetchingComposite = false;
        statusLabel->setText(QString("Fetching DSS image for %1...").arg(currentObject.name));
        progressBar->show();
        setControlsEnabled(false);
        
        DSSurvey survey = (DSSurvey)surveyCombo->currentData().toInt();
        ImageFormat format = (ImageFormat)formatCombo->currentData().toInt();
        
        // Fetch using coordinates from catalog
        fetcher->fetchByCoordinates(currentObject.sky_position.ra_deg,
                                   currentObject.sky_position.dec_deg,
                                   widthSpinBox->value(),
                                   heightSpinBox->value(),
                                   survey,
                                   format);
    }
    
    void onFetchComposite() {
        if (currentObject.name.isEmpty()) {
            QMessageBox::warning(this, "No Selection", "Please select a Messier object first!");
            return;
        }
        
        fetchingComposite = true;
        compositeFetchCount = 0;
        irImage = QImage();
        redImage = QImage();
        blueImage = QImage();
        
        statusLabel->setText(QString("Fetching composite image for %1 (1/3: IR)...").arg(currentObject.name));
        progressBar->show();
        progressBar->setRange(0, 3);
        progressBar->setValue(0);
        setControlsEnabled(false);
        
        // Fetch IR first
        fetcher->fetchByCoordinates(currentObject.sky_position.ra_deg,
                                   currentObject.sky_position.dec_deg,
                                   widthSpinBox->value(),
                                   heightSpinBox->value(),
                                   DSSurvey::POSS2UKSTU_IR,
                                   ImageFormat::GIF);
    }
    
    void continueCompositeFetch() {
        compositeFetchCount++;
        progressBar->setValue(compositeFetchCount);
        
        if (compositeFetchCount == 1) {
            // Fetch Red next
            statusLabel->setText(QString("Fetching composite image for %1 (2/3: Red)...").arg(currentObject.name));
            fetcher->fetchByCoordinates(currentObject.sky_position.ra_deg,
                                       currentObject.sky_position.dec_deg,
                                       widthSpinBox->value(),
                                       heightSpinBox->value(),
                                       DSSurvey::POSS2UKSTU_RED,
                                       ImageFormat::GIF);
        } else if (compositeFetchCount == 2) {
            // Fetch Blue last
            statusLabel->setText(QString("Fetching composite image for %1 (3/3: Blue)...").arg(currentObject.name));
            fetcher->fetchByCoordinates(currentObject.sky_position.ra_deg,
                                       currentObject.sky_position.dec_deg,
                                       widthSpinBox->value(),
                                       heightSpinBox->value(),
                                       DSSurvey::POSS2UKSTU_BLUE,
                                       ImageFormat::GIF);
        } else if (compositeFetchCount == 3) {
            // All images fetched, create composite
            createFalseColorComposite();
        }
    }
    
    void createFalseColorComposite() {
        statusLabel->setText(QString("Creating false color composite for %1...").arg(currentObject.name));
        
        // Ensure all images are the same size
        if (irImage.isNull() || redImage.isNull() || blueImage.isNull()) {
            QMessageBox::critical(this, "Error", "Failed to fetch all required images for composite!");
            progressBar->hide();
            setControlsEnabled(true);
            fetchingComposite = false;
            return;
        }
        
        // Get dimensions (use the smallest common size)
        int width = qMin(qMin(irImage.width(), redImage.width()), blueImage.width());
        int height = qMin(qMin(irImage.height(), redImage.height()), blueImage.height());
        
        // Scale all images to same size if needed
        if (irImage.size() != QSize(width, height)) {
            irImage = irImage.scaled(width, height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        }
        if (redImage.size() != QSize(width, height)) {
            redImage = redImage.scaled(width, height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        }
        if (blueImage.size() != QSize(width, height)) {
            blueImage = blueImage.scaled(width, height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        }
        
        // Convert to RGB32 format for processing
        irImage = irImage.convertToFormat(QImage::Format_RGB32);
        redImage = redImage.convertToFormat(QImage::Format_RGB32);
        blueImage = blueImage.convertToFormat(QImage::Format_RGB32);
        
        // Create composite image: R=IR, G=Red, B=Blue
        QImage composite(width, height, QImage::Format_RGB32);
        
        for (int y = 0; y < height; ++y) {
            QRgb* compositeLine = reinterpret_cast<QRgb*>(composite.scanLine(y));
            const QRgb* irLine = reinterpret_cast<const QRgb*>(irImage.constScanLine(y));
            const QRgb* redLine = reinterpret_cast<const QRgb*>(redImage.constScanLine(y));
            const QRgb* blueLine = reinterpret_cast<const QRgb*>(blueImage.constScanLine(y));
            
            for (int x = 0; x < width; ++x) {
                // Extract grayscale values from each channel (using red channel as it's brightest in grayscale)
                int irValue = qRed(irLine[x]);
                int redValue = qRed(redLine[x]);
                int blueValue = qRed(blueLine[x]);
                
                // Create false color: R=IR, G=Red, B=Blue
                compositeLine[x] = qRgb(irValue, redValue, blueValue);
            }
        }
        
        // Display the composite
        currentImage = composite;
        
        // Convert to byte array for saving
        QBuffer buffer(&currentImageData);
        buffer.open(QIODevice::WriteOnly);
        composite.save(&buffer, "PNG");
        
        // Scale image to fit label
        QPixmap pixmap = QPixmap::fromImage(composite);
        imageLabel->setPixmap(pixmap.scaled(imageLabel->size(), 
                                           Qt::KeepAspectRatio, 
                                           Qt::SmoothTransformation));
        
        statusLabel->setText(QString("False color composite created for %1! (R=IR, G=Red, B=Blue) Size: %2×%3")
                            .arg(currentObject.name)
                            .arg(width)
                            .arg(height));
        statusLabel->setStyleSheet("QLabel { padding: 5px; background-color: #d4edda; color: #155724; }");
        
        progressBar->hide();
        progressBar->setRange(0, 0);
        setControlsEnabled(true);
        saveImageBtn->setEnabled(true);
        fetchingComposite = false;
    }
    
    void onImageReceived(const QImage& image, const QByteArray& rawData) {
        if (fetchingComposite) {
            // Store image based on fetch count
            if (compositeFetchCount == 0) {
                irImage = image;
            } else if (compositeFetchCount == 1) {
                redImage = image;
            } else if (compositeFetchCount == 2) {
                blueImage = image;
            }
            
            // Continue to next image or create composite
            continueCompositeFetch();
            return;
        }
        
        // Normal single image fetch
        currentImage = image;
        currentImageData = rawData;
        
        // Scale image to fit label while maintaining aspect ratio
        QPixmap pixmap = QPixmap::fromImage(image);
        imageLabel->setPixmap(pixmap.scaled(imageLabel->size(), 
                                           Qt::KeepAspectRatio, 
                                           Qt::SmoothTransformation));
        
        statusLabel->setText(QString("%1 loaded successfully! Size: %2×%3 pixels")
                            .arg(currentObject.name)
                            .arg(image.width())
                            .arg(image.height()));
        statusLabel->setStyleSheet("QLabel { padding: 5px; background-color: #d4edda; color: #155724; }");
        
        progressBar->hide();
        setControlsEnabled(true);
        saveImageBtn->setEnabled(true);
    }
    
    void onFitsReceived(const QByteArray& fitsData) {
        currentImageData = fitsData;
        
        imageLabel->setText(QString("FITS data received for %1\n%2 bytes\n\nUse 'Save Image' to save FITS file")
                           .arg(currentObject.name)
                           .arg(fitsData.size()));
        
        statusLabel->setText(QString("FITS data loaded: %1 bytes").arg(fitsData.size()));
        statusLabel->setStyleSheet("QLabel { padding: 5px; background-color: #d4edda; color: #155724; }");
        
        progressBar->hide();
        setControlsEnabled(true);
        saveImageBtn->setEnabled(true);
    }
    
    void onError(const QString& error) {
        if (fetchingComposite) {
            statusLabel->setText(QString("Error fetching composite for %1: %2")
                                .arg(currentObject.name)
                                .arg(error));
            fetchingComposite = false;
        } else {
            statusLabel->setText(QString("Error fetching %1: %2")
                                .arg(currentObject.name)
                                .arg(error));
        }
        
        statusLabel->setStyleSheet("QLabel { padding: 5px; background-color: #f8d7da; color: #721c24; }");
        
        QMessageBox::critical(this, "Fetch Error", error);
        
        progressBar->hide();
        progressBar->setRange(0, 0);
        setControlsEnabled(true);
    }
    
    void onSaveImage() {
        if (currentImageData.isEmpty()) {
            QMessageBox::warning(this, "No Data", "No image data to save!");
            return;
        }
        
        QString filter;
        QString defaultName = currentObject.name.replace(" ", "_");
        
        if (fetchingComposite || formatCombo->currentData().toInt() == (int)ImageFormat::GIF) {
            filter = "PNG Images (*.png);;GIF Images (*.gif);;All Files (*)";
            defaultName += "_composite.png";
        } else {
            filter = "FITS Files (*.fits);;All Files (*)";
            defaultName += ".fits";
        }
        
        QString fileName = QFileDialog::getSaveFileName(this,
                                                       "Save DSS Image",
                                                       defaultName,
                                                       filter);
        
        if (!fileName.isEmpty()) {
            if (fetcher->saveImage(currentImageData, fileName)) {
                statusLabel->setText(QString("Image saved to: %1").arg(fileName));
                QMessageBox::information(this, "Success", 
                    QString("DSS image of %1 saved successfully!").arg(currentObject.name));
            } else {
                statusLabel->setText("Failed to save image");
                QMessageBox::critical(this, "Error", "Failed to save image file!");
            }
        }
    }
    
    void setControlsEnabled(bool enabled) {
        fetchObjectBtn->setEnabled(enabled);
        fetchCompositeBtn->setEnabled(enabled);
        messierObjectCombo->setEnabled(enabled);
        messierObjectList->setEnabled(enabled);
        imagedOnlyCheckbox->setEnabled(enabled);
        filterTypeCombo->setEnabled(enabled);
        widthSpinBox->setEnabled(enabled);
        heightSpinBox->setEnabled(enabled);
        surveyCombo->setEnabled(enabled);
        formatCombo->setEnabled(enabled);
    }
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    // Create and show main window
    DSSViewerWindow window;
    window.show();
    
    return app.exec();
}

#include "main.moc"
