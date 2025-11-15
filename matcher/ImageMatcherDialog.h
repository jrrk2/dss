#ifndef IMAGEMATCHERDIALOG_H
#define IMAGEMATCHERDIALOG_H

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QBuffer>
#include <QProgressBar>
#include <QTableWidget>
#include <QHeaderView>
#include <QGroupBox>
#include <QMessageBox>
#include "FitsProcessor.h"

class ImageMatcherDialog : public QDialog {
    Q_OBJECT

private:
    QLabel* userImageLabel;
    QLabel* libraryImageLabel;
    QLabel* statusLabel;
    QProgressBar* progressBar;
    QTableWidget* analysisTable;
    QPushButton* applyBackgroundBtn;
    
    std::vector<float> userData;
    std::vector<float> libraryData;
    int userWidth, userHeight;
    int libWidth, libHeight;
    WCSInfo userWCS;
    WCSInfo libraryWCS;
    BackgroundGradient userBG;
    PSFModel userPSF;
    PSFModel libraryPSF;
    
    FitsProcessor* processor;

public:
    ImageMatcherDialog(const QString& userFitsPath, 
                      const QByteArray& libraryFitsData,
                      QWidget* parent = nullptr) 
        : QDialog(parent) {
        
        setWindowTitle("Image Matcher - WCS Alignment & Analysis");
        resize(1400, 800);
        
        processor = new FitsProcessor(this);
        
        setupUI();
        
        // Load both images
        loadImages(userFitsPath, libraryFitsData);
        
        // Perform analysis
        analyzeImages();
    }

private:
    void setupUI() {
        QVBoxLayout* mainLayout = new QVBoxLayout(this);
        
        // Status bar
        statusLabel = new QLabel("Loading images...");
        statusLabel->setStyleSheet("QLabel { padding: 5px; background-color: #f0f0f0; }");
        mainLayout->addWidget(statusLabel);
        
        progressBar = new QProgressBar();
        progressBar->setRange(0, 0);
        mainLayout->addWidget(progressBar);
        
        // Image comparison
        QHBoxLayout* imageLayout = new QHBoxLayout();
        
        QGroupBox* userGroup = new QGroupBox("Your FITS Image");
        QVBoxLayout* userLayout = new QVBoxLayout(userGroup);
        userImageLabel = new QLabel();
        userImageLabel->setMinimumSize(600, 600);
        userImageLabel->setAlignment(Qt::AlignCenter);
        userImageLabel->setStyleSheet("QLabel { background-color: black; }");
        userLayout->addWidget(userImageLabel);
        imageLayout->addWidget(userGroup);
        
        QGroupBox* libGroup = new QGroupBox("DSS Library Image");
        QVBoxLayout* libLayout = new QVBoxLayout(libGroup);
        libraryImageLabel = new QLabel();
        libraryImageLabel->setMinimumSize(600, 600);
        libraryImageLabel->setAlignment(Qt::AlignCenter);
        libraryImageLabel->setStyleSheet("QLabel { background-color: black; }");
        libLayout->addWidget(libraryImageLabel);
        imageLayout->addWidget(libGroup);
        
        mainLayout->addLayout(imageLayout);
        
        // Analysis results
        QGroupBox* analysisGroup = new QGroupBox("Analysis Results");
        QVBoxLayout* analysisLayout = new QVBoxLayout(analysisGroup);
        
        analysisTable = new QTableWidget();
        analysisTable->setColumnCount(3);
        analysisTable->setHorizontalHeaderLabels({"Parameter", "Your Image", "Library Image"});
        analysisTable->horizontalHeader()->setStretchLastSection(true);
        analysisTable->setAlternatingRowColors(true);
        analysisTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        analysisLayout->addWidget(analysisTable);
        
        mainLayout->addWidget(analysisGroup);
        
        // Action buttons
        QHBoxLayout* buttonLayout = new QHBoxLayout();
        
        applyBackgroundBtn = new QPushButton("Apply Background Correction");
        applyBackgroundBtn->setEnabled(false);
        buttonLayout->addWidget(applyBackgroundBtn);
        
        QPushButton* closeBtn = new QPushButton("Close");
        connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
        buttonLayout->addWidget(closeBtn);
        
        buttonLayout->addStretch();
        mainLayout->addLayout(buttonLayout);
        
        connect(applyBackgroundBtn, &QPushButton::clicked, 
                this, &ImageMatcherDialog::applyBackgroundCorrection);
    }
    
