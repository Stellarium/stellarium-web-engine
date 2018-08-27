// Stellarium Web - Copyright (c) 2018 - Noctua Software Ltd
//
// This program is licensed under the terms of the GNU AGPL v3, or
// alternatively under a commercial licence.
//
// The terms of the AGPL v3 license can be found in the main directory of this
// repository.

<template>

<v-toolbar dark dense>
  <v-btn icon to="/"><v-icon>close</v-icon></v-btn>
  <v-spacer></v-spacer>
  {{ userFirstName }}
  <v-menu offset-y left v-if="loginEnabled">
    <v-btn icon slot="activator">
      <v-icon>account_circle</v-icon>
    </v-btn>
    <v-list subheader>
      <v-subheader v-if='userLoggedIn'>Logged as {{ userEmail }}</v-subheader>
      <v-list-tile v-if='userLoggedIn' avatar to="/observing/profile">
        <v-list-tile-avatar>
          <v-icon>account_circle</v-icon>
        </v-list-tile-avatar>
        <v-list-tile-content>
          <v-list-tile-title>My Profile</v-list-tile-title>
        </v-list-tile-content>
      </v-list-tile>
      <v-list-tile v-if='userLoggedIn' avatar @click="logout()">
        <v-list-tile-avatar>
          <v-icon>exit_to_app</v-icon>
        </v-list-tile-avatar>
        <v-list-tile-content>
          <v-list-tile-title>Logout</v-list-tile-title>
        </v-list-tile-content>
      </v-list-tile>
      <v-list-tile v-if='!userLoggedIn' avatar to="/observing/signin">
        <v-list-tile-avatar>
          <v-icon>account_box</v-icon>
        </v-list-tile-avatar>
        <v-list-tile-content>
          <v-list-tile-title>Sign In</v-list-tile-title>
        </v-list-tile-content>
      </v-list-tile>
    </v-list>
  </v-menu>
</v-toolbar>

</template>

<script>
import NoctuaSkyClient from '@/assets/noctuasky-client'

export default {
  data: function () {
    return {
      loginEnabled: false
    }
  },
  methods: {
    logout: function () {
      NoctuaSkyClient.users.logout()
    }
  },
  computed: {
    userLoggedIn: function () {
      return this.$store.state.noctuaSky.status === 'loggedIn'
    },
    userFirstName: function () {
      if (!this.loginEnabled) {
        return ''
      }
      return this.$store.state.noctuaSky.user.first_name ? this.$store.state.noctuaSky.user.first_name : 'Anonymous'
    },
    userEmail: function () {
      return this.$store.state.noctuaSky.user.email
    }
  }
}
</script>

<style>
</style>
