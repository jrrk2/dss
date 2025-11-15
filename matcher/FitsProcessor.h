#ifndef FITSPROCESSOR_H
#define FITSPROCESSOR_H

#include <QObject>
#include <QString>
#include <QImage>
#include <fitsio.h>
#include <vector>
#include <cmath>

// WCS coordinate structure
struct WCSInfo {
    double crval1;      // RA at reference pixel
    double crval2;      // Dec at reference pixel
    double crpix1;      // Reference pixel X
    double crpix2;      // Reference pixel Y
    double cdelt1;      // Degrees per pixel X
    double cdelt2;      // Degrees per pixel Y
    double crota2;      // Rotation angle
    QString ctype1;     // Coordinate type (RA---TAN)
    QString ctype2;     // Coordinate type (DEC--TAN)
    double equinox;     // Coordinate equinox (2000.0)
    
    bool isValid;
    
    WCSInfo() : crval1(0), crval2(0), crpix1(0), crpix2(0),
                cdelt1(0), cdelt2(0), crota2(0), equinox(2000.0),
                isValid(false) {}
    
    // Convert pixel coordinates to RA/Dec
    void pixelToWorld(double x, double y, double& ra, double& dec) const {
        if (!isValid) return;
        
        // Simple TAN projection (gnomonic)
        double dx = (x - crpix1) * cdelt1;
        double dy = (y - crpix2) * cdelt2;
        
        // Apply rotation if present
        double theta = crota2 * M_PI / 180.0;
        double dxr = dx * cos(theta) - dy * sin(theta);
        double dyr = dx * sin(theta) + dy * cos(theta);
        
        // Convert to RA/Dec
        double dec0 = crval2 * M_PI / 180.0;
        double ra0 = crval1 * M_PI / 180.0;
        
        double r = sqrt(dxr * dxr + dyr * dyr) * M_PI / 180.0;
        double theta_p = atan2(dxr, dyr);
        
        dec = asin(cos(r) * sin(dec0) + sin(r) * cos(dec0) * cos(theta_p));
        ra = ra0 + atan2(sin(r) * sin(theta_p), 
                        cos(r) * cos(dec0) - sin(r) * sin(dec0) * cos(theta_p));
        
        ra = ra * 180.0 / M_PI;
        dec = dec * 180.0 / M_PI;
        
        // Normalize RA to [0, 360)
        while (ra < 0) ra += 360.0;
        while (ra >= 360.0) ra -= 360.0;
    }
    
    // Convert RA/Dec to pixel coordinates
    void worldToPixel(double ra, double dec, double& x, double& y) const {
        if (!isValid) return;
        
        double ra0 = crval1 * M_PI / 180.0;
        double dec0 = crval2 * M_PI / 180.0;
        double ra_rad = ra * M_PI / 180.0;
        double dec_rad = dec * M_PI / 180.0;
        
        // TAN projection
        double denom = sin(dec0) * sin(dec_rad) + 
                      cos(dec0) * cos(dec_rad) * cos(ra_rad - ra0);
        
        double dx = cos(dec_rad) * sin(ra_rad - ra0) / denom;
        double dy = (cos(dec0) * sin(dec_rad) - 
                    sin(dec0) * cos(dec_rad) * cos(ra_rad - ra0)) / denom;
        
        // Convert to degrees
        dx = dx * 180.0 / M_PI;
        dy = dy * 180.0 / M_PI;
        
        // Apply rotation
        double theta = -crota2 * M_PI / 180.0;
        double dxr = dx * cos(theta) - dy * sin(theta);
        double dyr = dx * sin(theta) + dy * cos(theta);
        
        // Convert to pixels
        x = crpix1 + dxr / cdelt1;
        y = crpix2 + dyr / cdelt2;
    }
};

// Background gradient model
struct BackgroundGradient {
    double a, b, c, d, e, f;  // 2D quadratic: z = ax^2 + by^2 + cxy + dx + ey + f
    double rms;               // RMS of residuals
    
