# Changelog

## [0.0.5] - 2020-11-27
- Fix selection issues
- Properly invalidate client cache when the server code was modified

## [0.0.4] - 2020-11-27
- Render the sky with 2 levels of details (LOD)
- In LOD 0: the boundaries of the features displayed are aligned on the healpix grid
- In LOD 1: displayed features are now the real geospatial union of the groupped features
- Improve polygon pre-processing at load time, deal with more corner cases
- Dispay progress bars while loading geojson data

## [0.0.3] - 2020-11-20
- Add version and Changelog file
- Show info about SMT server in the GUI
- Enable client-side caching of tile data

## [0.0.2] - 2020-11-19
- Footprints are cut precisely on healpix pixels boundaries
- Switch to data_01 branch from smt-data project
- Fix aggregation for string fields in spatial queries
- Fix footprint colors function with updated aggregation function

## [0.0.1] - 2020-11-17

- First deployed version on test dev server [smt-frontend](https://smt-frontend.stellarium-web.org/)
- Data files are now stored on [smt-data github project](https://github.com/Stellarium-Labs/smt-data)
- Official code repo is now on [smt github project](https://github.com/Stellarium-Labs/smt)

