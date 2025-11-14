// main.cpp - Complete example using DSS Image Fetcher
#include "DSSFetcher.h"
#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QMessageBox>
#include <QFileDialog>
#include <QProgressBar>

class DSSViewerWindow : public QWidget {
    Q_OBJECT

private:
    DSSImageFetcher* fetcher;
    QLabel* imageLabel;
    QLabel* statusLabel;
    QProgressBar* progressBar;
    
    // Input controls
    QLineEdit* objectNameEdit;
    QDoubleSpinBox* raSpinBox;
    QDoubleSpinBox* decSpinBox;
    QDoubleSpinBox* widthSpinBox;
    QDoubleSpinBox* heightSpinBox;
    QComboBox* surveyCombo;
    QComboBox* formatCombo;
    
    QPushButton* fetchByNameBtn;
    QPushButton* fetchByCoordsBtn;
    QPushButton* saveImageBtn;
    
    QByteArray currentImageData;
    QImage currentImage;

public:
    DSSViewerWindow(QWidget* parent = nullptr) : QWidget(parent) {
        setWindowTitle("DSS Image Fetcher - Astronomy Viewer");
        resize(1000, 800);
        
        // Create fetcher
        fetcher = new DSSImageFetcher(this);
        
        // Setup UI
        setupUI();
        
        // Connect signals
        connectSignals();
        
        // Set default values
        setDefaults();
    }

private:
    void setupUI() {
        QVBoxLayout* mainLayout = new QVBoxLayout(this);
        
        // Controls section
        QGroupBox* controlsGroup = new QGroupBox("Fetch Controls");
        QVBoxLayout* controlsLayout = new QVBoxLayout(controlsGroup);
        
        // Object name input
        QHBoxLayout* nameLayout = new QHBoxLayout();
        nameLayout->addWidget(new QLabel("Object Name:"));
        objectNameEdit = new QLineEdit();
        objectNameEdit->setPlaceholderText("e.g., M51, NGC1234, M42");
        nameLayout->addWidget(objectNameEdit);
        fetchByNameBtn = new QPushButton("Fetch by Name");
        nameLayout->addWidget(fetchByNameBtn);
        controlsLayout->addLayout(nameLayout);
        
        // Coordinate inputs
        QHBoxLayout* coordLayout = new QHBoxLayout();
        coordLayout->addWidget(new QLabel("RA (deg):"));
        raSpinBox = new QDoubleSpinBox();
        raSpinBox->setRange(0.0, 360.0);
        raSpinBox->setDecimals(6);
        coordLayout->addWidget(raSpinBox);
        
        coordLayout->addWidget(new QLabel("Dec (deg):"));
        decSpinBox = new QDoubleSpinBox();
        decSpinBox->setRange(-90.0, 90.0);
        decSpinBox->setDecimals(6);
        coordLayout->addWidget(decSpinBox);
        
        fetchByCoordsBtn = new QPushButton("Fetch by Coordinates");
        coordLayout->addWidget(fetchByCoordsBtn);
        controlsLayout->addLayout(coordLayout);
        
        // Field of view
        QHBoxLayout* fovLayout = new QHBoxLayout();
        fovLayout->addWidget(new QLabel("Width (arcmin):"));
        widthSpinBox = new QDoubleSpinBox();
        widthSpinBox->setRange(1.0, 60.0);
        widthSpinBox->setValue(15.0);
        widthSpinBox->setDecimals(1);
        fovLayout->addWidget(widthSpinBox);
        
        fovLayout->addWidget(new QLabel("Height (arcmin):"));
        heightSpinBox = new QDoubleSpinBox();
        heightSpinBox->setRange(1.0, 60.0);
        heightSpinBox->setValue(15.0);
        heightSpinBox->setDecimals(1);
        fovLayout->addWidget(heightSpinBox);
        controlsLayout->addLayout(fovLayout);
        
        // Survey and format selection
        QHBoxLayout* optionsLayout = new QHBoxLayout();
        optionsLayout->addWidget(new QLabel("Survey:"));
        surveyCombo = new QComboBox();
        surveyCombo->addItem("POSS2/UKSTU Red", (int)DSSurvey::POSS2UKSTU_RED);
        surveyCombo->addItem("POSS2/UKSTU Blue", (int)DSSurvey::POSS2UKSTU_BLUE);
        surveyCombo->addItem("POSS2/UKSTU IR", (int)DSSurvey::POSS2UKSTU_IR);
        surveyCombo->addItem("POSS1 Red", (int)DSSurvey::POSS1_RED);
        surveyCombo->addItem("POSS1 Blue", (int)DSSurvey::POSS1_BLUE);
        surveyCombo->addItem("Quick-V", (int)DSSurvey::QUICKV);
        optionsLayout->addWidget(surveyCombo);
        
        optionsLayout->addWidget(new QLabel("Format:"));
        formatCombo = new QComboBox();
        formatCombo->addItem("GIF (Display)", (int)ImageFormat::GIF);
        formatCombo->addItem("FITS (Science)", (int)ImageFormat::FITS);
        optionsLayout->addWidget(formatCombo);
        controlsLayout->addLayout(optionsLayout);
        
        mainLayout->addWidget(controlsGroup);
        
        // Progress bar
        progressBar = new QProgressBar();
        progressBar->setRange(0, 0); // Indeterminate
        progressBar->hide();
        mainLayout->addWidget(progressBar);
        
        // Status label
        statusLabel = new QLabel("Ready to fetch DSS images");
        statusLabel->setStyleSheet("QLabel { padding: 5px; background-color: #f0f0f0; }");
        mainLayout->addWidget(statusLabel);
        
        // Image display
        QGroupBox* imageGroup = new QGroupBox("Image Display");
        QVBoxLayout* imageLayout = new QVBoxLayout(imageGroup);
        
        imageLabel = new QLabel();
        imageLabel->setMinimumSize(600, 600);
        imageLabel->setAlignment(Qt::AlignCenter);
        imageLabel->setStyleSheet("QLabel { background-color: black; color: white; }");
        imageLabel->setText("No image loaded\nFetch an image to display");
        imageLayout->addWidget(imageLabel);
        
        // Save button
        QHBoxLayout* saveLayout = new QHBoxLayout();
        saveImageBtn = new QPushButton("Save Image");
        saveImageBtn->setEnabled(false);
        saveLayout->addStretch();
        saveLayout->addWidget(saveImageBtn);
        imageLayout->addLayout(saveLayout);
        
        mainLayout->addWidget(imageGroup);
        
        setLayout(mainLayout);
    }
    