    BackgroundGradient() : a(0), b(0), c(0), d(0), e(0), f(0), rms(0) {}
    
    double evaluate(double x, double y) const {
        return a*x*x + b*y*y + c*x*y + d*x + e*y + f;
    }
};

// Point Spread Function
struct PSFModel {
    double fwhm;           // Full Width Half Maximum in pixels
    double sigma;          // Gaussian sigma
    double ellipticity;    // PSF ellipticity
    double theta;          // Orientation angle
    double beta;           // Moffat beta parameter (if using Moffat)
    QString modelType;     // "gaussian" or "moffat"
    
    PSFModel() : fwhm(0), sigma(0), ellipticity(0), theta(0), 
                 beta(2.5), modelType("gaussian") {}
};

class FitsProcessor : public QObject {
    Q_OBJECT

public:
    explicit FitsProcessor(QObject* parent = nullptr) : QObject(parent) {}
    
    // Load FITS file and extract WCS
    bool loadFits(const QString& filename, 
                  std::vector<float>& imageData,
                  int& width, int& height,
                  WCSInfo& wcs) {
        
        fitsfile* fptr = nullptr;
        int status = 0;
        
        if (fits_open_file(&fptr, filename.toLocal8Bit().constData(), READONLY, &status)) {
            fits_report_error(stderr, status);
            return false;
        }
        
        // Read image dimensions
        int naxis = 0;
        long naxes[3] = {1, 1, 1};
        fits_get_img_dim(fptr, &naxis, &status);
        fits_get_img_size(fptr, 3, naxes, &status);
        
        if (naxis < 2) {
            fits_close_file(fptr, &status);
            return false;
        }
        
        width = naxes[0];
        height = naxes[1];
        
        // Read WCS header
        wcs = readWCS(fptr);
        
        // Read image data
        long npixels = width * height;
        imageData.resize(npixels);
        
        long fpixel[3] = {1, 1, 1};
        if (fits_read_pix(fptr, TFLOAT, fpixel, npixels, nullptr, 
                         imageData.data(), nullptr, &status)) {
            fits_report_error(stderr, status);
            fits_close_file(fptr, &status);
            return false;
        }
        
        fits_close_file(fptr, &status);
        return true;
    }
    
    // Extract WCS from FITS header
    WCSInfo readWCS(fitsfile* fptr) {
        WCSInfo wcs;
        int status = 0;
        char comment[FLEN_COMMENT];
        
        fits_read_key(fptr, TDOUBLE, "CRVAL1", &wcs.crval1, comment, &status);
        if (status) { status = 0; wcs.crval1 = 0; }
        
        fits_read_key(fptr, TDOUBLE, "CRVAL2", &wcs.crval2, comment, &status);
        if (status) { status = 0; wcs.crval2 = 0; }
        
        fits_read_key(fptr, TDOUBLE, "CRPIX1", &wcs.crpix1, comment, &status);
        if (status) { status = 0; wcs.crpix1 = 0; }
        
        fits_read_key(fptr, TDOUBLE, "CRPIX2", &wcs.crpix2, comment, &status);
        if (status) { status = 0; wcs.crpix2 = 0; }
        
        fits_read_key(fptr, TDOUBLE, "CDELT1", &wcs.cdelt1, comment, &status);
        if (status) { status = 0; wcs.cdelt1 = 0; }
        
        fits_read_key(fptr, TDOUBLE, "CDELT2", &wcs.cdelt2, comment, &status);
        if (status) { status = 0; wcs.cdelt2 = 0; }
        
        fits_read_key(fptr, TDOUBLE, "CROTA2", &wcs.crota2, comment, &status);
        if (status) { status = 0; wcs.crota2 = 0; }
        
        fits_read_key(fptr, TDOUBLE, "EQUINOX", &wcs.equinox, comment, &status);
        if (status) { status = 0; wcs.equinox = 2000.0; }
        
        char ctype1[FLEN_VALUE], ctype2[FLEN_VALUE];
        fits_read_key(fptr, TSTRING, "CTYPE1", ctype1, comment, &status);
        if (status) { status = 0; strcpy(ctype1, "RA---TAN"); }
        
        fits_read_key(fptr, TSTRING, "CTYPE2", ctype2, comment, &status);
        if (status) { status = 0; strcpy(ctype2, "DEC--TAN"); }
        
        wcs.ctype1 = QString(ctype1);
        wcs.ctype2 = QString(ctype2);
        
        wcs.isValid = (wcs.crval1 != 0 || wcs.crval2 != 0) && 
                     (wcs.cdelt1 != 0 && wcs.cdelt2 != 0);
        
        return wcs;
    }
    
