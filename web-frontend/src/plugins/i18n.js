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
 * 3. Let's assume you want to translate from English to Russian, and you have
 *    done 1st and 2nd step. You need to copy and paste existing 'en'
 *    object in this file (in 'const translations') and change name of new
 *    object to 'ru'.
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
    general: {
      constellation: 'constellation'
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
      about_dialog: {
        title: 'About',
        line1: 'Welcome to Stellarium Web, a free open source planetarium running in your web browser.',
        line2p1: 'This page is sill in beta, please report any bugs or ask questions to ',
        line2p2: ' or on our ',
        line2p3: 'Github page',
        line3p1: 'By using this website, you agree with our ',
        line3p2: 'Privacy Policy'
      },
      app: {
        cookies: 'This site uses cookies. By continuing to browse the site you are agreeing to our use of cookies. Check our ',
        privacy_policy: 'Privacy Policy',
        agree: 'I Agree',
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
      gui_loader: {
        titlep1: 'Loading Stellarium ',
        titlep2: 'Web',
        titlep3: ', the online Planetarium',
        error: ' It seems that your browser cannot load Web Assembly!',
        error1: 'Web assembly is necessary for Stellarium Web to display the night sky. Please upgrade your web browser and try again!',
        error2p1: 'In the meantime, you can try the ',
        error2p2: 'desktop version',
        error2p3: ', or read the project\'s ',
        errpr2p4: 'news'
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
      my_profile: {
        my_profile: 'My Profile',
        first_name: 'First Name',
        last_name: 'Last Name',
        email: 'E-Mail',
        password: 'Password',
        delete_account: 'Delete Account',
        delete_my_account: 'Delete my account',
        delete_warning1: 'Deleting your NoctuaSky account is permanent and cannot be undone. Before deleting your account, please keep in mind that:',
        delete_warning2: 'Deleting your account will irremediably delete all observations, personal informations and pictures you created from our servers.',
        delete_warning3: 'Observations you shared with public links won\'t be accessible anymore.',
        delete_ok: 'Yes, delete all',
        delete_cancel: 'Cancel!',
        change_password: 'Change Password',
        current_pass: 'Current password',
        new_pass: 'New password',
        confirm_new_pass: 'Confirm new password',
        update_user_info: 'Update User Information',
        send: 'Send'
      },
      observing_panel_root_toolbar: {
        logged_as: 'Logged as',
        my_profile: 'My Profile',
        logout: 'Logout',
        sign_in: 'Sign in'
      },
      planets_visibility: {
        planets_visibility: 'Planets Visibility',
        night_from: 'Night from',
        to: 'to',
        rise: 'Rise',
        set: 'Set'
      },
      signin: {
        signin_to: 'Sign in to NoctuaSky',
        new: 'New to NoctuaSky?',
        create_account: 'Create an account',
        create_account2: 'Create a NoctuaSky account',
        create_password: 'Create password',
        confirm_password: 'Confirm password',
        sign_up: 'Sign Up',
        thanks: 'Thanks for signing in to NoctuaSky.com!',
        msg1: 'Before you can continue, you need to confirm that your email is valid.',
        msg2: 'We have just sent you a message containing a confirmation link that you need to click.',
        ok: 'OK, get me to SignIn'
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
    general: {
      constellation: 'gwiazdozbiór'
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
      about_dialog: {
        title: 'O programie',
        line1: 'Witaj w Stellarium Web, darmowym i otwarto-źródłowym planetarium, działającym w Twojej przeglądarce.',
        line2p1: 'Ta strona jest nadal w wersji beta, prosimy o zgłoszenie błędów lub o pytania tutaj: ',
        line2p2: ' lub na naszą ',
        line2p3: 'stronę na Github\'ie',
        line3p1: 'Korzystając z tej strony, zgadzasz się z naszą ',
        line3p2: 'Polityką Prywatności'
      },
      app: {
        cookies: 'Ta strona używa plików cookies. Kontynuując przeglądanie witryny, wyrażasz zgodę na używanie przez nas plików cookies. Sprawdź naszą ',
        privacy_policy: 'Politykę Prywatności',
        agree: 'Wyrażam Zgodę',
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
      gui_loader: {
        titlep1: 'Ładowanie Stellarium ',
        titlep2: 'Web',
        titlep3: ', planetarium online',
        error: ' Wygląda na to, że Twoja przeglądarka nie obsługuje Web Assembly!',
        error1: 'Web Assembly jest wymagane dla Stellarium, aby wyświetlać nocne niebo. Prosimy o aktualizację przeglądarki i o ponowną próbę uruchomienia Stellarium Web.',
        error2p1: 'W międzyczasie, możesz wypróbować ',
        error2p2: 'wersję na pulpit',
        error2p3: ', lub przeczytać ',
        errpr2p4: 'wiadomości o projekcie'
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
      my_profile: {
        my_profile: 'Mój profil',
        first_name: 'Imię',
        last_name: 'Nazwisko',
        email: 'E-Mail',
        password: 'Hasło',
        delete_account: 'Usuń konto',
        delete_my_account: 'Usuń moje konto',
        delete_warning1: 'Usuwanie Twojego konta NoctuaSky jest nieodwracalne. Przed usuwaniem konta, pamiętaj o tym, że:',
        delete_warning2: 'Usuwanie Twojego konta na zawsze usunie z naszych serwerów wszystkie zapisane obserwacje, informacje prywatne, a także wszystkie zdjęcia.',
        delete_warning3: 'Obserwacje, które udostępniłeś za pomocą linków publicznych nie będą dłużej dostępne.',
        delete_ok: 'Tak, usuń wszystko',
        delete_cancel: 'Anuluj!',
        change_password: 'Zmień hasło',
        current_pass: 'Aktualne hasło',
        new_pass: 'Nowe hasło',
        confirm_new_pass: 'Potwierdź nowe hasło',
        update_user_info: 'Aktualizuj informacje o użytkowniku',
        send: 'Wyślij'
      },
      observing_panel_root_toolbar: {
        logged_as: 'Zalogowany jako',
        my_profile: 'Mój profil',
        logout: 'Wyloguj się',
        sign_in: 'Zaloguj się'
      },
      planets_visibility: {
        planets_visibility: 'Widoczność planet',
        night_from: 'Noc z',
        to: 'na',
        rise: 'Wschód',
        set: 'Zachód'
      },
      signin: {
        signin_to: 'Zaloguj się do NoctuaSky',
        new: 'Jesteś nowy w NoctuaSky?',
        create_account: 'Utwórz konto',
        create_account2: 'Utwórz konto NoctuaSky',
        create_password: 'Utwórz hasło',
        confirm_password: 'Potwierdź hasło',
        sign_up: 'Zarejestruj się',
        thanks: 'Dziękujemy za rejestrację w NoctuaSky.com!',
        msg1: 'Zanim będziesz mógł kontynuować, musisz potwierdzić, że Twój e-mail jest ważny.',
        msg2: 'Właśnie wysłaliśmy Ci wiadomość zawierającą link potwierdzający, który musisz kliknąć.',
        ok: 'OK, przejdźmy do logowania'
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
