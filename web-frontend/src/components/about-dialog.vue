// Stellarium Web - Copyright (c) 2018 - Noctua Software Ltd
//
// This program is licensed under the terms of the GNU AGPL v3, or
// alternatively under a commercial licence.
//
// The terms of the AGPL v3 license can be found in the main directory of this
// repository.

<template>
<v-dialog lazy max-width="600" v-model="$store.state.showAboutDialog">
  <v-card v-if="$store.state.showAboutDialog" class="secondary white--text">
    <v-card-title><div class="headline">About</div></v-card-title>
    <v-card-text>
      <p>Welcome to Stellarium Web, a free open source planetarium running in your web browser.
      This page is sill in beta, please report any bugs or ask questions to <a href="mailto:contact@noctua-software.com">contact@noctua-software.com</a> or on our <a href="https://github.com/Stellarium/stellarium-web" target="_blank">Github page</a>.</p>
      <p>If you want to get notified when we add new features (only for important news: no spam!) you can subscribe to our mailing list:</p>
      <p>By subscribing to this mailing list, you agree with our <a v-on:click.stop="$store.state.showPrivacyDialog = true">Privacy Policy</a></p>
      <v-layout row justify-space-around>
        <v-text-field label="Email" v-model='email'/>
        <v-btn color="secondary" @click.native='addEmail()' style="margin-top: auto; margin-bottom: auto;">Send</v-btn>
      </v-layout>
    </v-card-text>
    <v-card-actions>
      <v-spacer></v-spacer><v-btn class="blue--text darken-1" flat @click.native="$store.state.showAboutDialog = false">Close</v-btn>
    </v-card-actions>
  </v-card>
</v-dialog>
</template>

<script>
  export default {
    data: function () {
      return {
        email: ''
      }
    },
    methods: {
      addEmail: function () {
        if (process.browser) {
          this.$store.dispatch('addEmailToMailingList', this.email).then(() => { alert('Your email was added') })
        }
      }
    }
  }
</script>

<style>
a {
  color: #82b1ff;
}
</style>
