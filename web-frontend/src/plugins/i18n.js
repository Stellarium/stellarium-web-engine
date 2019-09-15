/* =================
 * Translation guide
 * =================
 * 1. Add your desired language (without hyphen) to an array in
 *    /src/plugins/langs.js. The list should then look like this:
 *    `const listLanguages = ['en', 'pl', 'ru']`
 *    - note: don't add eg. 'zh_CN', but only 'zh'
 *
 * 2. Add your language to 'items' array in /src/components/lang-settings-
 *    dialog.vue
 *
 * 3. Duplicate the existing 'en' object in this file (in 'const translations')
 *    and change name of it object to the short name of your language.
 *
 * 4. Translate key/value pairs in the new object. Don't change key, only change
 *    values. If you can't translate something, leave the original value.
 *
 * 5. Don't forget to add your name to `doc/cla` directory and create a PR
 *
 * Translators:
 * - Paweł Pleskaczyński: added translation support and added Polish translation
*/

import Vue from 'vue'
import VueI18n from 'vue-i18n'
import langs from './langs.js'

var language = langs.language()

const translations = {
  en: {
    planets: {
      sun: 'sun',
      moon: 'moon',
      mercury: 'mercury',
      venus: 'venus',
      earth: 'earth',
      mars: 'mars',
      jupiter: 'jupiter',
      saturn: 'saturn',
      uranus: 'uranus',
      neptune: 'neptune',
      pluto: 'pluto',
      planet: 'planet'
    },
    galaxy_type: {
      elliptical: 'Elliptical',
      bar_spiral: 'Barred Spiral',
      int_spiral: 'Intermediate Spiral',
      spiral: 'Spiral',
      lenticular: 'Lenticular',
      irregular: 'Irregular',
      dwf_spheroidal: 'Dwarf Spheroidal',
      dwf_elliptical: 'Dwarf Elliptical'
    },
    ui: {
      app: {
        ephemeris: 'Ephemeris',
        planets_tonight: 'Planets Tonight',
        settings: 'Settings',
        lang_settings: 'Change language',
        view_settings: 'View Settings',
        about: 'About',
        data_credits: 'Data Credits',
        privacy: 'Privacy'
      },
      bottom_bar: {
        constellations: 'Constellations',
        constellations_art: 'Constellations Art',
        atmosphere: 'Atmosphere',
        landscape: 'Landscape',
        az_grid: 'Azimuthal Grid',
        eq_grid: 'Equatorial Grid',
        dso: 'Deep Sky Objects',
        night_mode: 'Night Mode',
        fullscreen: 'Fullscreen'
      },
      common: {
        close: 'Close'
      },
      date_time_picker: {
        back_realtime: 'Back to realtime',
        pause_unpause: 'Pause/unpause time'
      },
      gui_loader: {
        titlep1: 'Loading Stellarium ',
        titlep2: 'Web',
        titlep3: ', the online Planetarium',
        error_cannot_load_wasm: ' It seems that your browser cannot load Web Assembly!',
        error_wasm_is_needed: 'Web assembly is necessary for Stellarium Web to display the night sky. Please upgrade your web browser and try again!',
        error_in_the_meantime_try: 'In the meantime, you can try the ',
        error_desktop_version: 'desktop version',
        error_or_read: ', or read the project\'s ',
        error_project_news: 'news'
      },
      lang_settings_dialog: {
        lang_settings: 'Change language',
        select_lang: 'Select language'
      },
      location_dialog: {
        use_autolocation: 'Use Autolocation'
      },
      location_mgr: {
        my_locations: 'My Locations',
        use_this_location: ' Use this location',
        unknown_address: 'Unknown Address',
        drag_to_adjust: 'Drag to adjust'
      },
      planets_visibility: {
        planets_visibility: 'Planets Visibility',
        night_from: 'Night from',
        to: 'to',
        rise: 'Rise',
        set: 'Set'
      },
      skysource_search: {
        search: 'Search...'
      },
      toolbar: {
        observe: 'Observe'
      },
      selected_object_info: {
        known_as: 'Also known as',
        read_more_on: ' read more on ',
        magnitude: 'Magnitude',
        distance: 'Distance',
        radius: 'Radius',
        spectral_type: 'Spectral type',
        size: 'Size',
        ra_dec: 'Ra/Dec',
        az_alt: 'Az/Alt',
        phase: 'Phase',
        not_visible: 'Not visible tonight',
        visible: 'Always visible tonight',
        visibility: 'Visibility',
        rise: 'Rise',
        set: 'Set',
        unknown: 'Unknown',
        light_years: 'light years',
        au: 'AU'
      },
      view_settings_dialog: {
        view_settings: 'View Settings',
        milky_way: 'Milky Way',
        dss: 'DSS',
        simulate_refraction: 'Simulate refraction',
        meridian_line: 'Meridian Line',
        ecliptic_line: 'Ecliptic Line',
        orange_night_mode: 'Orange night mode (default is red)'
      }
    }
  },
  pl: {
    planets: {
      sun: 'słońce',
      moon: 'księżyc',
      mercury: 'merkury',
      venus: 'wenus',
      earth: 'ziemia',
      mars: 'mars',
      jupiter: 'jowisz',
      saturn: 'saturn',
      uranus: 'uran',
      neptune: 'neptun',
      pluto: 'pluton',
      planet: 'planeta'
    },
    galaxy_type: {
      elliptical: 'Galaktyka eliptyczna',
      bar_spiral: 'Galaktyka z poprzeczką',
      int_spiral: 'Pośrednia galaktyka spiralna',
      spiral: 'Galaktyka spiralna',
      lenticular: 'Galaktyka soczewkowata',
      irregular: 'Galaktyka nieregularna',
      dwf_spheroidal: 'Karłowata galaktyka sferoidalna',
      dwf_elliptical: 'Karłowata galaktyka eliptyczna'
    },
    ui: {
      app: {
        ephemeris: 'Efemerydy',
        planets_tonight: 'Widoczność planet tej nocy',
        settings: 'Ustawienia',
        lang_settings: 'Zmień język/Change language',
        view_settings: 'Ustawienia wyświetlania',
        about: 'O programie',
        data_credits: 'Źródła danych',
        privacy: 'Prywatność'
      },
      bottom_bar: {
        constellations: 'Gwiazdozbiory',
        constellations_art: 'Rysunki gwiazdozbiorów',
        atmosphere: 'Atmosfera',
        landscape: 'Krajobraz',
        az_grid: 'Siatka azymutalna',
        eq_grid: 'Siatka równikowa',
        dso: 'Obiekty głębokiego nieba',
        night_mode: 'Tryb nocny',
        fullscreen: 'Pełny ekran'
      },
      common: {
        close: 'Zamknij'
      },
      date_time_picker: {
        back_realtime: 'Powrót do czasu rzeczywistego',
        pause_unpause: 'Wstrzymaj/wznów czas'
      },
      gui_loader: {
        titlep1: 'Ładowanie Stellarium ',
        titlep2: 'Web',
        titlep3: ', planetarium online',
        error_cannot_load_wasm: ' Wygląda na to, że Twoja przeglądarka nie obsługuje Web Assembly!',
        error_wasm_is_needed: 'Web Assembly jest wymagane dla Stellarium, aby wyświetlać nocne niebo. Prosimy o aktualizację przeglądarki i o ponowną próbę uruchomienia Stellarium Web.',
        error_in_the_meantime_try: 'W międzyczasie, możesz wypróbować ',
        error_desktop_version: 'wersję na pulpit',
        error_or_read: ', lub przeczytać ',
        error_project_news: 'wiadomości o projekcie'
      },
      lang_settings_dialog: {
        lang_settings: 'Change language/Zmień język',
        select_lang: 'Select language'
      },
      location_dialog: {
        use_autolocation: 'Korzystaj z automatycznej lokalizacji'
      },
      location_mgr: {
        my_locations: 'Moje lokalizacje',
        use_this_location: ' Korzystaj z tej lokalizacji',
        unknown_address: 'Nieznany adres',
        drag_to_adjust: 'Przeciągnij, aby dostosować'
      },
      planets_visibility: {
        planets_visibility: 'Widoczność planet',
        night_from: 'Noc z',
        to: 'na',
        rise: 'Wschód',
        set: 'Zachód'
      },
      skysource_search: {
        search: 'Szukaj...'
      },
      toolbar: {
        observe: 'Obserwacje'
      },
      selected_object_info: {
        known_as: 'Również znany jako',
        read_more_on: ' przeczytaj więcej: ',
        magnitude: 'Magnitudo',
        distance: 'Odległość',
        radius: 'Promień',
        spectral_type: 'Typ spektralny',
        size: 'Rozmiar',
        ra_dec: 'Ra/Dec',
        az_alt: 'Az/Alt',
        phase: 'Faza',
        not_visible: 'Niewidoczny tej nocy',
        visible: 'Ciągle widoczny tej nocy',
        visibility: 'Widoczność',
        rise: 'Wschód',
        set: 'Zachód',
        unknown: 'Nieznane',
        light_years: 'lata świetlne',
        au: 'j.a.'
      },
      view_settings_dialog: {
        view_settings: 'Ustawienia wyświetlania',
        milky_way: 'Droga Mleczna',
        dss: 'Zdjęcia nieba z DSS',
        simulate_refraction: 'Symuluj refrakcję',
        meridian_line: 'Linia południkowa',
        ecliptic_line: 'Linia ekliptyki',
        orange_night_mode: 'Pomarańczowy tryb nocny (domyślny jest czerwony)'
      }
    }
  }
}

export default translations

Vue.use(VueI18n)
export const i18n = new VueI18n({
  locale: language,
  fallbackLocale: 'en',
  messages: translations,
  silentTranslationWarn: false
})