    // Calculate background gradient using robust polynomial fitting
    BackgroundGradient calculateBackgroundGradient(const std::vector<float>& data,
                                                   int width, int height,
                                                   int gridSize = 50) {
        BackgroundGradient bg;
        
        // Sample background on a grid, excluding bright sources
        std::vector<double> xSamples, ySamples, zSamples;
        
        // Calculate median and MAD for outlier rejection
        std::vector<float> sortedData = data;
        std::nth_element(sortedData.begin(), 
                        sortedData.begin() + sortedData.size()/2, 
                        sortedData.end());
        float median = sortedData[sortedData.size()/2];
        
        for (float& val : sortedData) val = std::abs(val - median);
        std::nth_element(sortedData.begin(), 
                        sortedData.begin() + sortedData.size()/2, 
                        sortedData.end());
        float mad = sortedData[sortedData.size()/2] * 1.4826f;
        
        // Sample grid points
        for (int y = 0; y < height; y += gridSize) {
            for (int x = 0; x < width; x += gridSize) {
                int idx = y * width + x;
                float val = data[idx];
                
                // Reject outliers (likely stars)
                if (std::abs(val - median) < 3.0 * mad) {
                    xSamples.push_back(x / (double)width);
                    ySamples.push_back(y / (double)height);
                    zSamples.push_back(val);
                }
            }
        }
        
        // Fit 2D quadratic using least squares
        int n = xSamples.size();
        if (n < 6) return bg;
        
        // Build design matrix for z = ax^2 + by^2 + cxy + dx + ey + f
        std::vector<std::vector<double>> A(n, std::vector<double>(6));
        std::vector<double> b_vec(n);
        
        for (int i = 0; i < n; ++i) {
            double x = xSamples[i];
            double y = ySamples[i];
            A[i][0] = x * x;
            A[i][1] = y * y;
            A[i][2] = x * y;
            A[i][3] = x;
            A[i][4] = y;
            A[i][5] = 1.0;
            b_vec[i] = zSamples[i];
        }
        
        // Solve normal equations: A^T A x = A^T b
        std::vector<double> coeffs = solveLinearSystem(A, b_vec);
        
        if (coeffs.size() == 6) {
            bg.a = coeffs[0] / (width * width);
            bg.b = coeffs[1] / (height * height);
            bg.c = coeffs[2] / (width * height);
            bg.d = coeffs[3] / width;
            bg.e = coeffs[4] / height;
            bg.f = coeffs[5];
            
            // Calculate RMS
            double sumSq = 0;
            for (int i = 0; i < n; ++i) {
                double pred = bg.evaluate(xSamples[i] * width, ySamples[i] * height);
                double residual = zSamples[i] - pred;
                sumSq += residual * residual;
            }
            bg.rms = sqrt(sumSq / n);
        }
        
        return bg;
    }
    