    void displayImageFits(const QByteArray &fitsData, QLabel* label) {
      /*        
      */				      
	if (fitsData.isEmpty())
	    return;

	fitsfile *fptr = nullptr;
	int status = 0;

	// CFITSIO requires a non-const memory pointer
	QByteArray mutableData = fitsData;
	size_t memsize = mutableData.size();
	void *(*mem_realloc)(void *p, size_t newsize);
	char *data = mutableData.data();

	// Open FITS from memory
	if (fits_open_memfile(&fptr,
			      "memory.fits",
			      READONLY,
			      (void**)&data,
			      &memsize,
			      0,
			      mem_realloc,
			      &status))
	{
	    fits_report_error(stderr, status);
	    return;
	}

	// Ensure it's an image HDU
	int hdutype = 0;
	fits_get_hdu_type(fptr, &hdutype, &status);
	if (hdutype != IMAGE_HDU)
	{
	    fits_close_file(fptr, &status);
	    return;
	}

	// Read image parameters
	int bitpix = 0, naxis = 0;
	long naxes[3] = {1, 1, 1};

	fits_get_img_param(fptr, 3, &bitpix, &naxis, naxes, &status);

	if (naxis < 2)   // must be at least 2D
	{
	    fits_close_file(fptr, &status);
	    return;
	}

	const int width  = naxes[0];
	const int height = naxes[1];

	// --- Read pixel data into a float buffer ---
	const long npixels = width * height;
	std::vector<float> buffer(npixels);

	long fpixel[3] = {1, 1, 1};
	if (fits_read_pix(fptr, TFLOAT, fpixel, npixels, NULL, buffer.data(), NULL, &status))
	{
	    fits_report_error(stderr, status);
	    fits_close_file(fptr, &status);
	    return;
	}

	fits_close_file(fptr, &status);

	// --- Compute min/max for scaling ---
	auto [minIt, maxIt] = std::minmax_element(buffer.begin(), buffer.end());
	float minVal = *minIt;
	float maxVal = *maxIt;

	if (minVal == maxVal)
	    maxVal = minVal + 1.0;   // avoid divide-by-zero

	const float scale = 255.0f / (maxVal - minVal);

	// --- Create an 8-bit grayscale QImage ---
	QImage img(width, height, QImage::Format_Grayscale8);

	for (int y = 0; y < height; ++y)
	{
	    uchar *scan = img.scanLine(y);
	    for (int x = 0; x < width; ++x)
	    {
		float v = buffer[y * width + x];
		int scaled = int((v - minVal) * scale + 0.5f);
		if (scaled < 0)   scaled = 0;
		if (scaled > 255) scaled = 255;

		scan[x] = uchar(scaled);
	    }
	}

        // Mirror vertically for FITS orientation
        img = img.mirrored(false, true);
        
        QPixmap pixmap = QPixmap::fromImage(img);
        label->setPixmap(pixmap.scaled(label->size(), 
                                      Qt::KeepAspectRatio,
                                      Qt::SmoothTransformation));
    }

    void loadImages(const QString& userPath, const QByteArray& libraryData) {
        statusLabel->setText("Loading user FITS image...");
        
        // Load user image
        if (!processor->loadFits(userPath, userData, userWidth, userHeight, userWCS)) {
            QMessageBox::critical(this, "Error", "Failed to load user FITS file!");
            return;
        }

        displayImage(userData, userWidth, userHeight, userImageLabel);

        statusLabel->setText("Loading library FITS image...");
        
        // Load library image from memory
        if (!loadFitsFromMemory(libraryData)) {
            QMessageBox::critical(this, "Error", "Failed to load library FITS data!");
            return;
        }

        displayImageFits(libraryData, libraryImageLabel);

        progressBar->hide();
        statusLabel->setText("Images loaded successfully");
    }

