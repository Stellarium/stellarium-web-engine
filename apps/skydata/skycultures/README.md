# Sky Cultures format

__Draft, work in progress__

This is the documentation of the new format used for defining sky cultures in
Stellarium Web Engine. This is work in progress and this format is therefore
still evolving quickly.

The sky cultures found here are all originating from the ones in Stellarium
desktop version, but there is no simple one-to-one mapping between the old
legacy format and this one.

## Main file format changes

The main changes brought by this new format are the following:

 - all the content is now stored in 2 files instead of many files in the legacy
 format:
   - a single index.json file containing the definition of the constellations
     line, illustrations, common names and other metadata.
   - a single description.md (markdown) file containing the textual description
     in english of a sky culture with a somewhat rigid structure.
 - translation (po) files for a sky culture are now stored within the sky
   culture directory, in the po/ directory. The po files contain the
   constellations and sky object common names, but also the elements related to
   sky culture description, in markdown format.
 - images for illustrations are now stored into an illustrations/ subdirectory.
 - new metadata was added such as:
   - thumbnail for each sky culture or constellations
   - description for constellations or associated to a sky object name
   - native names and pronunciation for all common names (e.g. chinese
     characters and pidgin spelling)
   - multiple names for one sky object or constellation
   - region name for each sky culture

## JSON file content

The main index.json file must have the following format:
```
{
  // Identifier of the constellation, must match the directory name and
  // is used to generate the ids for each constellations of this sky culture.
  "id": "western",
  // One of "America", "Middle East", "Europe", "Asia", "Oceania"
  "region": "Europe",
  // Thumbnail image to display e.g. when showing the description for this
  // sky culture.
  "thumbnail": "my_thumb.webp",
  // Define to true if international names must be used as fallback when no
  // cultural name is explicitly defined for a sky object.
  "fallback_to_international_names": true,
  // Language which will use the native name by default to label objects
  "langs_use_native_names": ["en", "pt", "es"],
  // Language used for native names
  "native_lang": "lat",
  // The list of constellations
  "constellations": [
    {
      // Unique ID, must start with prefix "CON id" where id is the id of
      // the sky culture
      "id": "CON western Aql",
      // List of lines paths. Each number is a Hipparcos star number.
      "lines": [[98036, 97649, 97278], [97649, 95501, 97804], [99473, 97804], [95501, 93747, 93244], [95501, 93805]],
      // Image used as illustration in the sky
      "image": {
        "file": "illustrations/aquila.webp",
        // Size doesn't have to match the actual image size, it is only used
        // as a reference when converting anchors pixel position to relative
        // position, i.e. between 0 and 1. This allows to change images
        // resolutions without changing the json.
        "size": [512, 512],
        "anchors": [
          {"pos": [163, 232], "hip": 97649},
          {"pos": [385, 131], "hip": 93244},
          {"pos": [397, 397], "hip": 93805}
        ],
        // Thumbnail to display e.g. when constellation is selected
        "thumbnail": "my_thumb.webp"
      },
      // Common names can define the english name (the one used as reference
      // for translations), the native name (using native spelling), and the
      // pronounciation name, e.g. pinyin for chinese.
      "common_name": {"english": "Hairy Head", "pronounce": "Mǎoxiù", "native": "昴宿"}
      // IAU abbreviation, only used for western sky culture
      "iau": "Aql"
    },
    ...
  ],
  // Only used for western sky culture
  "edges": [...]
```

## Markdown description

The description.md file has to follow a somewhat strict structure.
All level 1 and 2 titles (the ones with # or ##) are reserved.

```markdown
# Sky culture name

## Introduction

A short intruction text for this sky culture. It can be used to display a quick
preview in the GUI.

## Description

Detailed description using the markdown format, including markdown extra
extensions, as well a special syntax for defining reference like this [#1].

## Constellations

##### Constellation 1

Description of constellation 1. The title must be of level 5, and must match the
name defined in the index.json file. Note that this content can also be put
directly inside the "description" field for a constellation in the index.json
file, but for long text with formatting it is easier to put it here.

##### Constellation 2

Description of constellation 2.

## References

 - [#1]: Blah blah et al. [website](http://www.blahblah.html)

## Authors

Free text about the authors and their credits.

## Licence

Free text about the licence(s)
```


## Culture-specific and "international" names

Sky object names are now split into 2 categories:
 - a global list of "international" names, such as "Orion Nebula" or
   "Proxima Centauri". This list is independent of any sky culture, and is
   usually used for faint objects which don't exist in pre-telescope times, or
   for IAU names.
 - a list of sky culture-specific names. Only these have to be defined in
   the sky culture index.json file.

Sky cultures can specify in the index.json file that sky objects which do not
have a culture-specific name can fallback to using the "international" name
instead. This allows to avoid re-defining common star or planets names in each
Western sky cultures. If no fallback is requested, many objets won't be named
if the culture-specific names are unknown.

## Chinese star names

Chinese star are named using a bayer-like notation with the following format:
"traditional constellation_name [Added] [star number]", e.g. star HIP 115065 is
called "Flying Serpent Added VII", meaning "the 8th added star in the Flying
Serpent traditional constellation".

To avoid filling the translation files with hundreds of very similar names, only
the traditional constellation name is stored in the po files. This allows to
dramatically reduce the amount of translations, at the expense of requiring some
runtime logic for translating those names.

## Western sky culture and latin constellation names

The IAU defined the official constellations names in latin, and these latin
names are also commonly used in some languages like english, spanish or
portuguese. However, for these languages, a more localized name is usually also
defined, e.g. for the "Aquila" constellation, english speakers can also use the
name "Eagle", portugueses the name "Águia".

For this reason, we adopted the following standard for the western
constellation names:
 - "native" name is defined in latin (e.g. "Aquila")
 - "english" name is defined in english (e.g. "Eagle")

To allow Stellarium to display the proper name for each language, each sky
culture can define a list of languages for which the native name is preferred
over the translated english name. This list is defined in the index.json file
under the langs_use_native_names property.

## Tooling

Beside the file format changes, the new format comes with script tools:
 - a kind of "sky culture compiler" mostly converting markdown data to HTML
   for display in the app.
 - tool for updating the .po files when the sky culture content was changed.