    // Estimate PSF from bright stars in image
    PSFModel estimatePSF(const std::vector<float>& data, int width, int height) {
        PSFModel psf;
        
        // Find bright, isolated stars
        std::vector<float> sortedData = data;
        std::nth_element(sortedData.begin(), 
                        sortedData.begin() + sortedData.size()*99/100,
                        sortedData.end());
        float threshold = sortedData[sortedData.size()*99/100];
        
        std::vector<std::pair<int, int>> starCenters;
        
        // Simple peak detection
        for (int y = 20; y < height - 20; ++y) {
            for (int x = 20; x < width - 20; ++x) {
                int idx = y * width + x;
                if (data[idx] > threshold) {
                    // Check if local maximum
                    bool isMax = true;
                    for (int dy = -2; dy <= 2 && isMax; ++dy) {
                        for (int dx = -2; dx <= 2; ++dx) {
                            if (dx == 0 && dy == 0) continue;
                            int nidx = (y+dy)*width + (x+dx);
                            if (data[nidx] > data[idx]) {
                                isMax = false;
                                break;
                            }
                        }
                    }
                    if (isMax) {
                        starCenters.push_back({x, y});
                    }
                }
            }
        }
        
        // Measure FWHM from radial profiles
        std::vector<double> fwhms;
        
        for (const auto& center : starCenters) {
            if (fwhms.size() >= 50) break;  // Use up to 50 stars
            
            int cx = center.first;
            int cy = center.second;
            float peak = data[cy * width + cx];
            float half = peak / 2.0f;
            
            // Measure radius at half maximum
            double fwhm = 0;
            int count = 0;
            
            for (int angle = 0; angle < 8; ++angle) {
                double theta = angle * M_PI / 4.0;
                double dx = cos(theta);
                double dy = sin(theta);
                
                for (double r = 1; r < 20; r += 0.5) {
                    int x = cx + r * dx;
                    int y = cy + r * dy;
                    if (x >= 0 && x < width && y >= 0 && y < height) {
                        float val = data[y * width + x];
                        if (val < half) {
                            fwhm += r * 2.0;
                            count++;
                            break;
                        }
                    }
                }
            }
            
            if (count > 0) {
                fwhms.push_back(fwhm / count);
            }
        }
        
        // Calculate median FWHM
        if (!fwhms.empty()) {
            std::nth_element(fwhms.begin(), fwhms.begin() + fwhms.size()/2, fwhms.end());
            psf.fwhm = fwhms[fwhms.size()/2];
            psf.sigma = psf.fwhm / 2.355;  // Convert FWHM to sigma
            psf.modelType = "gaussian";
        }
        
        return psf;
    }
    
private:
    // Solve linear system using normal equations
    std::vector<double> solveLinearSystem(const std::vector<std::vector<double>>& A,
                                         const std::vector<double>& b) {
        int m = A.size();      // rows
        int n = A[0].size();   // cols
        
        // Compute A^T A
        std::vector<std::vector<double>> ATA(n, std::vector<double>(n, 0));
        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < n; ++j) {
                for (int k = 0; k < m; ++k) {
                    ATA[i][j] += A[k][i] * A[k][j];
                }
            }
        }
        
        // Compute A^T b
        std::vector<double> ATb(n, 0);
        for (int i = 0; i < n; ++i) {
            for (int k = 0; k < m; ++k) {
                ATb[i] += A[k][i] * b[k];
            }
        }
        
        // Solve using Gaussian elimination
        for (int i = 0; i < n; ++i) {
            // Partial pivoting
            int maxRow = i;
            for (int k = i + 1; k < n; ++k) {
                if (std::abs(ATA[k][i]) > std::abs(ATA[maxRow][i])) {
                    maxRow = k;
                }
            }
            std::swap(ATA[i], ATA[maxRow]);
            std::swap(ATb[i], ATb[maxRow]);
            
            // Forward elimination
            for (int k = i + 1; k < n; ++k) {
                double factor = ATA[k][i] / ATA[i][i];
                for (int j = i; j < n; ++j) {
                    ATA[k][j] -= factor * ATA[i][j];
                }
                ATb[k] -= factor * ATb[i];
            }
        }
        
        // Back substitution
        std::vector<double> x(n);
        for (int i = n - 1; i >= 0; --i) {
            x[i] = ATb[i];
            for (int j = i + 1; j < n; ++j) {
                x[i] -= ATA[i][j] * x[j];
            }
            x[i] /= ATA[i][i];
        }
        
        return x;
    }
};

#endif // FITSPROCESSOR_H
