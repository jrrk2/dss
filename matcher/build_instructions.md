# Enhanced DSS Image Matcher

Advanced astronomical image analysis tool with WCS matching, background gradient removal, PSF estimation, and intelligent caching.

## New Features

### 1. **Load Arbitrary FITS Files**
- Load your own FITS images with WCS information
- Automatic WCS extraction and validation
- Support for standard FITS headers (CRVAL, CDELT, CRPIX, etc.)

### 2. **WCS Coordinate Matching**
- Match your images to DSS library images using World Coordinate System
- Pixel-to-world and world-to-pixel coordinate transformations
- Support for TAN (gnomonic) projection
- Rotation and scale analysis

### 3. **Background Gradient Removal**
- Automatic 2D quadratic background model fitting
- Robust outlier rejection using MAD (Median Absolute Deviation)
- Grid-based sampling to avoid bright sources
- Calculate and apply optimal background correction

### 4. **Point Spread Function (PSF) Estimation**
- Automatic star detection in images
- FWHM (Full Width Half Maximum) measurement
- Gaussian PSF modeling
- Compare PSF between your image and library image

### 5. **Intelligent Disk Caching**
- Automatic caching of downloaded DSS images
- Separate caching for single surveys and composites
- JSON metadata tracking (access times, counts, file sizes)
- LRU (Least Recently Used) cache cleanup
- Cache statistics and management UI

## File Structure

```
DSSMatcher/
├── main_enhanced.cpp        # Enhanced main application
├── DSSMatcher.h             # DSS API interface (unchanged)
├── MessierCatalog.h         # Messier object database (unchanged)
├── FitsProcessor.h          # NEW: FITS loading & WCS processing
├── ImageCache.h             # NEW: Disk-based caching system
├── ImageMatcherDialog.h     # NEW: WCS matching & analysis UI
└── DSSMatcher.pro           # Qt project file
```

## Dependencies

### Required Libraries
- **Qt 5.15+** or **Qt 6.x**
  - QtCore
  - QtGui
  - QtWidgets
  - QtNetwork

- **CFITSIO 3.x+**
  - FITS file I/O library
  - Install on macOS: `brew install cfitsio`
  - Install on Linux: `sudo apt-get install libcfitsio-dev`

### Build System
- **qmake** (comes with Qt)
- **Xcode** (for macOS/iMac)
- **C++17** compiler support

## Building on macOS/iMac with Xcode

### 1. Install Dependencies

```bash
# Install Homebrew if not already installed
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install Qt
brew install qt@6

# Install CFITSIO
brew install cfitsio

# Add Qt to PATH
echo 'export PATH="/usr/local/opt/qt@6/bin:$PATH"' >> ~/.zshrc
source ~/.zshrc
```

### 2. Create Qt Project File

Create `DSSMatcher.pro`:

```qmake
QT       += core gui widgets network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = DSSMatcher
TEMPLATE = app

CONFIG += c++17

# CFITSIO library
macx {
    INCLUDEPATH += /usr/local/include
    LIBS += -L/usr/local/lib -lcfitsio
}

linux {
    INCLUDEPATH += /usr/include
    LIBS += -lcfitsio
}

SOURCES += main_enhanced.cpp

HEADERS += \
    DSSMatcher.h \
    MessierCatalog.h \
    FitsProcessor.h \
    ImageCache.h \
    ImageMatcherDialog.h

# Deployment
macx {
    QMAKE_INFO_PLIST = Info.plist
}

# Enable warnings
QMAKE_CXXFLAGS += -Wall -Wextra
```

### 3. Generate Xcode Project

```bash
cd /path/to/DSSMatcher
qmake -spec macx-xcode DSSMatcher.pro
```

This creates `DSSMatcher.xcodeproj`

### 4. Build in Xcode

```bash
# Open in Xcode
open DSSMatcher.xcodeproj

# Or build from command line
xcodebuild -project DSSMatcher.xcodeproj -scheme DSSMatcher -configuration Release
```

### 5. Alternative: Build with qmake directly

```bash
qmake DSSMatcher.pro
make
./DSSMatcher
```

## Usage Guide

### Basic Workflow

