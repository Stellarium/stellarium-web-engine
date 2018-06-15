// Stellarium Web - Copyright (c) 2018 - Noctua Software Ltd
//
// This program is licensed under the terms of the GNU AGPL v3, or
// alternatively under a commercial licence.
//
// The terms of the AGPL v3 license can be found in the main directory of this
// repository.

<template>
<v-dialog lazy scrollable max-width='600' v-model="$store.state.showPrivacyDialog">
  <v-card v-if="$store.state.showPrivacyDialog">
    <v-card-title><div class="headline">Our Privacy Policy</div></v-card-title>
    <v-card-text style="height: 600px;">
    <p>Stellarium Web is an online planetarium developed by Noctua Software Ltd with the goal of being easy to use and beautiful. We collect your data to help us achieve this goal, and we consider and respect your privacy rights every step of the way.</p>

    <p>We want to be completely open about what we are doing and why. The purpose of this policy is to explain how Noctua Software Ltd and all of its subsidiaries collect and use the personal information you provide to us, whether online, via phone, email, in letters, in any other correspondence, or even from other people.</p>

    <p>We ensure that we use your information in accordance with all applicable laws concerning the protection of personal information.</p>

    <p>If you have any questions at any time about data privacy at Noctua Software, please contact us at <a href="contact@noctua-software.com">contact@noctua-software.com</a></p>

    <h3>1. Who we are</h3>
    <p>Noctua Software Ltd is a Hong Kong based company created in 2011 by two of the main developpers of the famous open source software Stellarium.
    We develop and sell mobile phone apps and provide consulting service mostly related to astronomy software development.</p>

    <h3>2. What information we collect about you and when</h3>
    <p>We aim to limit the information we collect about you to only what we think is required for the purpose for which we collect it.</p>

    <p><strong>When you sign up to receive our newsletters or blog updates:</strong>
      <ul class="privacy">
        <li>we collect your email address.</li>
      </ul>
    </p>

    <p><strong>When you visit our website:</strong>
      <ul class="privacy">
        <li>we may collect technical information relating to your use of our website, including your browser type or the Internet Protocol (IP) address used to connect your computer to the internet. For more on this, see section “Information about cookies”.</li>
        <li>we may collect your current local time and location in order to compute an accurate and personalized sky map.</li>
      </ul>
    </p>

    <p><strong>When you sign up to our service:</strong>
      <ul class="privacy">
        <li>we collect your name, email address, login name and password when you give it to us for various purposes.</li>
        <li>we also collect all astronomical observational data you add in the observation tool such as date, location and details of the instrument used for the observation, as well as comments and associated pictures.</li>
      </ul>
    </p>

    <p><strong>For debugging purpose:</strong>
      <ul class="privacy">
        <li>we may collect your IP address</li>
      </ul>
    </p>

    <p>As the information we collect about you will be stored on our IT infrastructure, it may also be shared with our data processors who provide email, relationship management, anonymous analytics, document management, and storage services to us.</p>

    <p>If you want to understand more about why we collect your data, please contact us at <a href="contact@noctua-software.com">contact@noctua-software.com</a></p>

    <h3>3. How we collect and store that information</h3>
    <p>We take appropriate technical and organisational measures to ensure that the information disclosed to us is kept secure, accurate, and up to date, and that it is kept only for as long as is necessary for the purposes for which it is used.</p>

    <p>You should be aware that the use of the internet is not entirely secure, and although we will do our best to protect your personal data, we cannot guarantee the security or integrity of any personal information that is transferred from you or to you via the internet. Any transmission is at your own risk. Once we have received your information, we will use strict procedures and security features such as encryption to try to prevent unauthorised access.</p>

    <h3>4. What we use your information for</h3>
    <p>We collect your data on the occasions listed in section 3, “Why we collect that Information”.</p>

    <p>We use the information we have collected to deliver to you 1) information and services connected with the goal of this website, and 2) any newsletters you subscribe to. We may tell you about news and updates, ask you to participate in surveys, and check how we are doing in our goal.</p>

    <p>Where we have asked for your consent, we may use your image in our materials on our website, at events, and for other promotional purposes.</p>

    <h3>5. On what legal basis we use your information</h3>
    <p>We have a lawful basis for collecting your data as set out in section 3, “Why we collect that Information”, and for using the data for the purposes described in section 4, “What we use your information for”.</p>

    <p>Lawful basis may include:
      <ul class="privacy">
        <li>Where it is within our legitimate interests to process your information to further our goal to create an easy to use online planetarium. We know that under this basis of processing, we have a heightened responsibility to keep your interests central and to make sure that your rights as a data subject override our legitimate interest.</li>
        <li>Where we need to comply with a legal obligation.</li>
        <li>Where processing is necessary for the performance of a contract to which you are a party. If you fail to provide this information, we may be unable to perform the contract.</li>
        <li>Where we have your consent. If we want to use your image for the purposes of our mission, then we will ask you to consent to that use. Very occasionally, we will also process other areas of sensitive personal data, but we will always ask for your consent to do this.</li>
      </ul>
    </p>

    <h3>6. Who we share your information with</h3>
    <p>We never exchange or sell your information to any third party.</p>

    <h3>7. How long we keep your information for</h3>
    <p>We keep your information for no longer than is necessary for our purposes. What is necessary will depend on the reason for which the information was collected.</p>

    <p>If you have any questions about how long we keep your information, contact us at <a href="contact@noctua-software.com">contact@noctua-software.com</a></p>

    <h3>8. Your rights</h3>
    <p><strong>Right of access</strong> — you have a right to ask us to confirm whether we are processing information about you, and to request access to this information.</p>

    <p><strong>Right to rectification</strong> — you may ask us to rectify information you think is inaccurate, and you may also ask us to remove information which is inaccurate or incomplete. If you inform us that your personal data is inaccurate, we will inform relevant third parties with whom we have shared your data so that they may update their own records.</p>

    <p><strong>Right of portability</strong> — you have a right to obtain your personal data from us and reuse it for your own purposes.</p>

    <p><strong>Right to be forgotten</strong> — you have a right to seek the erasure of your data, for example where it is no longer necessary for us to continue holding or processing your personal data. This right is not absolute, as we may need to continue processing this information to comply with a legal obligation.</p>

    <p><strong>Right to restriction</strong> — you have a right to ask us to restrict our processing of your information if:
      <ul class="privacy">
        <li>You contest its accuracy and we need to verify whether it is accurate.</li>
        <li>You ask us to restrict use of it instead of deleting it.</li>
        <li>We no longer need the information for the purpose of processing, but you need it to establish or defend legal claims.</li>
        <li>You have objected to processing of your information being necessary for the purposes of our legitimate interests. The restriction would apply while we carry out a balancing act between your rights and our legitimate interests. If you exercise your right to restrict processing, we would still need to process your information for the purpose of exercising or defending legal claims, for the purpose of protecting the rights of another person, or for public interest reasons.</li>
      </ul>
    </p>

    <p>You can always unsubscribe from any mailing list you are included on, ask us to delete or anonymize your forum posts, and ask us to delete any comments on our blog. You can do so by contacting us at <a href="contact@noctua-software.com">contact@noctua-software.com</a>.</p>

    <p>We may ask for your help in locating specific information, e.g. on forum postings and confirming your identity. If you would like to exercise any of your rights, please let us know by writing to <a href="contact@noctua-software.com">contact@noctua-software.com</a>. We will act in accordance with your instructions as soon as reasonably possible, and there will be no charge.</p>

    <h3>9. Information about cookies</h3>
    <p>The Stellarium Web website, like most websites, uses small text files called cookies. This note provides information on what cookies are, which cookies we uses, and how they can be controlled.</p>

    <p>Our website uses cookies to distinguish you from other users of our website. A cookie is a small file of letters and numbers that we store on your browser or the hard drive of your computer if you agree. Cookies helps us to provide you with a good experience when you browse our website and also allows us to improve our site. By continuing to browse the site, you are agreeing to our use of cookies.</p>

    <p>We use the following cookies:

    <ul class="privacy">
      <li>Strictly necessary cookies. These are cookies that are required for the operation of our website. They include, for example, cookies that enable you to log into secure areas of our website such as forums.</li>
      <li>Analytical/performance cookies. They allow us to recognise and count the number of visitors and to see how visitors move around our website when they are using it. This lets us improve the way our website works, for example by helping us to ensure that users are easily finding what they are looking for.</li>
      <li>Functionality cookies. These are used to recognise you when you return to our website. This enables us to personalise our content for you.</li>
