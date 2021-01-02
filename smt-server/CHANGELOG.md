# Changelog

## [0.0.9] - 2021-01-3
- Fix date range slider disalignement when scroll bar appears
- Fix histograms tooltip blinking bug
- Only pass fieldId in server API instead of the full field definition

## [0.0.8] - 2020-12-16
- Fix date range bug when range is too small
- Fix invalid number of results caused by sub-features (features splitted on the healpix grid)

## [0.0.7] - 2020-12-15
- Use SQLite as a primary DB engine

## [0.0.6] - 2020-12-03
- Run the node server behind a nginx frontend
- Enable server-side caching and gzip compression
- Fix some docker issues
- Change API to use a /api/v1/ prefix
- Add cache-friendly GET query methods

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