1. **Browse Messier Objects**
   - Select from dropdown or list
   - Filter by type (galaxies, nebulae, etc.)
   - Filter by "imaged only" if you've photographed them

2. **Fetch DSS Images**
   - Single survey: Click "Fetch Selected Survey"
   - Composite: Click "Fetch False Color Composite"
   - Images are automatically cached for faster access

3. **Load Your FITS Image**
   - Click "Load Your FITS File..." button
   - Select a FITS file with WCS information
   - Must have standard WCS headers (CRVAL1, CRVAL2, CDELT1, CDELT2, etc.)

4. **Match & Analyze**
   - First fetch a DSS library image for the same object
   - Then click "Match & Analyze with DSS Image"
   - View side-by-side comparison
   - See WCS parameters, PSF measurements, and background analysis

5. **Background Correction**
   - In the matcher dialog, click "Apply Background Correction"
   - View your image with gradient removed
   - Compare before/after

### Cache Management

- **View Cache Info**: Menu → Cache → Cache Info
- **Clear Cache**: Menu → Cache → Clear Cache
- **Cleanup Old**: Menu → Cache → Cleanup Old Entries (removes entries >30 days)
- Cache location: `~/Library/Caches/DSS_Images/` (macOS)

### Analysis Output

The matcher dialog shows:
- **Image dimensions**: Pixel sizes of both images
- **WCS parameters**: RA/Dec centers, pixel scales, rotation
- **Background model**: Quadratic coefficients, RMS residuals
- **PSF measurements**: FWHM in pixels and arcseconds, sigma values

## Advanced Features

### Custom Survey Selection
Choose from:
- POSS2/UKSTU Red, Blue, IR (best quality)
- POSS1 Red, Blue (historical)
- Quick-V (fast, lower quality)

### Composite Image Processing
- False color: R=IR, G=Red, B=Blue channels
- Automatically cached as 3-plane FITS
- All three surveys fetched automatically

### WCS Coordinate System
- Supports TAN (gnomonic) projection
- J2000 equinox
- Handles rotation (CROTA2)
- Pixel-to-world transformations

### Background Modeling
- 2D quadratic: `z = ax² + by² + cxy + dx + ey + f`
- Robust fitting with 3-sigma clipping
- Grid sampling to avoid stars
- RMS calculation for quality assessment

### PSF Estimation
- Automatic star detection
- Radial profile analysis
- FWHM measurement from multiple stars
- Median combining for robustness

## Troubleshooting

### CFITSIO Not Found
```bash
# Check installation
brew list cfitsio

# Reinstall if needed
brew reinstall cfitsio

# Verify library path
ls -l /usr/local/lib/libcfitsio*
```

### Qt Not Found
```bash
# Check Qt installation
which qmake

# Install/reinstall Qt
brew install qt@6

# Update PATH
export PATH="/usr/local/opt/qt@6/bin:$PATH"
```

### Build Errors
- Ensure C++17 support: Add `CONFIG += c++17` to .pro file
- Check include paths in .pro file match your installation
- Verify CFITSIO version: `brew info cfitsio`

### Runtime Issues
- Missing WCS: Ensure FITS file has CRVAL1, CRVAL2, CDELT1, CDELT2 headers
- Cache errors: Check write permissions to cache directory
- Network errors: Verify internet connection for DSS downloads

## Performance Tips

1. **Use Cache**: Downloaded images are cached automatically
2. **Batch Processing**: Fetch composites for multiple objects at once
3. **Grid Size**: Adjust background grid sampling (default: 50 pixels)
4. **Cleanup Regularly**: Remove old cache entries to save disk space

## Future Enhancements

Potential additions:
- [ ] Plate solving for images without WCS
- [ ] Image registration and stacking
- [ ] Photometry tools
- [ ] Star catalog overlay
- [ ] Export analysis reports
- [ ] Batch processing mode

## Credits

- **DSS**: Digitized Sky Survey (STScI)
- **CFITSIO**: NASA HEASARC
- **Qt**: The Qt Company
- **Messier Catalog**: Charles Messier (1774)

## License

This software is for educational and research purposes. Please respect the DSS usage policies and cite appropriately in publications.