    void connectSignals() {
        // Fetch buttons
        connect(fetchByNameBtn, &QPushButton::clicked, this, &DSSViewerWindow::onFetchByName);
        connect(fetchByCoordsBtn, &QPushButton::clicked, this, &DSSViewerWindow::onFetchByCoords);
        connect(saveImageBtn, &QPushButton::clicked, this, &DSSViewerWindow::onSaveImage);
        
        // Allow Enter key in object name field
        connect(objectNameEdit, &QLineEdit::returnPressed, this, &DSSViewerWindow::onFetchByName);
        
        // Fetcher signals
        connect(fetcher, &DSSImageFetcher::imageReceived, this, &DSSViewerWindow::onImageReceived);
        connect(fetcher, &DSSImageFetcher::fitsDataReceived, this, &DSSViewerWindow::onFitsReceived);
        connect(fetcher, &DSSImageFetcher::errorOccurred, this, &DSSViewerWindow::onError);
    }
    
    void setDefaults() {
        // Set M42 (Orion Nebula) coordinates as example
        objectNameEdit->setText("M42");
        raSpinBox->setValue(83.8221);
        decSpinBox->setValue(-5.3911);
        widthSpinBox->setValue(20.0);
        heightSpinBox->setValue(20.0);
    }

private slots:
    void onFetchByName() {
        QString objName = objectNameEdit->text().trimmed();
        if (objName.isEmpty()) {
            QMessageBox::warning(this, "Input Error", "Please enter an object name!");
            return;
        }
        
        statusLabel->setText(QString("Fetching image for: %1...").arg(objName));
        progressBar->show();
        setControlsEnabled(false);
        
        DSSurvey survey = (DSSurvey)surveyCombo->currentData().toInt();
        ImageFormat format = (ImageFormat)formatCombo->currentData().toInt();
        
        fetcher->fetchByObjectName(objName, 
                                   widthSpinBox->value(),
                                   heightSpinBox->value(),
                                   survey,
                                   format);
    }
    