<!--  <li>Targeting cookies. These cookies record your visit to our website, the pages you have visited, and the links you have followed. We will use this information to make our website and the advertising displayed on it more relevant to your interests. We may also share this information with third parties for this purpose.</li> -->
    </ul>

    <p>For more information about the individual cookies we use, you can contact us at <a href="contact@noctua-software.com">contact@noctua-software.com</a>.</p>

    <p>Please note that third parties (including, for example, <!-- advertising networks and --> providers of external services like web traffic analysis services) may also use cookies, over which we have no control. These cookies are likely to be analytical/performance cookies <!-- or targeting --> cookies.</p>

    <p>You block cookies by activating the setting on your browser that allows you to refuse the setting of all or some cookies. However, if you use your browser settings to block all cookies (including essential cookies), you may not be able to access all or parts of our site. We recommend you visit <a href="www.aboutcookies.org">www.aboutcookies.org</a> for detailed guidance.</p>

    <p>Except for essential cookies, all cookies will expire after we no longer use them and in any event within two years of your session.</p>


    <h3>10. About changes to our privacy policy</h3>
    <p>Any changes we may make to our privacy policy in the future will be posted here and, where appropriate, sent to you by email. Please check the website regularly to see any updates or changes to our privacy policy. By continuing to use our website, you will be deemed to have accepted such changes.</p>

    <h3>11. How to contact us about your data</h3>
    <p>Data rights are human rights and we are determined to fairly and fully honour yours. You can unsubscribe at any time or just get in touch with any questions, comments, or instructions regarding your information by writing to <a href="contact@noctua-software.com">contact@noctua-software.com</a> or writing to: <i>Noctua Software Ltd, Unit 1109, 11/F., Kowloon Centre, 33 Ashley Road, Tsimshatsui, Kowloon, HongKong</i></p>
    </v-card-text>
    <v-card-actions>
      <v-spacer></v-spacer><v-btn class="blue--text darken-1" flat @click.native="$store.state.showPrivacyDialog = false">Close</v-btn>
    </v-card-actions>
  </v-card>
</v-dialog>
</template>

<script>
</script>

<style>
a {
  color: #82b1ff;
}

.privacy {
  padding-left: 30px;
}
</style>