    bool loadFitsFromMemory(const QByteArray& fitsData) {
        fitsfile* fptr = nullptr;
        int status = 0;
        
        QByteArray mutableData = fitsData;
        size_t memsize = mutableData.size();
        void* (*mem_realloc)(void*, size_t) = nullptr;
        char* data = mutableData.data();

        if (fits_open_memfile(&fptr, "memory.fits", READONLY,
                             (void**)&data, &memsize, 0, mem_realloc, &status)) {
            return false;
        }
        
        int naxis = 0;
        long naxes[3] = {1, 1, 1};
        fits_get_img_dim(fptr, &naxis, &status);
        fits_get_img_size(fptr, 3, naxes, &status);
        
        if (naxis < 2) {
            fits_close_file(fptr, &status);
            return false;
        }
        
        libWidth = naxes[0];
        libHeight = naxes[1];
        
        libraryWCS = processor->readWCS(fptr);
        
        long npixels = libWidth * libHeight;
        libraryData.resize(npixels);
        
        long fpixel[3] = {1, 1, 1};
        if (fits_read_pix(fptr, TFLOAT, fpixel, npixels, nullptr,
                         libraryData.data(), nullptr, &status)) {
            fits_close_file(fptr, &status);
            return false;
        }
        
        fits_close_file(fptr, &status);
        return true;
    }
    
    void displayImage(const std::vector<float>& data, int width, int height, 
                     QLabel* label) {
        if (data.empty()) return;
        
        // Find min/max for scaling
        auto [minIt, maxIt] = std::minmax_element(data.begin(), data.end());
        float minVal = *minIt;
        float maxVal = *maxIt;
        float scale = 255.0f / (maxVal - minVal);
        
        QImage img(width, height, QImage::Format_Grayscale8);
        
        for (int y = 0; y < height; ++y) {
            uchar* scanLine = img.scanLine(y);
            for (int x = 0; x < width; ++x) {
                float val = data[y * width + x];
                int scaled = (val - minVal) * scale;
                scaled = qBound(0, scaled, 255);
                scanLine[x] = scaled;
            }
        }
        
        // Mirror vertically for FITS orientation
        img = img.mirrored(false, true);
        
        QPixmap pixmap = QPixmap::fromImage(img);
        label->setPixmap(pixmap.scaled(label->size(), 
                                      Qt::KeepAspectRatio,
                                      Qt::SmoothTransformation));
    }
    
    void analyzeImages() {
        statusLabel->setText("Analyzing images...");
        progressBar->show();
        
        // Calculate background gradients
        userBG = processor->calculateBackgroundGradient(userData, userWidth, userHeight);
        
        // Estimate PSFs
        userPSF = processor->estimatePSF(userData, userWidth, userHeight);
        libraryPSF = processor->estimatePSF(libraryData, libWidth, libHeight);
        
        // Populate analysis table
        populateAnalysisTable();
        
        progressBar->hide();
        statusLabel->setText("Analysis complete");
        statusLabel->setStyleSheet("QLabel { padding: 5px; background-color: #d4edda; }");
        
        applyBackgroundBtn->setEnabled(true);
    }
    
