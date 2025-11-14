// test_healpix_grid.cpp - Verify HEALPix neighbor calculations
#include <QCoreApplication>
#include <QDebug>
#include "ProperHipsClient.h"

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    
    qDebug() << "=== HEALPix Grid Verification Test ===\n";
    
    ProperHipsClient client;
    
    // Test M31 position
    SkyPosition m31;
    m31.ra_deg = 10.6847;
    m31.dec_deg = 41.2687;
    m31.name = "M31";
    
    qDebug() << "Testing M31 Andromeda Galaxy";
    qDebug() << QString("Position: RA=%1°, Dec=%2°\n").arg(m31.ra_deg).arg(m31.dec_deg);
    
    // Test at different orders
    for (int order = 6; order <= 8; order++) {
        qDebug() << QString("\n========== ORDER %1 ==========").arg(order);
        
        long long centerPixel = client.calculateHealPixel(m31, order);
        qDebug() << "Center pixel:" << centerPixel;
        
        // Get the grid
        QList<QList<long long>> grid = client.createProper3x3Grid(centerPixel, order);
        
        // Verify alignment
        client.verifyGridAlignment(centerPixel, order);
        
        // Check for duplicates (which would indicate wrong neighbor calculation)
        QSet<long long> uniquePixels;
        int duplicates = 0;
        for (int row = 0; row < 3; row++) {
            for (int col = 0; col < 3; col++) {
                long long pixel = grid[row][col];
                if (uniquePixels.contains(pixel) && pixel != centerPixel) {
                    duplicates++;
                    qDebug() << QString("⚠️  DUPLICATE: pixel %1 appears multiple times!").arg(pixel);
                }
                uniquePixels.insert(pixel);
            }
        }
        
        if (duplicates == 0) {
            qDebug() << "✅ No duplicate pixels - grid looks good!";
        } else {
            qDebug() << QString("❌ Found %1 duplicate pixels - neighbor calculation is WRONG!").arg(duplicates);
        }
        
        // Calculate tile coverage
        long long nside = 1LL << order;
        double pixel_size_deg = sqrt(4.0 * M_PI / (12.0 * nside * nside)) * 180.0 / M_PI;
        double grid_size_deg = 3.0 * pixel_size_deg;
        
        qDebug() << QString("\nGrid covers: %1° × %2° (%3' × %4')")
                    .arg(grid_size_deg, 0, 'f', 3)
                    .arg(grid_size_deg, 0, 'f', 3)
                    .arg(grid_size_deg * 60.0, 0, 'f', 1)
                    .arg(grid_size_deg * 60.0, 0, 'f', 1);
    }
    
    qDebug() << "\n=== Test Complete ===";
    
    return 0;
}
