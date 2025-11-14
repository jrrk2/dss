// neighbor_diagnostic.cpp - Figure out the actual neighbor order
#include <QCoreApplication>
#include <QDebug>
#include "healpix_base.h"
#include "pointing.h"

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    
    qDebug() << "=== HEALPix Neighbor Pattern Diagnostic ===\n";
    
    // Test at order 6 with M31
    int order = 6;
    long long nside = 1LL << order;
    Healpix_Base healpix(nside, NEST, SET_NSIDE);
    
    // M31 position
    double ra_deg = 10.6847;
    double dec_deg = 41.2687;
    
    // Convert to HEALPix pointing
    double theta = (90.0 - dec_deg) * M_PI / 180.0;
    double phi = ra_deg * M_PI / 180.0;
    pointing center_pt(theta, phi);
    
    long long centerPixel = healpix.ang2pix(center_pt);
    
    qDebug() << QString("Center: RA=%1°, Dec=%2°, Pixel=%3")
                .arg(ra_deg).arg(dec_deg).arg(centerPixel);
    
    // Get center coordinates
    pointing center = healpix.pix2ang(centerPixel);
    double center_ra = center.phi * 180.0 / M_PI;
    double center_dec = 90.0 - center.theta * 180.0 / M_PI;
    
    qDebug() << QString("Center pixel coords: RA=%1°, Dec=%2°\n")
                .arg(center_ra, 0, 'f', 4).arg(center_dec, 0, 'f', 4);
    
    // Get neighbors
    fix_arr<int,8> neighbors;
    healpix.neighbors(centerPixel, neighbors);
    
    qDebug() << "Neighbor analysis (determining directional pattern):";
    qDebug() << "Index : Pixel  : RA       : Dec      : ΔRA    : ΔDec   : Direction";
    qDebug() << "------:--------:----------:----------:--------:--------:----------";
    
    for (int i = 0; i < 8; i++) {
        if (neighbors[i] >= 0) {
            pointing pt = healpix.pix2ang(neighbors[i]);
            double ra = pt.phi * 180.0 / M_PI;
            double dec = 90.0 - pt.theta * 180.0 / M_PI;
            
            double delta_ra = (ra - center_ra) * cos(center_dec * M_PI / 180.0);
            double delta_dec = dec - center_dec;
            
            // Determine direction from deltas
            QString direction;
            if (std::abs(delta_dec) > std::abs(delta_ra) * 2) {
                // Mostly vertical
                direction = (delta_dec > 0) ? "N" : "S";
            } else if (std::abs(delta_ra) > std::abs(delta_dec) * 2) {
                // Mostly horizontal
                direction = (delta_ra > 0) ? "E" : "W";
            } else {
                // Diagonal
                QString ns = (delta_dec > 0) ? "N" : "S";
                QString ew = (delta_ra > 0) ? "E" : "W";
                direction = ns + ew;
            }
            
            qDebug() << QString("  [%1] : %2 : %3 : %4 : %5 : %6 : %7")
                        .arg(i)
                        .arg(neighbors[i], 6)
                        .arg(ra, 8, 'f', 4)
                        .arg(dec, 8, 'f', 4)
                        .arg(delta_ra, 7, 'f', 3)
                        .arg(delta_dec, 7, 'f', 3)
                        .arg(direction, -10);
        } else {
            qDebug() << QString("  [%1] : NONE (edge of survey)").arg(i);
        }
    }
    
    qDebug() << "\n=== Direction Pattern Analysis ===";
    qDebug() << "From the ΔRA and ΔDec values above, we can determine";
    qDebug() << "the correct mapping for the createProper3x3Grid function.";
    qDebug() << "\nThe correct QStringList should map index -> direction";
    qDebug() << "based on the 'Direction' column above.";
    
    return 0;
}