    void onFetchByCoords() {
        double ra = raSpinBox->value();
        double dec = decSpinBox->value();
        
        statusLabel->setText(QString("Fetching image at RA=%1°, Dec=%2°...")
                            .arg(ra, 0, 'f', 4)
                            .arg(dec, 0, 'f', 4));
        progressBar->show();
        setControlsEnabled(false);
        
        DSSurvey survey = (DSSurvey)surveyCombo->currentData().toInt();
        ImageFormat format = (ImageFormat)formatCombo->currentData().toInt();
        
        fetcher->fetchByCoordinates(ra, dec,
                                   widthSpinBox->value(),
                                   heightSpinBox->value(),
                                   survey,
                                   format);
    }
    
    void onImageReceived(const QImage& image, const QByteArray& rawData) {
        currentImage = image;
        currentImageData = rawData;
        
        // Scale image to fit label while maintaining aspect ratio
        QPixmap pixmap = QPixmap::fromImage(image);
        imageLabel->setPixmap(pixmap.scaled(imageLabel->size(), 
                                           Qt::KeepAspectRatio, 
                                           Qt::SmoothTransformation));
        
        statusLabel->setText(QString("Image loaded successfully! Size: %1x%2 pixels")
                            .arg(image.width())
                            .arg(image.height()));
        statusLabel->setStyleSheet("QLabel { padding: 5px; background-color: #d4edda; color: #155724; }");
        
        progressBar->hide();
        setControlsEnabled(true);
        saveImageBtn->setEnabled(true);
    }
    
    void onFitsReceived(const QByteArray& fitsData) {
        currentImageData = fitsData;
        
        imageLabel->setText(QString("FITS data received\n%1 bytes\n\nUse 'Save Image' to save FITS file")
                           .arg(fitsData.size()));
        
        statusLabel->setText(QString("FITS data loaded: %1 bytes").arg(fitsData.size()));
        statusLabel->setStyleSheet("QLabel { padding: 5px; background-color: #d4edda; color: #155724; }");
        
        progressBar->hide();
        setControlsEnabled(true);
        saveImageBtn->setEnabled(true);
    }
    
    void onError(const QString& error) {
        statusLabel->setText(QString("Error: %1").arg(error));
        statusLabel->setStyleSheet("QLabel { padding: 5px; background-color: #f8d7da; color: #721c24; }");
        
        QMessageBox::critical(this, "Fetch Error", error);
        
        progressBar->hide();
        setControlsEnabled(true);
    }
    
    void onSaveImage() {
        if (currentImageData.isEmpty()) {
            QMessageBox::warning(this, "No Data", "No image data to save!");
            return;
        }
        
        QString filter;
        QString defaultExt;
        
        if (formatCombo->currentData().toInt() == (int)ImageFormat::FITS) {
            filter = "FITS Files (*.fits);;All Files (*)";
            defaultExt = ".fits";
        } else {
            filter = "GIF Images (*.gif);;PNG Images (*.png);;All Files (*)";
            defaultExt = ".gif";
        }
        
        QString fileName = QFileDialog::getSaveFileName(this,
                                                       "Save DSS Image",
                                                       "dss_image" + defaultExt,
                                                       filter);
        
        if (!fileName.isEmpty()) {
            if (fetcher->saveImage(currentImageData, fileName)) {
                statusLabel->setText(QString("Image saved to: %1").arg(fileName));
                QMessageBox::information(this, "Success", "Image saved successfully!");
            } else {
                statusLabel->setText("Failed to save image");
                QMessageBox::critical(this, "Error", "Failed to save image file!");
            }
        }
    }
    
    void setControlsEnabled(bool enabled) {
        fetchByNameBtn->setEnabled(enabled);
        fetchByCoordsBtn->setEnabled(enabled);
        objectNameEdit->setEnabled(enabled);
        raSpinBox->setEnabled(enabled);
        decSpinBox->setEnabled(enabled);
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
    
    // Example: Uncomment to auto-fetch on startup
    // QTimer::singleShot(500, &window, [&window]() {
    //     window.findChild<QPushButton*>("fetchByNameBtn")->click();
    // });
    
    return app.exec();
}

#include "main.moc"