    void populateAnalysisTable() {
        analysisTable->setRowCount(0);
        
        auto addRow = [this](const QString& param, const QString& userVal, 
                            const QString& libVal) {
            int row = analysisTable->rowCount();
            analysisTable->insertRow(row);
            analysisTable->setItem(row, 0, new QTableWidgetItem(param));
            analysisTable->setItem(row, 1, new QTableWidgetItem(userVal));
            analysisTable->setItem(row, 2, new QTableWidgetItem(libVal));
        };
        
        // Image dimensions
        addRow("Dimensions (WxH)", 
               QString("%1 × %2 px").arg(userWidth).arg(userHeight),
               QString("%1 × %2 px").arg(libWidth).arg(libHeight));
        
        // WCS information
        if (userWCS.isValid) {
            addRow("Center RA", 
                   QString("%1°").arg(userWCS.crval1, 0, 'f', 6),
                   QString("%1°").arg(libraryWCS.crval1, 0, 'f', 6));
            addRow("Center Dec",
                   QString("%1°").arg(userWCS.crval2, 0, 'f', 6),
                   QString("%1°").arg(libraryWCS.crval2, 0, 'f', 6));
            addRow("Pixel Scale X",
                   QString("%1 arcsec/px").arg(std::abs(userWCS.cdelt1) * 3600, 0, 'f', 3),
                   QString("%1 arcsec/px").arg(std::abs(libraryWCS.cdelt1) * 3600, 0, 'f', 3));
            addRow("Pixel Scale Y",
                   QString("%1 arcsec/px").arg(std::abs(userWCS.cdelt2) * 3600, 0, 'f', 3),
                   QString("%1 arcsec/px").arg(std::abs(libraryWCS.cdelt2) * 3600, 0, 'f', 3));
            addRow("Rotation",
                   QString("%1°").arg(userWCS.crota2, 0, 'f', 2),
                   QString("%1°").arg(libraryWCS.crota2, 0, 'f', 2));
        } else {
            addRow("WCS Info", "No valid WCS found", 
                   libraryWCS.isValid ? "Valid" : "No valid WCS");
        }
        
        // Background gradient
        addRow("Background Model", "2D Quadratic", "—");
        addRow("Background RMS",
               QString("%1").arg(userBG.rms, 0, 'f', 2),
               "—");
        addRow("Gradient Coefficient a",
               QString("%1e").arg(userBG.a, 0, 'e', 3),
               "—");
        addRow("Gradient Coefficient b",
               QString("%1e").arg(userBG.b, 0, 'e', 3),
               "—");
        
        // PSF information
        if (userPSF.fwhm > 0) {
            addRow("PSF FWHM",
                   QString("%1 px").arg(userPSF.fwhm, 0, 'f', 2),
                   libraryPSF.fwhm > 0 ? 
                       QString("%1 px").arg(libraryPSF.fwhm, 0, 'f', 2) : "—");
            
            if (userWCS.isValid) {
                double arcsecPerPx = std::abs(userWCS.cdelt1) * 3600;
                double fwhm_arcsec = userPSF.fwhm * arcsecPerPx;
                addRow("PSF FWHM (arcsec)",
                       QString("%1\"").arg(fwhm_arcsec, 0, 'f', 2),
                       "—");
            }
            
            addRow("PSF Sigma",
                   QString("%1 px").arg(userPSF.sigma, 0, 'f', 2),
                   libraryPSF.sigma > 0 ?
                       QString("%1 px").arg(libraryPSF.sigma, 0, 'f', 2) : "—");
        }
        
        analysisTable->resizeColumnsToContents();
    }
    
    void applyBackgroundCorrection() {
        statusLabel->setText("Applying background correction...");
        progressBar->show();
        
        // Create corrected image
        std::vector<float> correctedData = userData;
        
        for (int y = 0; y < userHeight; ++y) {
            for (int x = 0; x < userWidth; ++x) {
                int idx = y * userWidth + x;
                float bgValue = userBG.evaluate(x, y);
                correctedData[idx] -= bgValue;
            }
        }
        
        // Display corrected image
        displayImage(correctedData, userWidth, userHeight, userImageLabel);
        
        progressBar->hide();
        statusLabel->setText("Background correction applied");
        
        QMessageBox::information(this, "Success", 
            "Background gradient removed from your image.\n"
            "RMS of background model: " + QString::number(userBG.rms, 'f', 2));
    }
};

#endif // IMAGEMATCHERDIALOG_H
